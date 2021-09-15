#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <vnode.h>
#include <proc.h>
#include <uio.h>
#include <coremap.h>
#include <vm.h>
#include <segments.h>

/*
 * Zero a physical memory region
 */
static void zero_a_region(paddr_t paddr, size_t n)
{
    bzero((void *)PADDR_TO_KVADDR(paddr), n);
}

/*
 * Alloc the prog_segment structure and initialize its fields
 */
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

/*
 * It is called by load_elf for each segment that is declared in the ELF headers.
 * For each of them save the parameters that will be useful to alloc a page table
 * and to solve a vm_fault properly.
 */
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

/*
 * Copy the prog_segment structure and its content into a new one.
 */
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

/*
 * Loads a single page into the segment from file, it is called by vm_fault
 * whenever the page table entry corresponding to vaddr is still empty and it
 * must be loaded from file.
 */
int seg_load_page(struct prog_segment *ps, vaddr_t vaddr, paddr_t paddr)
{
    vaddr_t vbaseoffset, voffset;
    paddr_t dest_paddr;
    size_t read_len;
    off_t file_offset;
    struct iovec iov;
    struct uio u;
    int result;
    unsigned long page_index;

    KASSERT(ps != NULL);
    KASSERT(ps->permissions != PAGE_STACK);
    KASSERT(ps->pagetable != NULL);
    KASSERT(ps->elf_vnode != NULL);

    if (ps->file_size > (ps->n_pages) * PAGE_SIZE)
    {
        kprintf("segments.c: warning: segment filesize > segment memsize\n");
        ps->file_size = (ps->n_pages) * PAGE_SIZE;
    }

    DEBUG(DB_EXEC, "segments.c: Loading 1 page to 0x%lx\n", (unsigned long)vaddr);

    page_index = (vaddr - (ps->base_vaddr & PAGE_FRAME)) / PAGE_SIZE;
    KASSERT(page_index < ps->n_pages);
    vbaseoffset = ps->base_vaddr & ~(PAGE_FRAME);
    if (page_index == 0)
    {
        /* First page */
        /*
         * Let's imagine the situation below where fault vaddr is in the first page.
         * "x"s represent the declared virtual address space of the program.
         * Above the "x"s we have physical addresses
         * Below we have virtual addresses
         * 
         *   paddr       (paddr+vbaseoffset)->LOAD HERE!
         *   v           v
         *   |00000000000xxxx|xxxxxxxxxxxxxxx|xxxxx0000000000|
         *               | ^vaddr                              => page_index = 0
         *   |<--------->^(ps->base_vaddr)
         *        ^vbaseoffset
         * 
         * Since we know the paddr that has been assigned to that page, we can
         * compute where to load in the memory. We do this by summing the beginning
         * address of the frame (paddr) to the offset of the first virtual address 
         * of this segment with respect to the virtual address of the beginning 
         * of the page (vbaseoffset). Another way to see vbaseoffset is as the
         * value that base_vaddr would assume if the beginning of the page was at 0.
         * 
         *   |00000000000xxxx|xxxxxxxxxxxxxxx|xxxxx0000000000|
         *   0           | ^vaddr                              => page_index = 0
         *   |           ^(ps->base_vaddr) == vbaseoffset
         * 
         * The memory surrounding the virtual address space should be zeroed so
         * in the case of the first page we zero from the beginning of the frame
         * to the beginning of the actual content of the page, so of a length of
         * vbaseoffset.
         */
        dest_paddr = paddr + vbaseoffset;
        read_len = PAGE_SIZE - vbaseoffset;
        file_offset = ps->file_offset;
        zero_a_region(paddr & PAGE_FRAME, vbaseoffset);
    }
    else if (page_index == (ps->n_pages) - 1)
    {
        /* Last page */
        /* For the approach taken see the first page case above. The memory situation 
         * we suppose here is the following:
         *
         *   paddr                           (paddr+vbaseoffset+voffset)->LOAD HERE!
         *   v                               v
         *   |00000000000xxxx|xxxxxxxxxxxxxxx|xxxxx0000000000|
         *               |                      ^vaddr          => page_index = ps->n_pages-1
         *   |<--------->^(ps->base_vaddr)
         *        ^vbaseoffset               
         *   |           <------------------>|
         *                      ^voffset
         */
        voffset = (ps->n_pages - 1) * PAGE_SIZE - vbaseoffset;
        dest_paddr = paddr + vbaseoffset + voffset;
        read_len = ps->file_size - voffset;
        file_offset = ps->file_offset + voffset;
        zero_a_region((paddr & PAGE_FRAME) + (ps->file_size - voffset), PAGE_SIZE - (ps->file_size - voffset));
    }
    else
    {
        /* Middle page */
        /* For the approach taken see the first page case above. The memory situation 
         * we suppose here is the following:
         *
         *   paddr           (paddr + (PAGE_SIZE * page_index))->LOAD HERE!
         *   v               v
         *   |00000000000xxxx|xxxxxxxxxxxxxxx|xxxxx0000000000|
         *                          ^vaddr                     => page_index = ps->n_pages-1
         * 
         */
        dest_paddr = paddr + (PAGE_SIZE * page_index);
        read_len = PAGE_SIZE;
        file_offset = ps->file_offset + (page_index * PAGE_SIZE) - vbaseoffset;
    }

    /* Sanity check on read parameters */
    KASSERT((dest_paddr - paddr) / PAGE_SIZE < ps->n_pages);
    KASSERT(read_len <= PAGE_SIZE);
    KASSERT(file_offset - ps->file_offset < ps->file_size);

    /* Treat the page as a physical address inside kernel address space and perform a read */
    uio_kinit(&iov, &u, (void *)PADDR_TO_KVADDR(dest_paddr), read_len, file_offset, UIO_READ);
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

/*
 * Try to perform the logical->physical conversion by looking up
 * the page table for the specified segment.
 * Returns the physical address on a successful lookup,
 * PT_UNPOPULATED_PAGE otherwise.
 */
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

/*
 * Add an entry to the page table containing a virtual->physical conversion.
 * It should be called right after a page has been reserved in memory.
 */
void seg_add_pt_entry(struct prog_segment *ps, vaddr_t vaddr, paddr_t paddr)
{
    KASSERT(ps != NULL);
    KASSERT(ps->pagetable != NULL);
    KASSERT(paddr != 0);

    pt_add_entry(ps->pagetable, vaddr, paddr);
}

/*
 * Destroy the segment and its contents
 */
void seg_destroy(struct prog_segment *ps)
{
    KASSERT(ps != NULL);

    if (ps->pagetable != NULL)
    {
        pt_free(ps->pagetable);
    }

    kfree(ps);
}