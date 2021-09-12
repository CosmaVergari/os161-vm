#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <pt.h>
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

    unsigned long page_index = (vaddr - (pt->start_vaddr)) / PAGE_SIZE;
    if (pt->pages[page_index] == PT_UNPOPULATED_PAGE)
    {
        return PT_UNPOPULATED_PAGE;
    }
    else
    {
        return pt->pages[page_index];
    }
}

void pt_free(struct pagetable *pt)
{
    KASSERT(pt != NULL);
    KASSERT(pt->pages != NULL);

    kfree(pt->pages);
    kfree(pt);
}