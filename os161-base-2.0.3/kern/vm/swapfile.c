#include <types.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <vm.h>
#include <bitmap.h>
#include <swapfile.h>

#define SWAP_DEBUG 1 /* Does extra operations to ensure swap is zeroed and easy to debug */

// TODO le funzioni di swap in e swap out vanno dichiarate dopo aver risolto i dubbi del readME

/* 
 * Node handle in the file system  for the swapfile
 * (see SWAPFILE_PATH for location).
 * It is opened at the initialization of the VM and used thereon.
 */
static struct vnode *swapfile;

/*
 * Uses the builtin bitmap structure to represent efficiently which
 * pages in the swapfile have been occupied and which are not 
 * occupied and may be used to swap out.
 */
static struct bitmap *swapmap;

/* 
 * Opens SWAPFILE from root directory, fills it with zeroes if
 * SWAP_DEBUG is set and then initializes and allocs the bitmap swapmap.
 */
int swap_init(void)
{
    int result;
    char path[32], *zeroes;
    struct uio u;
    struct iovec iov;
    off_t offset;
    const size_t zeroes_size = 1024;

    strcpy(path, SWAPFILE_PATH);
    result = vfs_open(path, O_RDWR | O_CREAT, 0, &swapfile);
    if (result) {
        panic("swapfile.c : Unable to open the swapfile");
    }

#if SWAP_DEBUG
    /* Fill swap file with zeroes to occupy SWAPFILE_SIZE bytes */
    zeroes = (char *) kmalloc(zeroes_size);
    bzero(zeroes, zeroes_size);
    for (offset = 0; offset < SWAPFILE_SIZE; offset += zeroes_size) {
        uio_kinit(&iov, &u, zeroes, zeroes_size, offset, UIO_WRITE);
        result = VOP_WRITE(swapfile, &u);
        if (result) {
            panic("DEBUG ERROR: impossible to zero swap file");
        }
    }
    kfree(zeroes);
#endif

    swapmap = bitmap_create(SWAPFILE_SIZE / PAGE_SIZE);

    return 0;
}

/*
 * Writes in the first free page in the swapfile the content of
 * the memory pointed by argument page_paddr.
 * The offset inside the swapfile where the page has been saved
 * is returned by reference (ret_offset)
 * Update the swapmap as a consequence.
 * Returns 0 on success, panic on some failure.
 */
int swap_out(paddr_t page_paddr, off_t *ret_offset)
{
    unsigned int free_index;
    off_t free_offset;
    struct iovec iov;
    struct uio u;
    int result;

    KASSERT(page_paddr != 0);
    KASSERT((page_paddr & PAGE_FRAME) == page_paddr);

    result = bitmap_alloc(swapmap, &free_index);
    if (result) {
        panic("swapfile.c : Not enough space in swapfile\n");
    }

    free_offset = free_index * PAGE_SIZE;
    KASSERT(free_offset < SWAPFILE_SIZE);

    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, free_offset, UIO_WRITE);
    VOP_WRITE(swapfile, &u);
    if (u.uio_resid != 0) {
        panic("swapfile.c : Could not write to swapfile\n");
    }

    *ret_offset = free_offset;
    return 0;
}

/*
 * Read the content of the swapfile at offset ret_offset into a 
 * pre-allocated memory space pointed by page_paddr.
 * Update the swapmap as a consequence.
 */
int swap_in(paddr_t page_paddr, off_t swap_offset) {
    unsigned int swap_index;
    struct iovec iov;
    struct uio u;

    KASSERT((page_paddr & PAGE_FRAME) == page_paddr);
    KASSERT((swap_offset & PAGE_FRAME) == swap_offset);
    KASSERT(swap_offset < SWAPFILE_SIZE);

    swap_index = swap_offset / PAGE_SIZE;

    if (!bitmap_isset(swapmap, swap_index)) {
        panic("swapfile.c: Trying to access an unpopulated page in swapfile\n");
    }

    uio_kinit(&iov, &u, (void *) PADDR_TO_KVADDR(page_paddr), PAGE_SIZE, swap_offset, UIO_READ);
    VOP_READ(swapfile, &u);
    if (u.uio_resid != 0) {
        panic("swapfile.c : Could not read from swapfile addr %u\n", page_paddr);
    }

    bitmap_unmark(swapmap, swap_index);
    return 0;
}

/* 
 * Discard the page from the swapfile by clearing its bit 
 * in the bitmap. No zero-fill done in this case.
 */
void swap_free(off_t swap_offset) {
    unsigned int swap_index;

    KASSERT((swap_offset & PAGE_FRAME) == swap_offset);
    KASSERT(swap_offset < SWAPFILE_SIZE);

    swap_index = swap_offset / PAGE_SIZE;

    if (!bitmap_isset(swapmap, swap_index)) {
        panic("swapfile.c: Error: freeing an unpopulated page\n");
    }

    /* Leave the use of the page to some other program, no need to zero it */
    bitmap_unmark(swapmap, swap_index);
}