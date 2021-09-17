#ifndef PT_H
#define PT_H

/* The physical address 0 is reserved to the interrupt vector table so 
 * if a page refers to this address means that it has not been 
 * assigned yet 
 */
#define PT_UNPOPULATED_PAGE 0 

#include <types.h>

struct pagetable {
    unsigned long size;     /* Expressed in pages */
    vaddr_t start_vaddr;
    paddr_t *pages;
};

struct pagetable *pt_create(unsigned long size, vaddr_t start_address);
int pt_copy(struct pagetable *old, struct pagetable **ret);
paddr_t pt_get_entry(struct pagetable *pt, vaddr_t vaddr);
void pt_add_entry(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr);
void pt_free(struct pagetable *pt);
void pt_destroy(struct pagetable *pt);

#endif /* PT_H */