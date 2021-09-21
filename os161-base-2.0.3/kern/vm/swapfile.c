#include <types.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <vfs.h>
#include <uio.h>
#include <swapfile.h>

#define SWAP_DEBUG 1 /* Does extra operations to ensure swap is zeroed and easy to debug */

// TODO le funzioni di swap in e swap out vanno dichiarate dopo aver risolto i dubbi del readME
// TODO la gestione del file

static struct vnode *swapfile;

/* Opens SWAPFILE from root directory */
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

    return 0;
}

int swap_out(paddr_t page_paddr, off_t *ret_offset)
{
    (void) page_paddr;
    (void) ret_offset;
    return 0;
}