#ifndef _COREMAP_H_
#define _COREMAP_H_

#define COREMAP_BUSY_KERNEL 0
#define COREMAP_BUSY_USER 1
#define COREMAP_UNTRACKED 2
#define COREMAP_FREED 3

#include <addrspace.h>

/*
 * Structure used to keep track of the state of all memory pages.
 *
 * Each entry contains links to the previous and next allocated
 * entries, so that a linked list is achieved. It is used to
 * implement a FIFO strategy in replacement.
 * 
 * It also contains a reference to the addrspace to which it is
 * currently assigned, this is useful when dealing with pages
 * that do not belong to the current process (e.g. in swapping)
 */
struct coremap_entry {
    unsigned char entry_type;       /* Defines the state of the page */
    unsigned long allocSize;        /* How long the allocation is */

    /* Links to previously and next allocated page (used for replacement) */
    unsigned long prev_allocated, next_allocated;   
    vaddr_t vaddr;                  /* Reference to the virtual page address */
    struct addrspace *as;           /* Reference to the assigned addrspace */
};

void coremap_init(void);
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);
paddr_t alloc_upage(vaddr_t vaddr);
void free_upage(paddr_t paddr);

#endif