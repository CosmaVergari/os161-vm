#ifndef VM_STATS_H
#define VM_STATS_H

/* ----------------------------------------------------------------------- */
/* Virtual memory stats */
/* Tracks stats on user programs */

/*  REQUIRED STATS 
 *
 *  • TLB Faults: The number of TLB misses that have occurred (not including faults that cause
 *                  a program to crash).
 *  • TLB Faults with Free: The number of TLB misses for which there was free space in the
 *                  TLB to add the new TLB entry (i.e., no replacement is required).
 *  • TLB Faults with Replace: The number of TLB misses for which there was no free space
 *                  for the new TLB entry, so replacement was required.
 *  • TLB Invalidations: The number of times the TLB was invalidated (this counts the number
 *                  times the entire TLB is invalidated NOT the number of TLB entries invalidated)
 *  • TLB Reloads: The number of TLB misses for pages that were already in memory.
 *  • Page Faults (Zeroed): The number of TLB misses that required a new page to be zero-filled.
 *  • Page Faults (Disk): The number of TLB misses that required a page to be loaded from disk.
 *  • Page Faults from ELF: The number of page faults that require getting a page from the ELF file.
 *  • Page Faults from Swapfile: The number of page faults that require getting a page from the
 *                  swap file.
 *  • Swapfile Writes: The number of page faults that require writing a page to the swap file.
 *
 */

#define VMSTAT_TLB_FAULT              0  
#define VMSTAT_TLB_FAULT_FREE         1
#define VMSTAT_TLB_FAULT_REPLACE      2
#define VMSTAT_TLB_INVALIDATE         3
#define VMSTAT_TLB_RELOAD             4
#define VMSTAT_PAGE_FAULT_ZERO        5 // TODO
#define VMSTAT_PAGE_FAULT_DISK        6 // TODO
#define VMSTAT_ELF_FILE_READ          7
#define VMSTAT_SWAP_FILE_READ         8
#define VMSTAT_SWAP_FILE_WRITE        9
#define VMSTAT_COUNT                  10

/* ----------------------------------------------------------------------- */

/* Initialize the statistics */
void vmstats_init(void);

/* Increment the specified counter */
void vmstats_inc(unsigned int index);

/* Print the statistics */
void vmstats_print(void);

#endif /* VM_STATS_H */