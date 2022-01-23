---
title: os161-suchvm
author: Francesco Capano (s284739), Cosma Alex Vergari (s284922)
date: Ciccio
---

# Theorical introduction

This is an implementation of the project 1 by G. Cabodi. It implements *demand paging*, *swapping* and provides *statistics* on the performance of the virtual memory manager. It completely replaces DUMBVM.

## On-demand paging

The memory is divided in pages (hence paging) and frames. They lie in separate address spaces and for this reason they are indexed with different addresses :

- **virtual addresses** for pages
- **physical addresses** for frames

A page is the atomic unit managed by the virtual memory manager, on the other hand a frame is the smallest unit of physical memory. Whenever a page is **needed** by a process ( a logical address corresponding to that page has been accessed ), a correspondence between these two units is instantiated by the virtual memory manager.

This event is triggered by the TLB and uses information about the current state of the memory and structures like page tables and swapfiles to accomplish the address translation.

The whole concept of **on-demand paging** allows to avoid un-necessary I\O operations and a more efficient usage of memory. In this frame of reference, we implemented a ***lazy swapper***.

Our virtual memory manager is called *SuchVM*.

# os161 process implementation

### Running a user program

The normal flow of operation of os161 starting a user program begins from the menu where the following chain of calls is performed:

- `cmd_prog`
- `common_prog`
- `proc_create_program` ( create user process )
- `thread_fork`
- `cmd_progthread`
- `runprogram`
- `loadelf`
- `enter_new_process`

The parts analized and modified are the ones regarding `runprogram`,`loadelf` and the TLB invalidation process in the context switch. The latter is performed in `as_activate` (see [addrspace](#addrspace) section).

### runprogram

In the `runprogram` function, the program file is opened using `vfs_open`, and then a new address space is created and activated ( using `as_create` and `as_activate`) (see [addrspace](#addrspace) section). The process segments are defined by the `load_elf` function and the user stack is defined through `as_define_stack`, then the new process is started ( `enter_new_process` ). The major change to this function has been leaving the ELF program file open during the whole execution of the program. So there is no call to `vfs_close`, which instead will be executed only when the program terminates its execution (in `as_destroy`).

### loadelf

The `load_elf` function was previously used to load the entire file in the memory; but following the policy of the on demand paging there is no need to do that. Our solution does not use the function `load_segment` in the loadelf.c file to load the segments in memory but simply reads the executable header and define the regions of the address space using `as_define_region` and prepare it through `as_prepare_load` (see [addrspace](#addrspace) and [segments](#segments) sections). The function returns the entrypoint that will be used to start the process.

### Flow of page loading from TLB fault

When there is a TLB miss, this event is managed by `vm_fault` ( in [suchvm.c](#suchvm)). In `vm_fault` each time a new correspondance virtual to physcal address is saved in the TLB using a round robin algorithm to select the victim.
During the whole process, the event of saving a new page in memory is managed by the coremap (see [coremap](#coremap) section) using `alloc_upage` function.

In the `vm_fault` function the flow is the following :

- The current address space structure is retrieved and then the segment associated to the fault address ( `proc_getas` , `as_find_segment`)( see [addrspace](#addrspace) section)
- There is the attempt to get physical address of the current fault address (`seg_get_paddr`)(see [segments](#segments) section)):
  - If there is no correspondance, a new page is loaded from the file and allocated in memory (`seg_load_page`)
  - If that page has been swapped out previously,there is the swap-in from the swap file and the page is allocated in the main memory (`seg_swap_in`)
- At this point if in the page table there was not present an entry, a new one is created (`seg_add_pt_entry`)

# Implementation

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

`entry_type` is used to keep track of the state of the page and its possible values ( `#define constants`) are defined in coremap.h. `allocSize` instead keeps track of how many pages after the current one are allocated.
These 2 fields in all entries can produce a good representation of memory at a given point. And with those we can allocate memory, free it and later reuse some freed pages searched with an appropriate algorithm.

These fields are enough to keep track of *kernel* memory pages, however for *user processes* memory pages we need more information. In particular we added `vaddr` and `as`. `as` is a reference to the `struct addrspace` of the *user* process that has requested this page, while `vaddr` is the virtual address of the beginning of the page.

The array of `struct coremap_entry` is defined in *kern/vm/coremap.c* as a static variable *static struct coremap_entry \*coremap*, and allocated in `coremap_init()` with a length of *(number of RAM frames/page size)*.

But how do we obtain the physical address of the page? If we consider the beginning of the `coremap` array as the address `0x00000000` in memory, and that each `coremap_entry` corresponds to a page of size `PAGE_SIZE` (defined in *kern/arch/mips/include/vm.h* as 4096 bytes), then for the i-th entry the **physical address** will be:

```C
    paddr = i * PAGE_SIZE
```
The other fields will be discussed further in the explanation (see [swapping](#swapfile)).

There are 4 main functions inside the coremap module, that implement what we have discussed so far:

- `alloc_kpages()/free_kpages()`: Respectively allocates a number of pages or frees a previously allocated range of pages in the coremap requested by the **kernel**
- `alloc_upage()/free_upage()`: Respectively allocates **one** page or frees a previously allocated page in the coremap requested by a **user process**

The reason why we only allocate 1 page for user processes is intrinsic to the idea of demand-paging, where each single page is requested one at a time from the file and only whenever needed.

## segments

Any user program is divided in different parts that serve different purposes at execution time. These parts are called **segments** and in the case of os161 they are usually the following:

- **code** segment
- **data** segment
- **stack** segment

The code segment contains the actual machine code that will be executed by the processor when the process starts executing. Inside this segment there is a so called **entrypoint** that is the *first* instruction of the program. This segment must be **readonly** and we will see how to enforce this later on ( [suchvm](#suchvm)).

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

- `permissions` describes what operations can be performed on the segment (R/W/X, STACK) and can assume only one of the constant values declared in the header file, the logic should follow what has been said at the beginning of the chapter
- `file_size` contains the information of how long the segment is in the ELF file
- `file_offset` contains the offset inside the file where the considered segment begins at
- `base_vaddr` is the starting virtual address of the segment, so any access of the declaring process to a virtual address within `base_vaddr` and `base_vaddr + mem_size` will be inside this segment
- `n_pages` the length of the segment expressed in number of pages
- `mem_size` the length of the segment expressed in memory words
- `elf_vnode` a reference to the ELF file by which this segment was declared
- `pagetable` a pointer to a struct pagetable (see [pagetable](#page-table)) that will be used at address translation (better explanation later on)

In order to manage cleanly the creation and destruction of such struct we created 3 appropriate methods called `seg_create()`, `seg_copy()`, `seg_destroy()`, whose behaviour is pretty straightforward. The actual declaration of the properties of the segment is done inside `seg_define()` and `seg_define_stack()` which are called at process creation time. The distinction between the two functions is because code and data segments are loaded from file, while the stack segment is not. The `seg_prepare()` function allocates a page table `n_pages` long to accomodate the address translation later on.

At this point the kernel is aware of all the properties of a segment, however no actual RAM has been allocated. This is a normal behaviour in demand paging because the memory will be occupied only whenever the process actually accesses it, always in a *page granularity*. When a page is loaded in memory we will say that it is *resident* in memory.

In the case of code and data segments, after an access to a page that is not resident in memory, the appropriate page needs to be read from the ELF file. This task is accomplished by the function `seg_load_page()`.

### The seg_load_page() function

The `seg_load_page()` is one of the most important functions in the whole memory management system, and here is how it works. It receives as parameters the virtual address that has caused the page fault `vaddr`, and the starting physical address of the memory frame that has already been allocated to accomodated the page. There are 3 main cases for a page loading:

- The page to be loaded is the first of the `n_pages`
- The page to be loaded is the last of the `n_pages`
- The page to be loaded is in the middle of the virtual address range *(0 < page_index < n_pages-1)*

These cases can be distinguished by checking where `vaddr` falls in the segment declared virtual address range. Depending on the case, the kernel makes a calculation on the 3 parameters required for the following *read* operation that are:
- the destination physical address adjusted for internal offset
- the offset in file to read at 
-  the length of the data to read from file

To have a more detailed explanation on how these parameters are calculated, have a look at the implementation of the function in the `segments.c` file.

At this point, the frame is completely zeroed, and the `VOP_READ` operation is triggered, effectively loading from disk to memory.

### Remaining functions

The remaining functions declared in the segments.c file deal with the management of the `pagetable` struct (see [pagetable](#page-table)), adding and getting entries from it, but also there are some functions dedicated to the support of the *swapping* operations (more details in the [swapping](#swapfile) section)


## addrspace

The address space of a program can be represented as a collection of segments, in particular in os161 they are usually three: the *code segment*, the *data segment* and the *stack segment*. So we decided to use the address space structure as a contanier for the three segments structure, as shown below.

```C
struct addrspace {
    struct prog_segment *seg1;          /* code segment */
    struct prog_segment *seg2;          /* data segment */
    struct prog_segment *seg_stack;     /* stack segment */
};
```

In order to manage the address space we created functions: to create (`as_create`) and allocate the structure, to copy ( `as_copy`) and destroy (`as_destroy`) the address space. In particular is worth mentioning the `as_destroy` function because the data structure is destroyed only when the program has terminated, and so, since we are working in the on-demand paging setup, there is the need to close the program file with `vfs_close`, that we remind it was left open to load pages from it whenever needed.

Strictly correlated are: the function `as_prepare_load` that prepares the declared segments to the load (calls `seg_prepare`) and the function `as_define_region` since there are involved in the segment initialization and set up. In particular the latter computes the number of pages needed for the region and calls `seg_create` and `seg_define` for each segment. There is also the implementation of `as_define_stack`, that is used for the stack region in an equivalent manner as for the others two regions.

Since the stack in os161 grows downwards from 0x80000000 (`USERSTACK` constant) and the base address (the lowest address of stack segment) is needed, it is computed in this way :

```C
USERSTACK - (SUCHVM_STACKPAGES * PAGE_SIZE)
```

Then the stack pointer is assigned the value `USERSTACK`.

Concerning the operational phases, the most important functions are the ones that **activate the address space** at each context switch and the one to **locate the proper segments** in which the system needs to executes the operations ( such as writing or reading from the page table ).

The function `as_activate` is used to activate the address space, and in particular it invalidates all the tlb entries. This is necessary because the TLB is shared among processes and its entries do not contain any reference to the running process. The function checks at the beginning if the current address space is `NULL`. This means that it has been called by a kernel thread and no tlb invalidation is performed.

The function `as_find_segment` is used to retrieve the proper segment inside the current address space where the required address is located. It checks that the virtual address passed is inside the boundaries of the current adress space. There are 2 versions of this function, the first called `as_find_segment`, finds the segment by precisely checking if `vaddr` is between the start and the end address (defined as `base_vaddr + mem_size`) of one of the segments, and the second `as_find_segment_coarse` finds the segment by checking if vaddr is between the **page-aligned** virtual address start and end (defined as `base_vaddr + n_pages * PAGE_SIZE`) of one of the segments.

The second one is used in the swap out operation because the requested address comes from the coremap and so it is page aligned. The problem arises because the actual boundaries of any segment may not be page aligned.

## Page table

In order to support the paging operations a proper data structure that allows to have a correspondance between the page number and the physcial address is needed. Our choice is showed below.

```C
struct pagetable {
    unsigned long size;     /* Expressed in pages */
    vaddr_t start_vaddr;    /* Starting virtual address for the conversion*/
    paddr_t *pages;         /* Array of physical address to achieve the conversion */
};
```

In the structure above, `start_vaddr` and `size` are the starting virtual address of the page table and the number of pages that are translated. They are used to check if a translation to be added or retrieved is inside the boundaries of the table.
All addresses inside the page table are page aligned so that the conversion to index is trivial.

This is the code to get the `pages` array index from the virtual address. This value will later be used to assign or retrieve a physical address from the table:

```C
    vaddr = vaddr & PAGE_FRAME;
    page_index = (vaddr - pt->start_vaddr) / PAGE_SIZE;
```

At the table creation ( function `struct pagetable *pt_create(unsigned long size, vaddr_t start_address)` ) all the pages do not have a correspondence to a physical address, so they are assigned the constant `PT_UNPOPULATED_PAGE`. When a page is swapped out ( function `void pt_swap_out(struct pagetable *pt, off_t swapfile_offset, vaddr_t swapped_entry)`), we save in the page table the offset in the swapfile returned by the swapfile manager concatenated with the constant `PT_SWAPPED_PAGE` in this way :

```C
pt->pages[page_index] = ((paddr_t)swapfile_offset) | PT_SWAPPED_PAGE
```

We are sure that the constant PT_SWAPPED_PAGE does not affect normal operation because it is set in the lowest bit of the address, that can't be normally used because the CPU works on 4 bit aligned addresses.

Here are the 2 constants used in this context:

```C
#define PT_SWAPPED_PAGE 1
#define PT_SWAPPED_MASK 0x00000001
```

In doing so when the swapped page is requested back, it is possible to retrieve directly the offset of the swapfile ( function `off_t pt_get_swap_offset(struct pagetable *pt, vaddr_t vaddr)` ) and reload that page in memory. To retrieve the offset and to check if a page has been swapped out we use the mask `PT_SWAPPED_MASK` in the following way :

```C
    /*Page has been swapped out if the condition is true :*/
    (pt -> pages[i] & PT_SWAPPED_MASK) == PT_SWAPPED_PAGE
    /* The offset is retrieved in the following way*/
    off_t offset = (pt->pages[page_index] & (~PT_SWAPPED_MASK));
```

Other basic functions performed at page level by the page table are add and entry ( function `void pt_add_entry(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)`)to the table and get an entry ( `void pt_add_entry(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)`), using the trivial conversion described before. We also implemented the function `void pt_swap_in(struct pagetable *pt, vaddr_t vaddr, paddr_t paddr)` that is a wraper for the `pt_add_entry` function, when the page to be added to the page table is a swapped page.

There are some other functions needed to manage the page table structure: `pt_copy` to copy the entire struct, `pt_destroy` to destroy the struct and `pt_free` to free all the page saved in memory, in case there is a swapped entry, `swap_free` is called to invalidate the entry in the swapfile.


## swapfile

In order to support the swapping operation we created a management class called `swapfile.c`. This file contains most of the logic supporting the I/O from the swap file. The swapfile is located at the root of the filesystem (named SWAPFILE) and it is 9MB in space. _NOTE: These definitions are available and configurable in `swapfile.h`_. The swapfile is itself divided in pages of `PAGE_SIZE` length, just like the RAM memory. This is useful because it eases the management of the swapfile, the tracking of the offset in the swapfile, and the I/O operations.

In order to manage the swapfile we used a bitmap that allows us to keep track of which swapfile _page_ is free and which is filled, so it can't be used for swapping out at a given moment. Note also that the swapfile class is agnostic of which process occupies which page of the swapfile; this information is kept in the appropriate segment, and in particular in its **pagetable** (TODO: Aggiungi link a parte di pagetable in cui si parla dello swap)

Whenever the virtual memory manager starts, the `swap_init()` function is called. This function prepares the swapfile class to perform the following swapping operations. Such preparation consists in opening the swapfile, saving its handle in a global variable `struct vnode *swapfile`, and creating a properly sized bitmap called `swapmap` with `SWAPFILE_SIZE/PAGE_SIZE` entries.

The bitmap and its management methods were already implemented in the system in the `struct bitmap` data type, so we decided to use this structure.

During a `swap_out()` operation:

1. set the first available (zero) bit in the bitmap and retrieve its index
2. initialize the data structures required by a disk write operation (physical address to read from, length to write, offset in the file)
3. perform the disk write operation
4. return the offset in the swapfile where the page has been swapped out (equal to _bitmap_free_index \* PAGE_SIZE_)

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

`vm_bootstrap()` is the VM bootstrap function and it contains the necessary initialization to make the VM work. This consists in the initialization of the [`coremap` class](#coremap), of the [`swap` class](#swapfile), of the [`vmstats` class](#statistics) used for statistics and of the [TLB replacement algorithm](#tlb-replacement-algorithm). This function is called by `boot()` in _kern/main/main.c_, that contains the initialization sequence of the kernel.

On the other hand, `vm_shutdown()` is the VM shutdown function and it is the specular of the bootstrap, containing the functions deallocating the resources used by the VM. This function is called by the `shutdown()` function also defined in _kern/main/main.c_, that gets executed when the machine is powering off.

## Management of a page fault

It is implemented in the function `vm_fault()`. This function is directly called by the interrupt handler `mips_trap()` whenever the code of the interrupt assumes one of the following values (defined in _arch/mips/include/trapframe.h_):

- `EX_MOD` : attempted to write in a read-only page
- `EX_TLBL` : TLB miss on load
- `EX_TLBS` : TLB miss on store

These values have a corresponding _#define_ constant in _vm.h_. Respectively: `VM_FAULT_READONLY`, `VM_FAULT_READ`, `VM_FAULT_WRITE` are the possible values that the first parameter of `vm_fault` (`int faulttype`) can assume. This parameter is required because it produces different behaviour in the function.

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

We then declare some variables that will be used later on together with a _page-aligned_ version of the faulting virtual address. We then check if `faulttype` has some illegal value and return an error in that case. We also check if the fault happened because a write on a read-only page has been performed. If that is the case we return the `EACCES` error, that is defined in _errno.h_ as a _Permission Denied_ error.

In this latter case we want the calling process to terminate, as required by the project text. This is already done by default when returning an error from `vm_fault()`. To confirm this we can take a look at `mips_trap()` when calls `vm_fault()` and we can see that whenever `vm_fault()` returns a non-zero value (i.e. an error), and we are coming from code running in _user mode_, then the kernel executes a `kill_curthread()` on the thread/process that raised the exception.

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

At this point we check if the fault has been caused by a process, whose reference is contained in the global variable `curproc = curthread->t_proc`. If it has not been caused by a process it means that it happened inside the kernel at boot time. This fault is going to become a kernel panic in `mips_trap()`.

We perform a further check if the **address space** of the current process has been setup, which should have definetely have happened by now. If this is not the case, return an `EFAULT` error.

Finally, we try to distinguish which **segment** of the running process is the segment interested by the fault. If this fails, either the address is out of the declared segments ranges, or the address space has not been properly setup. Either way, we return an `EFAULT` error.

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

    if (ps->permissions != PAGE_STACK && unpopulated)
    {
        /* Load page from file*/
        result = seg_load_page(ps, faultaddress, paddr);
        if (result)
            return EFAULT;
    }

```

Now the VM checks if the appropriate segment contains already a logical->physical address translation for the requested logical address. This is done by looking up in the **page table** maintained by each segment. The following cases may happen:

1. The page has never been accessed before (`pt == PT_UNPOPULATED_PAGE`)
2. The page has been swapped out in the past and now it is required back by the process (`pt == PT_SWAPPED_PAGE`)
3. The page is resident in memory and the translation to the physical address has been returned from the page table.

In case (1), the memory manager needs to:

- allocate a free physical frame to the process, this is done by the function `alloc_upage()` defined in [`coremap.c`](#coremap); its physical address is held in `paddr`
- memorize the physical address of the allocated page in the page table of the correct segment
- distinguish if it is a _stack_ page or a _code/data_ page. If it is in a _code/data_ segment flag it as `unpopulated` which will cause below in the code to be **loaded from file**, if it is in a _stack_ segment just fill it with zeroes.

In case (2), the memory manager needs to:

- allocate a free physical frame to the process, this is done by the function `alloc_upage()` defined in [`coremap.c`](#coremap); its physical address is held in `paddr`
- perform a swap in the requesting segment. This is analyzed in the [swapping section](#swapfile)

In case (3), the memory manager doesn't need to perform any other operation, the physical address is already available in the variable `paddr` returned from the page table and the physical frame is already filled with the correct data.

### TLB replacement algorithm

```C
/* Ending part of vm_fault */
    vmstats_inc(VMSTAT_TLB_FAULT);
    /* Disable interrupts on this CPU while frobbing the TLB. */
    spl = splhigh();

    tlb_index = tlb_get_rr_victim();

    entry_hi = page_aligned_faultaddress;
    entry_lo = paddr | TLBLO_VALID;
    /* If writes are permitted set the DIRTY bit (see tlb.h) */
    if (ps->permissions == PAGE_RW || ps->permissions == PAGE_STACK)
    {
        entry_lo = entry_lo | TLBLO_DIRTY;
    }
    DEBUG(DB_VM, "suchvm: 0x%x -> 0x%x\n", page_aligned_faultaddress, paddr);

    tlb_read(&old_hi, &old_lo, tlb_index);
    if (old_lo & TLBLO_VALID){
        vmstats_inc(VMSTAT_TLB_FAULT_REPLACE);
    } else {
        vmstats_inc(VMSTAT_TLB_FAULT_FREE);
    }

    tlb_write(entry_hi, entry_lo, tlb_index);
    splx(spl);

    return 0;
}
```
This portion of code is dedicated to the TLB management now that the logical -> physical translation is available. First of all we get the index of the TLB entry where we should save the next entry using the `tlb_get_rr_victim()` function, that implements a Round-Robin algorithm to actually compute the index. This function looks like:

```C
    unsigned int victim = current_victim;
    current_victim = (current_victim + 1) % NUM_TLB;
    return victim;
```

Basically it retrieves the index from a global variable called `current_victim` and increments the index by 1 while capping it at the size of the TLB. The saved index is then returned to be used. The modular increment operation is what ensures a Round-Robin operation.

Even when the TLB is invalidated at some point by an `as_activate` run after a thread switch for example, the `current_victim` variable does not need to be reset, the entries will simply be saved starting from a middle point of the TLB.

This function together with the rest of the TLB accesses is run after _disabling the CPU interrupts_ (`splhigh`). This is because the function accesses a global variable and to avoid race conditions on the TLB entries.

Let's talk about how the address are saved in the TLB. Looking at the specifications of the TLB for this architecture, each entry is divided in two parts (`entry_hi` and `entry_lo`).

In `entry_hi` we save the _page-aligned_ virtual address that caused the page fault. In `entry_lo` we save the _page-aligned_ physical address of the frame that has been allocated to hold the faulting page. Moreover we need to set the bit at position `TLBLO_VALID` bit to tell the TLB that the translation is valid. In the `entry_lo` part also the R/W permission on the page are specified.

To be more precise, by default a page has read access, the write access is instead granted by setting the bit at position `TLBLO_DIRTY`. This is done according to what are the permissions specified in the faulting _segment_. In our case the types of segments that needed write access are a data segment tagged as `PAGE_RW` and a stack segment tagged as `PAGE_STACK`.

We then perform a check if the victim TLB entry was valid or not to update the relative statistics. Finally we save the two entries in the TLB at the index obtained before and reenable interrupts.


## Statistics

In order to compute the stats on page faults and the swap we created an array of counters, `stats_counts[]`, and we assigned a name to each index:

```C
#define VMSTAT_TLB_FAULT              0     // Total TLB fault
#define VMSTAT_TLB_FAULT_FREE         1     // Faults whithout replacement in the TLB
#define VMSTAT_TLB_FAULT_REPLACE      2     // Faults whithout replacement in the TLB
#define VMSTAT_TLB_INVALIDATE         3     // TLB invalidations
#define VMSTAT_TLB_RELOAD             4     // TLB misses for pages that were already in memory
#define VMSTAT_PAGE_FAULT_ZERO        5     // TLB misses that required a new page to be zero-filled
#define VMSTAT_PAGE_FAULT_DISK        6     // TLB misses that required a page to be loaded from disk
#define VMSTAT_ELF_FILE_READ          7     // Page faults that require getting a page from the ELF file
#define VMSTAT_SWAP_FILE_READ         8     // Page faults that require getting a page from the swap file
#define VMSTAT_SWAP_FILE_WRITE        9     // Page faults that require writing a page to the swap file
```

To access to the stats and modify the array you need a spinlock (`stats_lock`) that ensures the atomicity of the operation, except for the time when the stats are showed by `vmstats_print` at the shutdown when the atomicity is ensured by the fact that no other process is running. In this function, also the following checks are fulfilled :

```C
TLB Faults == TLB Faults with Free + TLB Faults with Replace
TLB Faults == TLB Reloads + Page Faults (Zeroed) + Page Faults (Disk)
ELF File reads + Swapfile reads == Page Faults (Disk)
```

The function `vmstats_init` is used to initialize the array of counters acquiring the lock and to activate the stats setting `stats_active = 1`. The spinlock `stats_lock`is initialized statically.

The specified counter is incremented via the function `vmstats_inc` to which the proper array index is passed as parameter.

# Addressed problems
TODO: Qua descriviamo i problemi che abbiamo risolto

# Tests

We ran different tests in order to check the correctness of the virtual memory manager, both in basic and stress cases. We also tried to isolate the components of the manager that we were testing such as faulting and swapping.

These are the tests in testbin that we ran :

- palin
- sort
- zero (apart from sbrk test)
- faulter
- ctest
- huge
- matmult
- faulter modified (faulterro)
- tlbreplace (created)

We added the `tlbreplace` test that tries to cause a TLB replace, knowing the overall architecture.

We also modified the size of the swap file and ran `matmult` to check if a _kernel panic_ is raised because the size of the logical address space is greater than the sum of the physical memory and swap file sizes.

To further test our tracking of the physical memory we ran the following kernel tests:

- at
- at2
- bt
- km1
- km2

Below is report a table with the statistics for each test

| Test name | TLB Faults | TLB Faults with Free | TLB Faults with Replace | TLB Invalidations | TLB Reloads | Page Faults (zero filled) | Page Faults (disk) | ELF File Read | Swapfile Read | Swapfile Writes |
|  :---:  | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: | :---: |
|  palin  |14008  |14008  |0      |7809   |14003  |1      |4      |4      |0      |0      |
|  sort   |7052   |7014   |38     |3144   |5316   |289    |1447   |4      |1443   |1661   |
|  huge   |7459   |7441   |18     |6752   |3880   |512    |3067   |3      |3064   |3506   |
| matmult |4355   |4336   |19     |1230   |3547   |380    |428    |3      |425    |733    |
|  ctest  |248591 |248579 |12     |249633 |123627 |257    |124707 |3      |124704 |124889 |
|  zero   |130    |130    |0      |128    |124    |3      |3      |3      |0      |0      |

The _faulter_ test tries to access an address that may be outside of the process address space boundaries. The _faulterro_ test attempts to write to a _read-only_ memory region within its boundaries. Both return segmentation fault and the process is killed, however the kernel does not panic.

In the _swapfile kernel panic_ test the kernel panic is succesfully raised.

The `tlbreplace` test returns a TLB replace stat of *2*. This *2* is because after the TLB is filled, the first entry (code page) is overwritten (+1). Since the code is then needed, another page fault is issued and another entry is replaced (+1). 

# Workload division

In this project there is an high level of coupling among all components and so there was the need to work together most of the time in a sort of "pair programming style". This project was carried out mostly remotely using the visual studio LiveShare feature wich allow us to write on a single file simultaneously. Only some part of work was divided among the component but after each implementation there was a session to update the state of the development.

Before each feature implementation there was a brain-storming session in wich we analyzed the possible solution to the problems.

For the debug phase, each time a problem arose there were first a static analysis on the code to try to understand errors and then a dynamic analysis of the code using the degugger (the GUI version of GDB).

The code has been shared using a GitHub repository.

# Conclusions

Working on this project allowed us to understand more thoroughly the topic explained during the lessons. Thanks to the theoretical and practical issues arisen during the work, we improved our problem solving capabilities and to organize to job in the best way possible to minimize the probability of errors.

# Improvements

A possible improvement for our project colud be the multi-processor capability for the on demanding page.
