#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <pt.h>
#include <coremap.h>
#include <vm.h>
#include <swapfile.h>

/* 
 * NOTE: Inside the page table all addresses are page aligned so that
 *       the conversion to index is trivial
 */

/* 
 * Creation of the page table 
 */
struct pagetable *pt_create(unsigned long size_in_pages, vaddr_t start_address)
{
    unsigned long i;

    KASSERT(size_in_pages != 0);
    /*
     * The struct pagetable is allocated
     */
    struct pagetable *pt = kmalloc(sizeof(struct pagetable));

    if (pt == NULL)
    {
        return NULL;
    }

    /*
     * Initialization of all the fields and 
     * allocation of the list of pages
    */
    pt->size = size_in_pages;
    pt->start_vaddr = start_address & PAGE_FRAME;
    pt->pages = kmalloc(size_in_pages * sizeof(paddr_t));
    if (pt->pages == NULL)
    {
        pt->size = 0;
        return NULL;
    }
    /*
     * each page is initialized as unpopulated
     * using the constant PT_UNPOPULATED_PAGE
    */
    for (i = 0; i < size_in_pages; i++)
    {
        pt->pages[i] = PT_UNPOPULATED_PAGE;
    }

    return pt;
}

/*
 * Copy of an old pagetable struct 
 * into a new one to be returne
 */
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

/*
 * From the virtual address the function 
 * returns the correspondence stored in the 
 * page table
 */
paddr_t pt_get_entry(struct pagetable *pt, vaddr_t vaddr)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);
    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr < (pt->start_vaddr) + (pt->size * PAGE_SIZE));

    vaddr = vaddr & PAGE_FRAME;
    /* 
     * Trivial conversion to index
     */
    unsigned long page_index = (vaddr - pt->start_vaddr) / PAGE_SIZE;
    KASSERT(page_index < pt->size);


    if (pt->pages[page_index] == PT_UNPOPULATED_PAGE)
    {
        return PT_UNPOPULATED_PAGE;
    }
    else if ((pt->pages[page_index] & PT_SWAPPED_MASK) == PT_SWAPPED_PAGE)
    {   
        /*
         * The check using PT_SWAPPED_MASK allows to
         * understand if the page has been swapped out
         * previously and so the respective constant is
         * returned
         */   
        return PT_SWAPPED_PAGE;
    }
    else
    {
        return pt->pages[page_index];
    }
}

/*
 * The function allows you to add an entry into the page table
 */
void pt_add_entry(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)
{

    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);
    /*
     * Boundary checks
     */
    KASSERT((vaddr & PAGE_FRAME) >= pt->start_vaddr);
    KASSERT((vaddr & PAGE_FRAME) < (pt->start_vaddr) + (pt->size * PAGE_SIZE));

    /*
     * Computation of the page aligned address
     * and the index in the table
     */
    vaddr = vaddr & PAGE_FRAME;
    unsigned long page_index = (vaddr - pt->start_vaddr) / PAGE_SIZE;
    /*
     * Check if the index is inside the boundaries
     * of the table and the entry stored at that index 
     * is not a valid physical address
     */
    KASSERT(page_index < pt->size);
    KASSERT(pt->pages[page_index] == PT_UNPOPULATED_PAGE ||
            (pt->pages[page_index] & PT_SWAPPED_MASK) == PT_SWAPPED_PAGE);
    pt->pages[page_index] = paddr;
}

/*
 * Free of the pages in the pageteable
 * if the page is not unpopulated then :
 *  - if is a swapped page, it is freed from the swapfile
 *  - if it is saved in memory, free_upage is called  
 */
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
            if ((pt -> pages[i] & PT_SWAPPED_MASK) == PT_SWAPPED_PAGE) {
                swap_free(pt -> pages[i] & (~PT_SWAPPED_MASK));
            } else {
                free_upage(pt->pages[i]);
            }
        }
    }
}


/*
 * The function allows to save the swapfile offset 
 * in the page table entry corresponding to the 
 * passed virtual address and mark that entry as
 * PT_SWAPPED_PAGE
 */
void pt_swap_out(struct pagetable *pt, off_t swapfile_offset, vaddr_t swapped_entry)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);
    /*
     * Boundary checks
     */
    KASSERT(swapped_entry >= pt->start_vaddr);
    KASSERT(swapped_entry < (pt->start_vaddr) + (pt->size * PAGE_SIZE));

    /*
     * The virtual address obtained is aligned
     * to the page 
     */
    swapped_entry = swapped_entry & PAGE_FRAME;
    unsigned long page_index = (swapped_entry - pt->start_vaddr) / PAGE_SIZE;
    
    /*
     * Check if the index is inside the boundaries
     * of the table and the entry stored at that index 
     * is a valid physical address
     */
    KASSERT(page_index < pt->size);
    KASSERT(pt->pages[page_index] != PT_UNPOPULATED_PAGE);
    KASSERT((pt->pages[page_index] & PT_SWAPPED_MASK) != PT_SWAPPED_PAGE);

    /*
     * In the page table is saved the offset in the
     * swapfile concatened with the mask PT_SWAPPED_PAGE
     * that allows to check if the page has been 
     * previously swapped out
     */

    pt->pages[page_index] = ((paddr_t)swapfile_offset) | PT_SWAPPED_PAGE;
}

/*
 * Wrapper to add an entry to the page table
 * for pages previously swapped out
 */
void pt_swap_in(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)
{
    pt_add_entry(pt, vaddr, paddr);
}

/*
 * From a virtual address return the offset  
 * of the swapped page in the swapfile
 */
off_t pt_get_swap_offset(struct pagetable *pt, vaddr_t vaddr)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);
    /*
     * Boundary checks
     */
    KASSERT(vaddr >= pt->start_vaddr);
    KASSERT(vaddr < (pt->start_vaddr) + (pt->size * PAGE_SIZE));

    /*
     * Page aligning and index retrieval
     */
    vaddr = vaddr & PAGE_FRAME;
    unsigned long page_index = (vaddr - pt->start_vaddr) / PAGE_SIZE;
    /*
     * Check if the index is inside the boundaries
     * of the table and the entry stored at that index 
     * is swapped out
     */
    KASSERT(page_index < pt->size);    
    KASSERT((pt->pages[page_index] & PT_SWAPPED_MASK) == PT_SWAPPED_PAGE);

    /*
     * remove the mask from the entry in the page table 
     */
    return (off_t) (pt->pages[page_index] & (~PT_SWAPPED_MASK));
}

/*
 * Free the page table structure
 */
void pt_destroy(struct pagetable *pt)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    kfree(pt->pages);
    kfree(pt);
}