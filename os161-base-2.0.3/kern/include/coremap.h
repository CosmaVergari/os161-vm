#ifndef _COREMAP_H_
#define _COREMAP_H_

void coremap_init(void);
vaddr_t alloc_kpages(unsigned npages);
void free_kpages(vaddr_t addr);

#endif