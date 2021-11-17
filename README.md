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
        - in case tlb is full is applied roud_robin for replacing




# Implementation
<!-- Qua parliamo di come abbiamo realizzato le funzionalità principali -->

# Addressed problems
Qua descriviamo i problemi che abbiamo risolto

# Tests
Che cosa fanno i test e che passano.

# Improvements
Non è stato implementato la capacità multi-processor.
Opzionale
