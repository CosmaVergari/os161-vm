---
title: os161-suchvm
author: Francesco Capano (s284739), Cosma Alex Vergari (s284922)
date: Ciccio
---

Strategia: Partire da come abbiamo implementato le singole classi, parlare dell'integrazione quando descriveremo suchvm.c, aggiornare i flow in alto aggiungendo informazioni che ci siamo ricordati descrivendo le classi.

# Theorical introduction

This is an implementation of the project 1 by G. Cabodi. It implements demand paging, swapping and provides statistics on the performance of the virtual memory manager. It completely replaces DUMBVM.

## On-demand paging
The memory is divided in pages (hence paging) and frames. Pages are indexed by a virtual address and each of them has a physical frame assigned.

The on-demand paging technique creates this correspondence page by page and only when a page is needed by a process. This event is triggered by the TLB.

# os161 process implementation


Per descrivere come os161 runna i processi e dove siamo intervenuti noi. Rimanda ai capitoli di addressed problems.

<<<<<<< HEAD
### Running a user program 
The normal flow of operation of os161 starting a user program is started from the menu where is activated the subsequent chain of calls :
cmd_prog -> common_prog -> proc_create_program ( create user process ) -> thread_fork --(execute)--> cmd_progthread -> runprogramm -> loadelf -> enter new process

The part analized and modified are the ones regarding runprogramm and loadelf. In particular runprogramm is used to open the file, to create and activate the new address space and to load the executable ( through loadelf ). 
Our fixings consist of :
- in the address space activation the whole TLB is invalidated
- the file is not loaded in memory but there is only a set up of the needed structure for the user programm managent
- the file is not closed because it will be needed during the programm execution to load required part from memory


### Flow of runprogramm 
- Passed progname as parameter
- Open the file ( vfs_open)
- create new address space ( as_create )
- switch to it and activate it ( proc_setas, as_activate )
- load the executable ( loadelf )
- close the file ( vfs_close)   // removed because of on demand paging
- define user stack ( as_define_stack)
- warp to user mode calling **enter_new_process**


### Fix during the process loading
- runprogramm does not close the file because of the on demand paging that needs pages to be loaded only when they are needed, so the file has to remain open for the entire process duration
- as_activate invalids the whole TLB because a new process is starting
- loadelf, the segment is not loaded when the load elf is called but only the as for that segment is set up (as_define_region, as_prepare_load and as_complete_load)
- as_define_region ( not called for the stack ) calls seg_define that fill the ps ( prog_segment ) struct with all the needed fields
- as_prepare_load calls seg_prepare that create the page table related to that file segment 
- as_complete_load not used
- returns the entrypoint that will be used to start the process


### Flow of page loading from TLB fault
- TLB miss
- vm_fault in suchvm.c
    - get_as
    - find segment of that as
    - try to get the physical address to the current fault virtual address
    - alloc one page in coremap and save it into the pagetable (if it has already a correspondance in the coremap only write that correspondance in the page table)
        - alloc_upage
        - seg_add_pt_entry or in case of swap seg_swap_in
    - load page from file
        - seg_load_page
    - tlb writing
        - in case tlb is full, is applied roud_robin for replacin



# Implementation
<!-- Qua parliamo di come abbiamo realizzato le funzionalità principali -->
## coremap
The whole memory can be represented as a collection of pages that are in different states. A page can be:
- *untracked*: if the memory manager has not the control over that page yet
- *occupied*: if the memory manager is aware of the page and it has been allocated for a user or kernel process
- *free*: if the memory manager is aware of the page but nobody is using it
os161 by default has a function in `ram.c` called `ram_stealmem()` that returns the physical address of a frame that has never been used before. This form of tracking is not enough for our purposes, so we created an array of structures `struct coremap_entry`, 1 entry for each possible page of memory.

Each entry contains the following information:

```C
/* kern/vm/coremap.h */

struct coremap_entry {
    unsigned char entry_type;
    unsigned long allocSize;
    unsigned long prev_allocated, next_allocated;
    vaddr_t vaddr;
    struct addrspace *as;
};
```
*NOTE: For more details on the data structure or the behaviour of a function or module, please refer to the source file indicated in the code blocks*

`entry_type` is used to keep track of the state of the page and its constants are defined in coremap.h, `allocSize` instead keeps track of how many pages after the current one are allocated.
These 2 fields in all entries can produce a good representation of memory at a given point. And with those we can allocate memory, free it and later reuse some freed pages searched with an appropriate algorithm.

These fields are okay to keep track of *kernel* memory pages, however for *user processes* memory pages we need more information, in particular we added `vaddr` and `as`. `as` is a reference to the `struct addrspace` of the *user* process that has requested this page, while `vaddr` is the virtual address of the beginning of the page.

The array of `struct coremap_entry` is defined in *kern/vm/coremap.c* as a static variable *static struct coremap_entry \*coremap*, and allocated in `coremap_init()` with a length of (number of RAM frames/page size).

But how do we obtain the physical address of the page? If we consider the beginning of the `coremap` array as the address `0x00000000` in memory, and that each `coremap_entry` corresponds to a page of size `PAGE_SIZE` (defined in *kern/arch/mips/include/vm.h* as 4096 bytes), then for the i-th entry the **physical address** will be:

> `paddr = i * PAGE_SIZE`

The other fields will be discussed further in the explanation (see TODO: add reference to swapping).

There are 4 main functions inside the coremap module, that implement what we've talked so far:
* `alloc_kpages()/free_kpages()`: Respectively allocates a number of pages or frees a previously allocated range of pages in the coremap requested by the **kernel**
* `alloc_upage()/free_upage()`: Respectively allocates **one** page or frees a previously allocated page in the coremap requested by a **user process**

The reason why we only allocate 1 page for user processes is intrinsic to the idea of demand-paging, where each single page is requested one at a time from the file and only whenever needed.

## List of files created/modified
- addrspace.c  Francesco
- pt.c Francesco
- vmstats.c Francesco
- coremap.c "Cosma"
- segments.c Cosma
- swapfile.c Cosma
- suchvm.c (delayed)

## Statistics
Describe what are the statistics about and how they are implemented


# Addressed problems
Qua descriviamo i problemi che abbiamo risolto

# Tests
Che cosa fanno i test e che passano.
- stava un test modificato quale ???
- serve un test ceh dia kernel panic perchè lo swapfile è pieno
- ctest
- huge
- matmult
- tictac
- sort
- palin
- faulter
- zero

kernel tests da mettere ? :
- at
- at2
- bt
- tlt
- km1
- km2



# Improvements
Non è stato implementato la capacità multi-processor.
Opzionale
