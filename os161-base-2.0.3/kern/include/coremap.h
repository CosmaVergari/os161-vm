#ifndef _COREMAP_H_
#define _COREMAP_H_

#define COREMAP_BUSY_KERNEL 0
#define COREMAP_BUSY_USER 1
#define COREMAP_UNTRACKED 2
#define COREMAP_FREED 3

#include <addrspace.h>

struct coremap_entry {
    unsigned char entry_type;
    unsigned long allocSize;
    vaddr_t vaddr;          // TODO : controllare se tutti i campi sono realmente necessari
    struct addrspace *as;
};

void coremap_init(void);
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);
paddr_t alloc_upage(vaddr_t vaddr);
void free_upage(paddr_t paddr);

#endif