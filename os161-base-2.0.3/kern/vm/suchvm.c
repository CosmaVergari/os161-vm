#include <types.h>
#include <kern/errno.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>
#include <spl.h>
#include <mips/tlb.h>
#include <vm.h>
#include <suchvm.h>
#include <coremap.h>
#include <segments.h>

/* This index is used to perform round-robin entry replacement
 * in the TLB. It is not reset in the as_activate function because
 * it doesn't matter from where the TLB starts saving the entries
 * the policy is what is important (Round Robin).
 * So it is initialized just once when suchvm starts (vm_bootstrap)
 */
static unsigned int current_victim;

/* Implements the round robin policy and returns the index
 * of the next exntry to be replace in the TLB
 */
static unsigned int tlb_get_rr_victim(void)
{
    unsigned int victim;
    victim = current_victim;
    current_victim = (current_victim + 1) % NUM_TLB;
    return victim;
}

void suchvm_can_sleep(void)
{
    if (CURCPU_EXISTS())
    {
        /* must not hold spinlocks */
        KASSERT(curcpu->c_spinlocks == 0);

        /* must not be in an interrupt handler */
        KASSERT(curthread->t_in_interrupt == 0);
    }
}

/*
 * This function is called at the end of the boot process and
 * it initializes the data structures required by suchvm
 */
void vm_bootstrap(void)
{
    coremap_init();
    current_victim = 0;
}

/*
 * This function is called on a TLB miss
 * When using paging this may happen when a page is in page table
 * but not yet in TLB (e.g. after a context switch) or when a page
 * has yet to be loaded in memory, in this case it must be loaded
 * from disk
 */
int vm_fault(int faulttype, vaddr_t faultaddress)
{
    unsigned int tlb_index;
    int spl, result;
    char unpopulated;
    vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
    paddr_t paddr;
    uint32_t entry_hi, entry_lo;
    struct addrspace *as;
    struct prog_segment *ps;

    /* Align virtual address to page beginning virtual address */
    faultaddress &= PAGE_FRAME;

    DEBUG(DB_VM, "suchvm: fault: 0x%x\n", faultaddress);

    /* Check for unknown fault type */
    if (faulttype != VM_FAULT_READONLY &&
        faulttype != VM_FAULT_READ &&
        faulttype != VM_FAULT_WRITE)
    {
        return EINVAL;
    }

    if (faulttype == VM_FAULT_READONLY)
    {
        /* TODO: Must terminate the running process, EACCES is ok? */
        return EACCES;
    }

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

    /* Check if address space is fully populated */
    KASSERT(as != NULL);
    KASSERT(as->seg1 != NULL);
    KASSERT(as->seg2 != NULL);
    KASSERT(as->seg_stack != NULL);
    KASSERT(as->seg1->pagetable != NULL);
    KASSERT(as->seg2->pagetable != NULL);
    KASSERT(as->seg_stack->pagetable != NULL);
    KASSERT((as->seg1->base_vaddr & PAGE_FRAME) == as->seg1->base_vaddr);
    KASSERT((as->seg2->base_vaddr & PAGE_FRAME) == as->seg2->base_vaddr);
    KASSERT((as->seg_stack->base_vaddr & PAGE_FRAME) == as->seg_stack->base_vaddr);

    /* 
     * Check if the fault address is within the boundaries of
     * the current address space
     */
    vbase1 = as->seg1->base_vaddr;
    vtop1 = vbase1 + (as->seg1->n_pages) * PAGE_SIZE;
    vbase2 = as->seg2->base_vaddr;
    vtop2 = vbase2 + (as->seg2->n_pages) * PAGE_SIZE;
    stackbase = USERSTACK - SUCHVM_STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;
    if (faultaddress >= vbase1 && faultaddress < vtop1)
    {
        ps = as->seg1;
    }
    else if (faultaddress >= vbase2 && faultaddress < vtop2)
    {
        ps = as->seg2;
    }
    else if (faultaddress >= stackbase && faultaddress < stacktop)
    {
        ps = as->seg_stack;
    }
    else
    {
        return EFAULT;
    }

    /* 
     * Get the physical address of the received virtual address
     * It also loads the page from disk if it was not in the page table
     * and if it is not in a stack segment.
     */
    unpopulated = 0;
    paddr = seg_get_paddr(ps, faultaddress);
    if (paddr == PT_UNPOPULATED_PAGE)
    {
        /* 
         * Alloc one page in coremap and add to page table
         */
        paddr = alloc_upage(faultaddress);
        seg_add_pt_entry(ps, faultaddress, paddr);
        unpopulated = 1;
    }

    /* make sure it's page-aligned */
    KASSERT((paddr & PAGE_FRAME) == paddr);

    tlb_index = tlb_get_rr_victim();

    if (faulttype == VM_FAULT_READ && ps->permissions != PAGE_STACK && unpopulated)
    {
        /* Load page from file*/
        result = seg_load_page(ps, faultaddress, paddr);
        if (result)
            return EFAULT;
    }

    /* Disable interrupts on this CPU while frobbing the TLB. */
    spl = splhigh();

    entry_hi = faultaddress;
    entry_lo = paddr | TLBLO_VALID;
    /* If writes are permitted set the DIRTY bit (see tlb.h) */
    if (ps->permissions == PAGE_RW || ps->permissions == PAGE_STACK)
    {
        entry_lo = entry_lo | TLBLO_DIRTY;
    }
    DEBUG(DB_VM, "suchvm: 0x%x -> 0x%x\n", faultaddress, paddr);
    tlb_write(entry_hi, entry_lo, tlb_index);
    splx(spl);

    return 0;
}

void vm_tlbshootdown(const struct tlbshootdown *ts)
{
    (void)ts;
    panic("suchvm tried to do tlb shootdown?!\n");
}