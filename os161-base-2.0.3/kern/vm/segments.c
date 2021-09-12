#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <vm.h>
#include <segments.h>


struct prog_segment *seg_create(void)
{
    struct prog_segment *seg = kmalloc(sizeof(struct prog_segment));
    if (seg == NULL)
    {
        return NULL;
    }

    seg->permissions = 0;
    seg->file_size = 0;
    seg->file_offset = 0;
    seg->base_vaddr = 0;
    seg->n_pages = 0;
    seg->elf_vnode = NULL;
    seg->pagetable = NULL;

    return seg;
}

int seg_define(struct prog_segment *ps, vaddr_t base_vaddr, size_t file_size, off_t file_offset,
               size_t n_pages, struct vnode *v, char read, char write, char execute)
{
    KASSERT(ps != NULL);
    /* Ensure that structure is still empty */
    KASSERT(ps->pagetable == NULL);
    KASSERT(ps->base_vaddr = 0);

    /* Sanity checks on variables */
    KASSERT(base_vaddr != 0);
    KASSERT(file_size > 0);
    KASSERT(file_offset > 0);
    KASSERT(n_pages > 0);
    KASSERT(v != NULL);
    KASSERT(!read && !write && !execute);

    /*  Part moved to prepare
     *
     *ps->pagetable = pt_create(n_pages, base_vaddr);
     *if (ps->pagetable == NULL)
     *{
     *    return ENOMEM;
     *}
     */

    ps->base_vaddr = base_vaddr;
    ps->file_size = file_size;
    ps->file_offset = file_offset;
    ps->n_pages = n_pages;
    ps->elf_vnode = v;
    if (write != 0)
    {
        ps->permissions = PAGE_RW;
    }
    else
    {
        ps->permissions = PAGE_RONLY;
    }
    (void)read;
    (void)execute;

    return 0;
}


int seg_define_stack(struct prog_segment *ps, vaddr_t base_vaddr, size_t n_pages)
{
    KASSERT(ps != NULL);
    /* Ensure that structure is still empty */
    KASSERT(ps->pagetable == NULL);
    KASSERT(ps->base_vaddr = 0);

    /* Sanity checks on variables */
    KASSERT(base_vaddr != 0);
    KASSERT(n_pages > 0);
    /*  Part moved to prepare
     *
     *ps->pagetable = pt_create(n_pages, base_vaddr);
     *if (ps->pagetable == NULL)
     *{
     *    return ENOMEM;
     *}
     */

    ps->base_vaddr = base_vaddr;
    ps->file_size = 0;
    ps->file_offset = 0;
    ps->n_pages = n_pages;
    ps->elf_vnode = NULL;
    ps->permissions = PAGE_STACK;

    ps->pagetable = pt_create(n_pages, base_vaddr);
    if (ps->pagetable == NULL)
    {
        return ENOMEM;
    }
    return 0;
}

/*
 *  Function used to alloc the page table of each segment
 *
 *
 */

int seg_prepare(struct prog_segment *ps){
    KASSERT(ps != NULL);
    KASSERT(ps->pagetable == NULL);

    ps->pagetable = pt_create(n_pages, base_vaddr);
    if (ps->pagetable == NULL)
    {
        return ENOMEM;
    }
    return 0;

}

int seg_copy(struct prog_segment *old, struct prog_segment **ret)
{
    struct prog_segment *newps;

    newps = seg_create();
    if (newps == NULL)
    {
        return ENOMEM;
    }

    if (old->pagetable != NULL)
    {
        if (!seg_define(newps, old->base_vaddr, old->file_size, old->file_offset,
                        old->n_pages, old->elf_vnode, 1, old->permissions == PAGE_RONLY ? 0 : 1, 1))
        {
            return ENOMEM;
        }
        pt_copy(old->pagetable, &(newps->pagetable));
    }

    *ret = newps;
    return 0;
}

static void load_page(struct prog_segment *ps, )

paddr_t seg_get_paddr(struct prog_segment *ps, vaddr_t vaddr) {
    paddr_t paddr;

    /* Sanity checks if segment is populated and address is valid */
    KASSERT(ps != NULL);
    KASSERT(ps -> n_pages != 0);
    KASSERT(ps -> pagetable != NULL);
    KASSERT(vaddr > ps -> base_vaddr);
    KASSERT(vaddr < (ps -> base_vaddr) + (ps -> n_pages * PAGE_SIZE));

    /* Get physical address from page table */
    paddr = pt_get_entry(ps ->pagetable, vaddr);
    if (paddr == PT_UNPOPULATED_PAGE) {
        /* 
         * The page must be loaded from DISK and evict another 
         * if there is not enough space (manged by coremap) 
         */
        paddr = alloc_upage(vaddr);

    }
    return paddr;
}

void seg_destroy(struct prog_segment *ps)
{
    KASSERT(ps != NULL);
    
    if (ps->pagetable != NULL)
    {
        kfree(ps->pagetable);
    }

    kfree(ps);
}