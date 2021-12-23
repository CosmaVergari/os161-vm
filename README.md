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
os161 by default has a function in `ram.c` called `ram_stealmem()` that returns the physical address of a frame that has never been used before. This form of tracking is not enough for our purposes, so we created an array of structures `struct coremap_entry`, 1 entry for each existing page of memory.

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

`entry_type` is used to keep track of the state of the page and its possible values (#define constants) are defined in coremap.h, `allocSize` instead keeps track of how many pages after the current one are allocated.
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

## segments
Any user program is divided in different parts that serve different purposes at execution time. These parts are called **segments** and in the case of os161 they are usually the following:
* **code** segment
* **data** segment
* **stack** segment

The code segment contains the actual machine code that will be executed by the processor when the process starts executing. Inside this segment there is a so called **entrypoint** that is the *first* instruction of the program. This segment must be __readonly__ and we will see how to enforce this later on (TODO: link).

The data segment contains the data that is used by the program during its execution. For example the memory space allocated to variables is part of this segment. This segment must be **read-write** to allow variables to be read and written back.

The stack segment is an empty memory space that is used by the process to perform several operations: allocate frames for a function call, variables space, and so on. This segment is empty at process creation time and it is used by the process during its execution.

The code and data segment properties declarations are in the executable ELF file headers, also their initial content is in the ELF file and for this reason they must be loaded from disk at some point in the process execution. The stack segment properties are decided by the kernel implementation instead, and its initial content is all zeroes.

In order to describe a segment we created a specific data structure called `prog_segment`, which is reported below:

```C
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
```

Here is a brief description of each field:
* `permissions` describes what operations can be performed on the segment (R/W/X, STACK) and can assume only one of the constant values declared in the header file, the logic should follow what has been said at the beginning of the chapter
* `file_size` contains the information of how long the segment is in the ELF file
* `file_offset` contains the offset inside the file where the considered segment begins at
* `base_vaddr` is the starting virtual address of the segment, so any access of the declaring process to a virtual address within `base_vaddr` and `base_vaddr + mem_size` will be inside this segment.
* `n_pages` the length of the segment expressed in number of pages
* `mem_size` the length of the segment expressed in memory words
* `elf_vnode` a reference to the ELF file by which this segment was declared
* `pagetable` a pointer to a struct pagetable (TODO: Aggiungi link a classe pagetable) that will be used at address translation (better explanation later on)

TODO: Bisogna fare riferimento a questa sezione quando si parla del caricamento da file?

In order to manage cleanly the creation and destruction of such struct we created 3 appropriate methods called `seg_create()`, `seg_copy()`, `seg_destroy()`, whose behaviour is pretty straightforward. The actual declaration of the properties of the segment is done inside `seg_define()` and `seg_define_stack()` which are called at process creation time. The distinction between the two functions is because code and data segments are loaded from file, while the stack segment is not. The `seg_prepare()` function allocates a page table `n_pages` long to accomodate the address translation later on.

At this point the kernel is aware of all the properties of a segment, however no actual RAM has been allocated. This is a normal behaviour in demand paging because the memory will be occupied only whenever the process actually accesses it, always in a *page granularity*. When a page is loaded in memory we will say that it is *resident* in memory.

In the case of code and data segments, after an access to a page that is not resident in memory, the appropriate page needs to be read from the ELF file. This task is accomplished by the function `seg_load_page()`.

### The seg_load_page() function

The `seg_load_page()` is one of the most important functions in the whole memory management system, and here is how it works. It receives as parameters the virtual address that has caused the page fault `vaddr`, and the starting physical address of the memory frame that has already been allocated to accomodated the page. There are 3 main cases for a page loading:

* The page to be loaded is the first of the `n_pages`
* The page to be loaded is the last of the `n_pages`
* The page to be loaded is in the middle of the virtual address range *(0 < page_index < n_pages-1)*

These cases can be distinguished by checking where `vaddr` falls in the segment declared virtual address range. Depending on the case, the kernel makes a calculation on the 3 parameters required for the following *read* operation that are: the destination physical address adjusted for internal offset, the offset in file to read at, and the length of the data to read from file.
To have a more detailed explanation on how these parameters are calculated, have a look at the implementation of the function in the `segments.c` file.

At this point, the frame is completely zeroed, and the `VOP_READ` operation is triggered, effectively loading from disk to memory.

### Remaining functions
The remaining functions declared in the segments.c file deal with the management of the `pagetable` struct, adding and getting entries from it, but also there are some functions dedicated to the support of the *swapping* operations, but we will see them in more detail while talking about the architecture of *swapping*.


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
