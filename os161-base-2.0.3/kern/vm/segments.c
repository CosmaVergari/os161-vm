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
    seg->mem_size = 0;
    seg->file_offset = 0;
    seg->base_vaddr = 0;
    seg->pagetable = NULL;

    return seg;
}

int seg_define(struct prog_segment *ps, vaddr_t base_vaddr, uint32_t mem_size, uint32_t file_size,
               uint32_t file_offset, char read, char write, char execute)
{
    KASSERT(ps != NULL);
    /* Ensure that structure is still empty */
    KASSERT(ps->pagetable == NULL);
    KASSERT(ps->base_vaddr = 0);

    /* Sanity checks on variables */
    KASSERT(base_vaddr != 0);
    KASSERT(file_size > 0);
    KASSERT(mem_size > file_size);
    KASSERT(file_offset != 0);

    ps->pagetable = pt_create(mem_size, base_vaddr);
    if (ps->pagetable == NULL)
    {
        return ENOMEM;
    }

    ps->base_vaddr = base_vaddr;
    ps->mem_size = mem_size;
    ps->file_size = file_size;
    ps->file_offset = file_offset;
    if (write != 0)
    {
        ps->permissions = PAGE_RW;
    }
    else
    {
        ps->permissions = PAGE_RONLY;
    }
    (void) read;
    (void) execute;

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
        if (!seg_define(newps, old->base_vaddr, old->mem_size, old->file_size,
                        old->file_offset, 1, old->permissions == PAGE_RONLY ? 0 : 1, 1))
        {
            return ENOMEM;
        }
        pt_copy(old -> pagetable, &(newps -> pagetable));
    }

    *ret = newps;
    return 0;
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