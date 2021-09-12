#ifndef SEGMENTS_H
#define SEGMENTS_H

#define PAGE_RONLY 0
#define PAGE_RW 1
#define PAGE_STACK 2

#include <types.h>
#include <pt.h>


struct prog_segment
{
    char permissions;
    size_t file_size;
    off_t file_offset;
    vaddr_t base_vaddr;
    size_t n_pages;
    struct vnode *elf_vnode;
    struct pagetable *pagetable;
};

struct prog_segment *seg_create(void);
int seg_define(struct prog_segment *ps, vaddr_t base_vaddr, size_t file_size, off_t file_offset,
               size_t n_pages, struct vnode *v, char read, char write, char execute);
int seg_define_stack(struct prog_segment *ps, vaddr_t base_vaddr, size_t n_pages);
int seg_prepare(struct prog_segment *ps);
int seg_copy(struct prog_segment *old, struct prog_segment **ret);
paddr_t seg_get_paddr(struct prog_segment *ps, vaddr_t addr);
void seg_destroy(struct prog_segment *ps);

#endif /* SEGMENTS_H */