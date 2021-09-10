#ifndef SEGMENTS_H
#define SEGMENTS_H

#define PAGE_RONLY 0
#define PAGE_RW 1

#include <types.h>
#include <pt.h>

// TODO: Aggiungi handle per il file
struct prog_segment
{
    char permissions;
    uint32_t file_size, mem_size;
    uint32_t file_offset;
    vaddr_t base_vaddr;
    struct pagetable *pagetable;
};

struct prog_segment *seg_create(void);
int seg_define(struct prog_segment *ps, vaddr_t base_vaddr, uint32_t mem_size, uint32_t file_size,
               uint32_t file_offset, char read, char write, char execute);
int seg_copy(struct prog_segment *old, struct prog_segment **ret);
void seg_destroy(struct prog_segment *ps);

#endif /* SEGMENTS_H */