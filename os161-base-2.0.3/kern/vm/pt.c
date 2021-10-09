#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <pt.h>
#include <coremap.h>
#include <vm.h>

struct pagetable *pt_create(unsigned long size_in_pages, vaddr_t start_address)
{
    unsigned long i;

    KASSERT(size_in_pages != 0);

    struct pagetable *pt = kmalloc(sizeof(struct pagetable));

    if (pt == NULL)
    {
        return NULL;
    }

    pt->size = size_in_pages;
    pt->start_vaddr = start_address;
    pt->pages = kmalloc(size_in_pages * sizeof(paddr_t));
    if (pt->pages == NULL)
    {
        pt->size = 0;
        return NULL;
    }
    for (i = 0; i < size_in_pages; i++)
    {
        pt->pages[i] = PT_UNPOPULATED_PAGE;
    }

    return pt;
}

int pt_copy(struct pagetable *old, struct pagetable **ret)
{
    struct pagetable *newpt;
    unsigned long i;

    KASSERT(old != NULL);
    KASSERT(old->size != 0);
    KASSERT(old->start_vaddr != 0);
    KASSERT(old->pages != NULL);

    newpt = pt_create(old->size, old->start_vaddr);
    if (newpt == NULL)
    {
        return ENOMEM;
    }

    newpt->size = old->size;
    newpt->start_vaddr = old->start_vaddr;
    for (i = 0; i < old->size; i++)
    {
        newpt->pages[i] = old->pages[i];
    }

    *ret = newpt;
    return 0;
}

paddr_t pt_get_entry(struct pagetable *pt, vaddr_t vaddr)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);
    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr < (pt->start_vaddr) + (pt->size * PAGE_SIZE));

    /* 
     * When vaddr is at the beginning of a frame and start_vaddr is not page aligned
     * we need to return the physical address of the frame where start_vaddr is and not
     * the physical address of the previous frame (which can happen if we do 
     * vaddr - start_vaddr without aligning to PAGE_FRAME). See schematic below.
     * |    xxx|xxxxxx|xxx     |
     *      lstart_vaddr lvaddr   => i=2 
     */
    unsigned long page_index = (vaddr - (pt->start_vaddr & PAGE_FRAME)) / PAGE_SIZE;
    if (pt->pages[page_index] == PT_UNPOPULATED_PAGE)
    {
        return PT_UNPOPULATED_PAGE;
    }
    else if ((pt->pages[page_index] & PT_SWAPPED_MASK) == PT_SWAPPED_PAGE)
    {
        return PT_SWAPPED_PAGE;
    }
    else
    {
        return pt->pages[page_index];
    }
}

void pt_add_entry(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);
    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr < (pt->start_vaddr) + (pt->size * PAGE_SIZE));

    /* See pt_get_entry() for more information */
    unsigned long page_index = (vaddr - (pt->start_vaddr & PAGE_FRAME)) / PAGE_SIZE;
    KASSERT(pt->pages[page_index] == PT_UNPOPULATED_PAGE ||
            (pt->pages[page_index] & PT_SWAPPED_MASK) == PT_SWAPPED_PAGE);
    pt->pages[page_index] = paddr;
}

void pt_free(struct pagetable *pt)
{
    unsigned long i;

    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);
    KASSERT(pt->size != 0);

    for (i = 0; i < pt->size; i++)
    {
        if (pt->pages[i] != PT_UNPOPULATED_PAGE)
        {
            free_upage(pt->pages[i]);
        }
    }
}

void pt_swap_out(struct pagetable *pt, off_t swapfile_offset, vaddr_t swapped_entry)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);
    KASSERT(swapped_entry >= pt->start_vaddr);
    KASSERT(swapped_entry < (pt->start_vaddr) + (pt->size * PAGE_SIZE));

    /* See pt_get_entry() for more information */
    unsigned long page_index = (swapped_entry - (pt->start_vaddr & PAGE_FRAME)) / PAGE_SIZE;
    KASSERT(pt->pages[page_index] != PT_UNPOPULATED_PAGE);
    KASSERT((pt->pages[page_index] & PT_SWAPPED_MASK) != PT_SWAPPED_PAGE);
    pt->pages[page_index] = ((paddr_t)swapfile_offset) | PT_SWAPPED_PAGE;
}

void pt_swap_in(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)
{
    pt_add_entry(pt, vaddr, paddr);
}

off_t pt_get_swap_offset(struct pagetable *pt, vaddr_t vaddr)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);
    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr < (pt->start_vaddr) + (pt->size * PAGE_SIZE));

    unsigned long page_index = (vaddr - (pt->start_vaddr & PAGE_FRAME)) / PAGE_SIZE;
    
    KASSERT((pt->pages[page_index] & PT_SWAPPED_MASK) == PT_SWAPPED_PAGE);

    return (off_t) (pt->pages[page_index] & (~PT_SWAPPED_MASK));
}

void pt_destroy(struct pagetable *pt)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    kfree(pt->pages);
    kfree(pt);
}