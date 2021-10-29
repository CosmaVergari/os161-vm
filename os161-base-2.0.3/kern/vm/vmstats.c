#include <types.h>
#include <lib.h>
#include <synch.h>
#include <spl.h>
#include <vmstats.h>

/* Counters for tracking statistics */
static unsigned int stats_counts[VMSTAT_COUNT];

struct spinlock stats_lock;

/* Printing purposes */
static const char *stats_names[] = {
 /*  0 */ "TLB Faults", 
 /*  1 */ "TLB Faults with Free",
 /*  2 */ "TLB Faults with Replace",
 /*  3 */ "TLB Invalidations",
 /*  4 */ "TLB Reloads",
 /*  5 */ "Page Faults (Zeroed)",
 /*  6 */ "Page Faults (Disk)",
 /*  7 */ "Page Faults from ELF",
 /*  8 */ "Page Faults from Swapfile",
 /*  9 */ "Swapfile Writes",
};


void
vmstats_init(void)
{
    int i = 0;
    spinlock_init(&stats_lock);

    spinlock_acquire(&stats_lock);   

    for (i=0; i<VMSTAT_COUNT; i++) {
        stats_counts[i] = 0;
    }
    
    spinlock_release(&stats_lock);
}

void
vmstats_inc(unsigned int index)
{
    //TODO : check vmstat_init has been called before

    spinlock_acquire(&stats_lock);

    KASSERT(index < VMSTAT_COUNT);
    stats_counts[index]++;

    spinlock_release(&stats_lock);

}


/* ---------------------------------------------------------------------- */
// TODO : personalizzare il commento
/* Assumes vmstat_init has already been called */
/* NOTE: We do not grab the spinlock here because kprintf may block
 * and we can't block while holding a spinlock.
 * Just use this when there is only one thread remaining.
 */

void
vmstats_print(void)
{
    int i = 0;
    int free_plus_replace = 0;
    int disk_plus_zeroed_plus_reload = 0;
    int tlb_faults = 0;
    int elf_plus_swap_reads = 0;
    int disk_reads = 0;

    for (i=0; i<VMSTAT_COUNT; i++) {
    }

    kprintf("VMSTATS:\n");
    for (i=0; i<VMSTAT_COUNT; i++) {
        kprintf("VMSTAT %25s = %10d\n", stats_names[i], stats_counts[i]);
    }

    tlb_faults = stats_counts[VMSTAT_TLB_FAULT];
    free_plus_replace = stats_counts[VMSTAT_TLB_FAULT_FREE] + stats_counts[VMSTAT_TLB_FAULT_REPLACE];
    disk_plus_zeroed_plus_reload = stats_counts[VMSTAT_PAGE_FAULT_DISK] + stats_counts[VMSTAT_PAGE_FAULT_ZERO] + stats_counts[VMSTAT_TLB_RELOAD];
    elf_plus_swap_reads = stats_counts[VMSTAT_ELF_FILE_READ] + stats_counts[VMSTAT_SWAP_FILE_READ];
    disk_reads = stats_counts[VMSTAT_PAGE_FAULT_DISK];

    kprintf("VMSTAT TLB Faults with Free + TLB Faults with Replace = %d\n", free_plus_replace);
    if (tlb_faults != free_plus_replace) {
        kprintf("WARNING: TLB Faults (%d) != TLB Faults with Free + TLB Faults with Replace (%d)\n", tlb_faults, free_plus_replace); 
    }

    kprintf("VMSTAT TLB Reloads + Page Faults (Zeroed) + Page Faults (Disk) = %d\n", disk_plus_zeroed_plus_reload);
    if (tlb_faults != disk_plus_zeroed_plus_reload) {
        kprintf("WARNING: TLB Faults (%d) != TLB Reloads + Page Faults (Zeroed) + Page Faults (Disk) (%d)\n", tlb_faults, disk_plus_zeroed_plus_reload); 
    }

    kprintf("VMSTAT ELF File reads + Swapfile reads = %d\n", elf_plus_swap_reads);
    if (disk_reads != elf_plus_swap_reads) {
        kprintf("WARNING: ELF File reads + Swapfile reads != Page Faults (Disk) %d\n", elf_plus_swap_reads);
    }
}