#ifndef PT_H
#define PT_H

/* The physical address 0 is reserved to the interrupt vector table so 
 * if a page refers to this address means that it has not been 
 * assigned yet 
 */
#define PT_UNPOPULATED_PAGE 0 

/*
 * The address 1 is not a valid address and it is used as a flag
 * in the last bit of a page table entry to signal that such page
 * has been swapped out. If it is set, the higher bits contain the
 * offset of the page inside the swapfile.
 */
#define PT_SWAPPED_PAGE 1
#define PT_SWAPPED_MASK 0x00000001

#include <types.h>

struct pagetable {
    unsigned long size;     /* Expressed in pages */
    vaddr_t start_vaddr;
    paddr_t *pages;
};

/* See pt.c for a general description on the following functions */

struct pagetable *pt_create(unsigned long size, vaddr_t start_address);
int pt_copy(struct pagetable *old, struct pagetable **ret);
paddr_t pt_get_entry(struct pagetable *pt, vaddr_t vaddr);
void pt_add_entry(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr);
void pt_free(struct pagetable *pt);
void pt_swap_out(struct pagetable *pt, off_t swapfile_offset, vaddr_t swapped_entry);
void pt_swap_in(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr);
off_t pt_get_swap_offset(struct pagetable *pt, vaddr_t vaddr);
void pt_destroy(struct pagetable *pt);

#endif /* PT_H */