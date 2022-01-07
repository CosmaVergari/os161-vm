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

## swapfile
In order to support the swapping operation we created a management class called `swapfile.c`. This file contains most of the logic supporting the I/O from the swap file. The swapfile is located at the root of the filesystem (named SWAPFILE) and it is 9MB in space. *NOTE: These definitions are available and configurable in `swapfile.h`*. The swapfile is itself divided in pages of `PAGE_SIZE` length, just like the RAM memory. This is useful because it eases the management of the swapfile, the tracking of the offset in the swapfile, and the I/O operations. 

In order to manage the swapfile we used a bitmap that allows us to keep track of which swapfile *page* is free and which is filled, so it can't be used for swapping out at a given moment. Note also that the swapfile class is agnostic of which process occupies which page of the swapfile; this information is kept in the appropriate segment, and in particular in its **pagetable** (TODO: Aggiungi link a parte di pagetable in cui si parla dello swap)

Whenever the virtual memory manager starts, the `swap_init()` function is called. This function prepares the swapfile class to perform the following swapping operations. Such preparation consists in opening the swapfile, saving its handle in a global variable `struct vnode *swapfile`, and creating a properly sized bitmap called `swapmap` with `SWAPFILE_SIZE/PAGE_SIZE` entries.

The bitmap and its management methods were already implemented in the system in the `struct bitmap` data type, so we decided to use this structure.

During a `swap_out()` operation:
1. set the first available (zero) bit in the bitmap and retrieve its index
2. initialize the data structures required by a disk write operation (physical address to read from, length to write, offset in the file)
3. perform the disk write operation
4. return the offset in the swapfile where the page has been swapped out (equal to *bitmap_free_index \* PAGE_SIZE*)

Step (4) will be useful for the tracking of the swapped pages inside for a specific process. If operation (1) fails, so when it returns a non-zero value, then it means that there is not enough free space in RAM and in swapfile. At this point the kernel does not have any other way to save the page, so it panics.

The `swap_in()` operation is the symmetrical of the `swap_out()`: given an offset in the swapfile, it reads from the swapfile and resets the bit in the `swapmap`.

Another available operation is the `swap_free()`. This function is used when a process that had some swapped out pages is exiting. Under this circumstances, every page that was swapped out becomes unused, so the `swapmap` has to be updated to make these pages available for other processes. No actual disk operation needs to be performed in this case, since the resetting of the bitmap is enough to make the pages usable by others again.

Every operation on the `swapmap` is performed under the ownership of a lock (`struct spinlock swaplock`) to prevent race condition between processors. This is redundant since for now we are in a single-processor architecture.

Finally, the `swap_shutdown()` is invoked at memory management shutdown and it frees the used resources: close the swapfile handle and free the space used by the `swapmap`.

# suchvm

This is the class where most of the pieces come together, it contains:

1. the VM bootstrap and shutdown procedures executed at boot and poweroff time
2. the **high-level management of a page fault**
3. the implementation of the round-robin **TLB replacement algorithm**

## VM initialization

Let's start with point (1). There are 2 functions that are involved: `vm_bootstrap()` and `vm_shutdown()`.

TODO: Update the links
TODO: Add in shutdown the deallocation of coremap

`vm_bootstrap()` is the VM bootstrap function and it contains the necessary initialization to make the VM work. This consists in the initialization of the [`coremap` class](#coremap), of the [`swap` class](#swapfile), of the [`vmstats` class](#vmstatsTODO) used for statistics and of the [TLB replacement algorithm](#tlb-replacement-algorithm). This function is called by `boot()` in *kern/main/main.c*, that contains the initialization sequence of the kernel.

On the other hand, `vm_shutdown()` is the VM shutdown function and it is the specular of the bootstrap, containing the functions deallocating the resources used by the VM. This function is called by the `shutdown()` function also defined in *kern/main/main.c*, that gets executed when the machine is powering off.

## Management of a page fault

It is implemented in the function `vm_fault()`. This function is directly called by the interrupt handler `mips_trap()` whenever the code of the interrupt assumes one of the following values (defined in *arch/mips/include/trapframe.h*): 

* `EX_MOD` : attempted to write in a read-only page
* `EX_TLBL` : TLB miss on load
* `EX_TLBS` : TLB miss on store

These values have a corresponding *#define* constant in *vm.h*. Respectively: `VM_FAULT_READONLY`, `VM_FAULT_READ`, `VM_FAULT_WRITE` are the possible values that the first parameter of `vm_fault` (`int faulttype`) can assume. This parameter is required because it produces different behaviour in the function.

The `vm_fault()` function closely resembles the theoretical flow of operations that we discussed in the first section ([Flow of page loading from TLB fault](#flow-of-page-loading-from-tlb-fault)). Let's now comment the `vm_fault()` function step by step:

```C
int vm_fault(int faulttype, vaddr_t faultaddress)
{
    unsigned int tlb_index;
    int spl, result;
    char unpopulated;
    paddr_t paddr;
    uint32_t entry_hi, entry_lo;
    struct addrspace *as;
    struct prog_segment *ps;
    vaddr_t page_aligned_faultaddress;

    page_aligned_faultaddress = faultaddress & PAGE_FRAME;

    if (faulttype != VM_FAULT_READONLY &&
        faulttype != VM_FAULT_READ &&
        faulttype != VM_FAULT_WRITE)
    {
        return EINVAL;
    }

    if (faulttype == VM_FAULT_READONLY)
    {
        return EACCES;
    }
```
In this first section we can see the prototype of the function, that accepts the fault type and the `faultaddress` parameters. The second parameter tells which is the **virtual address** inside the currently running process that caused this specific page fault.

We then declare some variables that will be used later on together with a *page-aligned* version of the faulting virtual address. We then check if `faulttype` has some illegal value and return an error in that case. We also check if the fault happened because a write on a read-only page has been performed. If that is the case we return the `EACCES` error, that is defined in *errno.h* as a *Permission Denied* error.

In this latter case we want the calling process to terminate, as required by the project text. This is already done by default when returning an error from `vm_fault()`. To confirm this we can take a look at `mips_trap()` when calls `vm_fault()` and we can see that whenever `vm_fault()` returns a non-zero value (i.e. an error), and we are coming from code running in *user mode*, then the kernel executes a `kill_curthread()` on the thread/process that raised the exception.

```C
    if (curproc == NULL)
    {
        /*
		 * No process. This is probably a kernel fault early
		 * in boot. Return EFAULT so as to panic instead of
		 * getting into an infinite faulting loop.
		 */
        return EFAULT;
    }


    /* Get current running address space structure */
    as = proc_getas();
    if (as == NULL)
    {
        /*
		 * No address space set up. This is probably also a
		 * kernel fault early in boot.
		 */
        return EFAULT;
    }


    ps = as_find_segment(as, faultaddress);
    if (ps == NULL)
    {
        return EFAULT;
    }
```

```C
    /* 
     * Get the physical address of the received virtual address
     * from the page table
     */
    unpopulated = 0;
    paddr = seg_get_paddr(ps, faultaddress);

    if (paddr == PT_UNPOPULATED_PAGE)
    {
        /* 
         * Alloc one page in coremap and add to page table
         */
        paddr = alloc_upage(page_aligned_faultaddress);
        seg_add_pt_entry(ps, faultaddress, paddr);
        if (ps->permissions == PAGE_STACK)
        {
            bzero((void *)PADDR_TO_KVADDR(paddr), PAGE_SIZE);

            vmstats_inc(VMSTAT_PAGE_FAULT_ZERO);
        }
        unpopulated = 1;
    }
    else if (paddr == PT_SWAPPED_PAGE)
    {
        /* 
         * Alloc one page in coremap swap in and update the page table 
         * (inside seg_swap_in())
         */
        paddr = alloc_upage(page_aligned_faultaddress);
        seg_swap_in(ps, faultaddress, paddr);
    } else{
        /* Page already in the memory */
        vmstats_inc(VMSTAT_TLB_RELOAD);
    }

    /* make sure it's page-aligned */
    KASSERT((paddr & PAGE_FRAME) == paddr);

    tlb_index = tlb_get_rr_victim();

    if (ps->permissions != PAGE_STACK && unpopulated)
    {
        /* Load page from file*/
        result = seg_load_page(ps, faultaddress, paddr);
        if (result)
            return EFAULT;
    }

    vmstats_inc(VMSTAT_TLB_FAULT);

    /* Disable interrupts on this CPU while frobbing the TLB. */
    spl = splhigh();

    entry_hi = page_aligned_faultaddress;
    entry_lo = paddr | TLBLO_VALID;
    /* If writes are permitted set the DIRTY bit (see tlb.h) */
    if (ps->permissions == PAGE_RW || ps->permissions == PAGE_STACK)
    {
        entry_lo = entry_lo | TLBLO_DIRTY;
    }
    DEBUG(DB_VM, "suchvm: 0x%x -> 0x%x\n", page_aligned_faultaddress, paddr);


    /*
     * Check added for stats purposes
     * tlb_probe: look for an entry matching the virtual page in ENTRYHI.
     * Returns the index, or a negative number if no matching entry
     * was found. ENTRYLO is not actually used, but must be set; 0
     * should be passed
     * 
     */
    if ( tlb_probe(entry_hi, 0) < 0){
        vmstats_inc(VMSTAT_TLB_FAULT_FREE);
    }else{
        vmstats_inc(VMSTAT_TLB_FAULT_REPLACE);
    }

    tlb_write(entry_hi, entry_lo, tlb_index);
    splx(spl);

    return 0;
}
```

## addrspace
The address space of a program che be represented as a collection of segments, in particular in os161 they are usually three , the code segment, the date segment and the stack segment. From this the decision using the address space structure as a contanier for the three segments structure, as shown below. 

```C
struct addrspace {
    struct prog_segment *seg1;          /* code segment */
    struct prog_segment *seg2;          /* data segment */
    struct prog_segment *seg_stack;     /* stack segment */
};
```
In order to manage the address space are created function to create (`as_create`) and allocate the structure, to copy ( `as_copy`) and destroy (`as_destroy`)the address space. In particular is worth to mention the `as_destroy` function becaude the data structured is destroyed oly when the program has terminated, and so since the we are working in the on demand paging setup there is the need to close the program file with `vfs_close`. 

Strictly correlated are `as_prepare_load` to prepare the segments involved ( call `seg_prepare`) and the function `as_define_region` since ther are involved in the segment initialization and set up. In particular the latter calls `seg_create` and `seg_define` for each segment, passing to the secon the proper number of pages needed for that region. There is also the implementation of `as_define_stack`, that is used for the stack region in an equivalent manner as for the others two regions. In particular since the stack in os161 the stack grows downwards from 0x80000000 (`USERSTACK`) and the base address ( the minimun ) is needed, it is computed in this way :
```C
USERSTACK - (SUCHVM_STACKPAGES * PAGE_SIZE)
```
Then to the stack pointer is assigned the value `USERSTACK`.

Concerning the operational phases, the most important functions are the one to activate the address space at each context switch and the one to locate the proper segments in which the system needs to executes the operations ( such as writing or reading from the page table ). The function `as_activate` is used to activate the address space, and in particular it invalidates all the tlb entries because it is shared among all the processes and so it needs to be cleaned. At the beginning of this function is checked if the current address space is null, in this way when a kernel thread is running there is no tlb invalidation. The function `as_find_segment` is used to retrieve the proper segment inside and address space where the required address is located. It checks that the virtual address passed is inside the boundaries of the current adress space. There are 2 versions of this function, the first called `as_find_segment`, finds the segment by precisely checking if `vaddr` is between the start and the end address (defined as `base_vaddr + mem_size`) of one of the segments, and the second `as_find_segment_coarse` finds the segment by checking if vaddr is between the **page-aligned** virtual address start and end (defined as `base_vaddr + n_pages * PAGE_SIZE`) of one of the segments. The second one is used in the swap out operation since the address requested is the one from the coremap and it is page aligned. The problem arise since the real boundaries of any segment is not page aligned .


## Page table

In order to support the paging operations a proper data structure that allows to have a correspondance between the page number and the physcial address is needed. Our choice is showed below.

```C
struct pagetable {
    unsigned long size;     /* Expressed in pages */
    vaddr_t start_vaddr;    /* Starting virtual address for the conversion*/
    paddr_t *pages;         /* Array of physical address to achieve the conversion */
};
```
In the structure above, `start_vaddr` and `size` are used to check if the page to be added or retrieved is inside the boundaries of the table.
Inside the page table all addresses are page aligned so that the conversion to index is trivial, his is the code to get the page index form the virtual address, it is used to assign and get a page from the table:
```C
    vaddr = vaddr & PAGE_FRAME;
    page_index = (vaddr - pt->start_vaddr) / PAGE_SIZE;
```
At the table creation ( function `struct pagetable *pt_create(unsigned long size, vaddr_t start_address)` )all the pages have not a correspondance to a physical address, so is assigned to them the constant `PT_UNPOPULATED_PAGE`. When a page is swapped out ( function `void pt_swap_out(struct pagetable *pt, off_t swapfile_offset, vaddr_t swapped_entry)`), in the page table is saved the correspondance to the offset in the swapfile concatenated with the constant `PT_SWAPPED_PAGE` in this way :
```C
pt->pages[page_index] = ((paddr_t)swapfile_offset) | PT_SWAPPED_PAGE
```  
In doing so when the swapped page is request is possible to retrieve directly the offset of the swapfile ( function `off_t pt_get_swap_offset(struct pagetable *pt, vaddr_t vaddr)` )and reload that page in memory. To retrieve the offset and to check is a page has been swapped out is used the mask `PT_SWAPPED_MASK` in the following ways :
```C
    /*Page has been swapped out if the condition is true :*/
    (pt -> pages[i] & PT_SWAPPED_MASK) == PT_SWAPPED_PAGE
    /* The offset is retrieved in the following way*/
    off_t offset = (pt->pages[page_index] & (~PT_SWAPPED_MASK));
```

Other basic functions performed at page level by the page table are add and entry ( function `void pt_add_entry(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)`)to the table and get an entry ( `void pt_add_entry(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)`), using the trivial conversion described before. It has been implemented also the function `void pt_swap_in(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)` that is a wraper for the `calls pt_add_entry` function, when the page to be added is a swapped page.

For operation at the table level are used `pt_copy` to copy the entire struct, `pt_destroy` to destroy the struct and `pt_free` to free all the page saved in memory, in case there is a swapped entry, `swap_free` is called to invalidate the entry in the swapfile.

## LoadELF
The function // TODO loadELF

## List of files created/modified
- addrspace.c  Francesco
- pt.c Francesco
- vmstats.c Francesco
- coremap.c "Cosma"
- segments.c Cosma
- swapfile.c Cosma
- suchvm.c (delayed)

TODO dire cosa e' cambiato nella loadelf

## Statistics
In order to compute the stats on page faults and the swap it is used an array of counter, `stats_counts[]`, and for each index is created a constant reported below 
```C
#define VMSTAT_TLB_FAULT              0  
#define VMSTAT_TLB_FAULT_FREE         1
#define VMSTAT_TLB_FAULT_REPLACE      2
#define VMSTAT_TLB_INVALIDATE         3
#define VMSTAT_TLB_RELOAD             4
#define VMSTAT_PAGE_FAULT_ZERO        5
#define VMSTAT_PAGE_FAULT_DISK        6
#define VMSTAT_ELF_FILE_READ          7
#define VMSTAT_SWAP_FILE_READ         8
#define VMSTAT_SWAP_FILE_WRITE        9
```
To access to the stats and modify the array is needed a spinlock (`stats_lock`) to ensure atomicity of the operation, except for the time when the stats are showed by `vmstats_print` at the shutdown when the atomicity is ensured by no process running. In this function, also the following checks are fulfilled :
```
TLB Faults = TLB Faults with Free + TLB Faults with Replace 

TLB Faults = TLB Reloads + Page Faults (Zeroed) + Page Faults (Disk)

ELF File reads + Swapfile reads = Page Faults (Disk) 
```
The function `vmstats_init` is used to initialize the array of counters acquiring the lock and to activate the stats setting `stats_active = 1`. The spinlock `stats_lock`is initialized statically.

To increment the specified counter is used the function `vmstats_inc` to wich is passed the proper index in the array.


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
