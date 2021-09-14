#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <vnode.h>
#include <proc.h>
#include <uio.h>
#include <coremap.h>
#include <vm.h>
#include <segments.h>

static void zero_a_region(paddr_t paddr, size_t n)
{
    /* static, so initialized as zero */
    static char zeros[16];
    size_t amt, i;

    if (n == 0)
        return;

    KASSERT(paddr != 0);

    i = 0;
    while (n > 0)
    {
        amt = sizeof(zeros);
        if (amt > n)
        {
            amt = n;
        }
        memcpy((void *)paddr + (i * sizeof(zeros)), (void *)zeros, amt);
        n -= amt;
        i++;
    }
}

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
    KASSERT(ps->base_vaddr == 0);

    /* Sanity checks on variables */
    KASSERT(base_vaddr != 0);
    KASSERT(file_size > 0);
    KASSERT(n_pages > 0);
    KASSERT(v != NULL);
    KASSERT(read || write || execute);

    ps->base_vaddr = base_vaddr;
    ps->file_size = file_size;
    ps->file_offset = file_offset;
    ps->n_pages = n_pages;
    ps->elf_vnode = v;
    if (write)
    {
        ps->permissions = PAGE_RW;
    }
    else
    {
        if (execute)
            ps->permissions = PAGE_EX;
        else
            ps->permissions = PAGE_RONLY;
    }
    (void)read;

    return 0;
}

int seg_define_stack(struct prog_segment *ps, vaddr_t base_vaddr, size_t n_pages)
{
    KASSERT(ps != NULL);
    /* Ensure that structure is still empty */
    KASSERT(ps->pagetable == NULL);
    KASSERT(ps->base_vaddr == 0);

    /* Sanity checks on variables */
    KASSERT(base_vaddr != 0);
    KASSERT(n_pages > 0);

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
 */

int seg_prepare(struct prog_segment *ps)
{
    KASSERT(ps != NULL);
    KASSERT(ps->pagetable == NULL);

    ps->pagetable = pt_create(ps->n_pages, ps->base_vaddr);
    if (ps->pagetable == NULL)
    {
        return ENOMEM;
    }
    return 0;
}

int seg_copy(struct prog_segment *old, struct prog_segment **ret)
{
    struct prog_segment *newps;

    KASSERT(old != NULL);
    KASSERT(old->pagetable != NULL);

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

int seg_load_page(struct prog_segment *ps, vaddr_t vaddr, paddr_t paddr)
{
    vaddr_t voffset, vbaseoffset;
    struct iovec iov;
    struct uio u;
    int result;
    unsigned long page_index;
    struct addrspace *as;

    KASSERT(ps != NULL);
    KASSERT(ps ->permissions != PAGE_STACK);
    KASSERT(ps->pagetable != NULL);
    KASSERT(ps->elf_vnode != NULL);

    as = proc_getas();

    if (ps->file_size > (ps->n_pages) * PAGE_SIZE)
    {
        kprintf("segments.c: warning: segment filesize > segment memsize\n");
        ps->file_size = (ps->n_pages) * PAGE_SIZE;
    }

    DEBUG(DB_EXEC, "segments.c: Loading 1 page to 0x%lx\n", (unsigned long)vaddr);

    u.uio_iov = &iov;
    u.uio_iovcnt = 1;
    u.uio_segflg = ps->permissions == PAGE_EX ? UIO_USERISPACE : UIO_USERSPACE;
    u.uio_rw = UIO_READ;
    u.uio_space = as;

    //|    XXXX|xxxxxx|XXXX    |
    //     | lvaddr      lvaddr2
    //|    base_vaddr
    //|

    //|    YYYY|xxxxxx|XXXX     |
    //|           lvaddr
    //|        |

    /* TODO: Explanation of code below */

    page_index = (vaddr - (ps->base_vaddr & PAGE_FRAME)) / PAGE_SIZE;
    KASSERT(page_index < ps->n_pages);
    vbaseoffset = ps->base_vaddr & ~(PAGE_FRAME);
    if (page_index == 0)
    {
        /* First page */
        voffset = (size_t)(ps->base_vaddr) & ~(PAGE_FRAME);
        iov.iov_ubase = (userptr_t)ps -> base_vaddr;
        iov.iov_len = PAGE_SIZE - voffset; /* Size in memory */
        u.uio_resid = PAGE_SIZE - voffset; /* amount to read from file */
        u.uio_offset = ps->file_offset;    /* Offset in file */
        zero_a_region(paddr & PAGE_FRAME, voffset);
    }
    else if (page_index == (ps->n_pages) - 1)
    {
        /* Last page */
        voffset = (size_t)((ps->n_pages - 1) * PAGE_SIZE) - vbaseoffset;
        iov.iov_ubase = (userptr_t)((ps -> base_vaddr) + voffset);
        iov.iov_len = ps->file_size - voffset;
        u.uio_resid = ps->file_size - voffset;
        u.uio_offset = ps->file_offset + (off_t)voffset;
        zero_a_region((paddr & PAGE_FRAME) + (ps->file_size - voffset), PAGE_SIZE - (ps->file_size - voffset));
    }
    else
    {
        /* Middle page */
        iov.iov_ubase = (userptr_t)(vaddr & PAGE_FRAME);
        iov.iov_len = PAGE_SIZE;
        u.uio_resid = PAGE_SIZE;
        u.uio_offset = ps->file_offset + (off_t)((page_index * PAGE_SIZE) - vbaseoffset);
    }

    result = VOP_READ(ps->elf_vnode, &u);
    if (result)
    {
        return result;
    }

    if (u.uio_resid != 0)
    {
        /* short read; problem with executable? */
        kprintf("segments.c: short read on segment - file truncated?\n");
        return ENOEXEC;
    }
    return 0;
}

paddr_t seg_get_paddr(struct prog_segment *ps, vaddr_t vaddr)
{
    paddr_t paddr;

    /* Sanity checks if segment is populated and address is valid */
    KASSERT(ps != NULL);
    KASSERT(ps->n_pages != 0);
    KASSERT(ps->pagetable != NULL);
    KASSERT(vaddr >= ps->base_vaddr);
    KASSERT(vaddr < (ps->base_vaddr) + (ps->n_pages * PAGE_SIZE));

    /* Get physical address from page table */
    paddr = pt_get_entry(ps->pagetable, vaddr);
    return paddr;
}

void seg_add_pt_entry(struct prog_segment *ps, vaddr_t vaddr, paddr_t paddr) {
    KASSERT(ps != NULL);
    KASSERT(ps -> pagetable != NULL);
    KASSERT(paddr != 0);

    pt_add_entry(ps->pagetable, vaddr, paddr);
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