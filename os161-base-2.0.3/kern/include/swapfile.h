#ifndef SWAPFILE_H
#define SWAPFILE_H

#define SWAPFILE_SIZE 9 * 1024 * 1024
#define SWAPFILE_PATH "emu0:/SWAPFILE"

int swap_init(void);
int swap_out(paddr_t page_paddr, off_t *ret_offset);
int swap_in(paddr_t page_paddr, off_t swap_offset);
void swap_free(off_t swap_offset);
void swap_shutdown(void);

#endif /* SWAPFILE_H */