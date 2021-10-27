#ifndef SEGMENTS_H
#define SEGMENTS_H

#define PAGE_RONLY 0
#define PAGE_RW 1
#define PAGE_EX 2
#define PAGE_STACK 3


#include <types.h>
#include <pt.h>


struct prog_segment
{
    char permissions;
    size_t file_size;
    off_t file_offset;
    vaddr_t base_vaddr;
    size_t n_pages;
    size_t mem_size;
    struct vnode *elf_vnode;
    struct pagetable *pagetable;
};

/*
 * Represent an ELF segment and the information needed for suchvm
 *
 * Defined functions:
 *    seg_create        - Create an empty segment object
 *    seg_copy          - Copy a segment from an old one to a newly allocated one
 *    seg_destroy       - Destroy the segment and its contents (called on program exit
 *    seg_define        - It is called by load_elf for each segment that is declared in the ELF headers.
 *                        For each of them save the parameters that will be useful to alloc a page table
 *                        and to solve a vm_fault properly.
 *    seg_define_stack  - Same as seg_define but it is specific for the stack segment
 *    seg_prepare       - Function used to alloc the page table of each segment.
 *                        Must be called after a seg_define or seg_define_stack
 *    seg_load_page     - Loads a single page into the segment from file, it is called by vm_fault
 *                        whenever the page table entry corresponding to vaddr is still empty and it
 *                        must be loaded from file.
 *                        See its implementation for more details on how it is handled.
 *    seg_get_paddr     - Try to perform the logical->physical conversion by looking up the page table for 
 *                        the specified segment.
 *                        Returns the physical address on a successful lookup, PT_UNPOPULATED_PAGE otherwise.
 *    seg_add_pt_entry  - Add an entry to the page table containing a virtual->physical conversion.
 *                        It should be called right after a page has been reserved in memory.
 *    seg_swap_out      - Save somewhere in the segment that a page has been swapped out and where (as an
 *                        offset in the swapfile). See pt_swap_out() for more information.
 *    seg_swap_in       - Retrieve the saved swapfile offset and swap in the desired page in the given
 *                        physical address and at a specific virtual address. Save the result in the
 *                        page table
 */
struct prog_segment *seg_create(void);
int seg_define(struct prog_segment *ps, vaddr_t base_vaddr, size_t file_size, off_t file_offset, size_t mem_size,
               size_t n_pages, struct vnode *v, char read, char write, char execute);
int seg_define_stack(struct prog_segment *ps, vaddr_t base_vaddr, size_t n_pages);
int seg_prepare(struct prog_segment *ps);
int seg_copy(struct prog_segment *old, struct prog_segment **ret);

paddr_t seg_get_paddr(struct prog_segment *ps, vaddr_t addr);
void seg_add_pt_entry(struct prog_segment *ps, vaddr_t vaddr, paddr_t paddr);
int seg_load_page(struct prog_segment *ps, vaddr_t vaddr, paddr_t paddr);

void seg_swap_out(struct prog_segment *ps, off_t swapfile_offset, vaddr_t swapped_entry);
void seg_swap_in(struct prog_segment *ps, vaddr_t vaddr, paddr_t paddr);

void seg_destroy(struct prog_segment *ps);

#endif /* SEGMENTS_H */