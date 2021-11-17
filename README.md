---
title: os161-suchvm
author: Francesco Capano (s284739), Cosma Alex Vergari (s284922)
date: Ciccio
---

Strategia: spiegare approfonditamente come funziona il demand paging e swapping e mostrando quale componente implementa quella funzionalità. 
Se c'è da precisare qualcosa su un componente si fa in un capitolo a parte.

# Theorical introduction

This is an implementation of the project 1 by G. Cabodi. It implements demand paging, swapping and provides statistics on the performance of the virtual memory manager. It completely replaces DUMBVM.

## On-demand paging
The memory is divided in pages (hence paging) and frames. Pages are indexed by a virtual address and each of them has a physical frame assigned.

The on-demand paging technique creates this correspondence page by page and only when a page is needed by a process. This event is triggered by the TLB.

# os161 process implementation


Per descrivere come os161 runna i processi e dove siamo intervenuti noi. Rimanda ai capitoli di addressed problems.


























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
- addrspace.c
- coremap.c
- pagetable.c
- segments.c
- suchvm.c
- swapfile.c


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
