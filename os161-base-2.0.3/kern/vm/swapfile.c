#include <types.h>
#include <kern/fcntl.h>
#include <vnode.h>
#include <vfs.h>
#include <swapfile.h>

// TODO le funzioni di swap in e swap out vanno dichiarate dopo aver risolto i dubbi del readME
// TODO la gestione del file

static struct vnode *swapfile;

/* TODO: Create SWAPFILE in root directory */
int swap_init(void)
{
    int result;
    char path[32];

    strcpy(path, SWAPFILE_PATH);
    result = vfs_open(path, O_RDWR | O_CREAT | O_TRUNC, 0, &swapfile);
    if (result) {
        panic("swapfile.c : Unable to open the swapfile");
    }

    return 0;
}

int swap_out(paddr_t page_paddr, off_t *ret_offset)
{
    (void) page_paddr;
    (void) ret_offset;
    return 0;
}