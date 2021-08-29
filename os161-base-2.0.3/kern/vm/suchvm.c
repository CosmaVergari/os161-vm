#include <types.h>
#include <current.h>
#include <cpu.h>
#include <suchvm.h>
#include <coremap.h>

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
void vm_bootstrap(void) {
    coremap_init();
}

int
vm_fault(int faulttype, vaddr_t faultaddress)
{
    (void) faulttype;
    (void) faultaddress;
	return 0;
}

void
vm_tlbshootdown(const struct tlbshootdown *ts)
{
	(void)ts;
	panic("suchvm tried to do tlb shootdown?!\n");
}