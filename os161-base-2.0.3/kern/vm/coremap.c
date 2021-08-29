#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <suchvm.h>
#include <coremap.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;
static struct coremap_entry *coremap = NULL;
static int nRamFrames = 0;

static int coremapActive = 0;

static int isCoremapActive()
{
	int active;
	spinlock_acquire(&coremap_lock);
	active = coremapActive;
	spinlock_release(&coremap_lock);
	return active;
}

/*
 *  This function allocates the arrays containing info on the memory
 *  and enables the coremap functionality.
 * 	It is called by vm_bootstrap() in suchvm.c
 */
void coremap_init(void)
{
	int i;
	nRamFrames = ((int)ram_getsize()) / PAGE_SIZE;
	/* alloc coremap */
	coremap = kmalloc(sizeof(struct coremap_entry) * nRamFrames);
	if (coremap == NULL)
	{
		panic("Failed coremap alloc");
	}

	for (i = 0; i < nRamFrames; i++)
	{
		coremap[i].entry_type = COREMAP_UNTRACKED;
		coremap[i].vaddr = 0;
		coremap[i].as = NULL;
		coremap[i].allocSize = 0;
	}

	spinlock_acquire(&coremap_lock);
	coremapActive = 1;
	spinlock_release(&coremap_lock);
}

/* 
 *  Search in freeRamFrames if there is a slot npages long
 *  of *freed* frames that can be occupied.
 */

// TODO : controlla i parametri passati
static paddr_t getfreeppages(unsigned long npages,
							 unsigned char entry_type,
							 struct addrspace *as,
							 vaddr_t vaddr)
{
	paddr_t addr;
	long i, first, found;
	long np = (long)npages;

	if (!isCoremapActive())
		return 0;

	spinlock_acquire(&coremap_lock);
	for (i = 0, first = found = -1; i < nRamFrames; i++)
	{
		if (coremap[i].entry_type == COREMAP_FREED)
		{
			if (i == 0 || coremap[i - 1].entry_type != COREMAP_FREED)
				first = i; /* set first free in an interval */
			if (i - first + 1 >= np)
			{
				found = first;
				break;
			}
		}
	}

	if (found >= 0)
	{
		for (i = found; i < found + np; i++)
		{
			coremap[i].entry_type = entry_type;

			if (entry_type == COREMAP_BUSY_USER)
			{
				coremap[i].as = as;
				coremap[i].vaddr = vaddr;
			}
			else if (entry_type == COREMAP_BUSY_KERNEL)
			{
				coremap[i].as = NULL;
				coremap[i].vaddr = 0;
			}
			else
			{
				return EINVAL;
			}
		}
		coremap[found].allocSize = np;
		addr = (paddr_t)found * PAGE_SIZE;
	}
	else
	{
		addr = 0;
	}

	spinlock_release(&coremap_lock);

	return addr;
}

/*
 *  Only called by the kernel beacuse the user can alloc 1 page at time
 *	Get pages to occupy, first search in free pages otherwise
 *	call ram_stealmem()
 */
static paddr_t getppages(unsigned long npages)
{
	paddr_t addr;
	unsigned long i;

	/* try freed pages first, managed by coremap */
	addr = getfreeppages(npages, COREMAP_BUSY_KERNEL, NULL, 0);
	if (addr == 0)
	{
		/* call stealmem if nothing found */
		spinlock_acquire(&stealmem_lock);
		addr = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);
	}

	/* Update the coremap to track the newly obtained pages from ram_stealmem */
	if (addr != 0 && isCoremapActive())
	{
		spinlock_acquire(&coremap_lock);
		for (i = 0; i < npages; i++)
		{
			int page_i = (addr / PAGE_SIZE) + i;
			coremap[page_i].entry_type = COREMAP_BUSY_KERNEL;
		}
		coremap[addr/PAGE_SIZE].allocSize = npages;
		spinlock_release(&coremap_lock);
	}

	return addr;
}

/*
 *	Free a desired number of pages starting from addr.
 *	These free pages are now managed by coremap and not the
 *	lower level ram module.
 */
static int freeppages(paddr_t addr, unsigned long npages)
{
	long i, first, np = (long)npages;

	if (!isCoremapActive())
		return 0;
	first = addr / PAGE_SIZE;
	KASSERT(nRamFrames > first);

	spinlock_acquire(&coremap_lock);
	for (i = first; i < first + np; i++)
	{
		coremap[i].entry_type = COREMAP_FREED;
	}
	coremap[first].allocSize = 0;
	spinlock_release(&coremap_lock);

	return 1;
}

/* Allocate/free some kernel-space virtual pages, also used in kmalloc() */
vaddr_t alloc_kpages(unsigned npages)
{
	paddr_t pa;

	suchvm_can_sleep();
	pa = getppages(npages);
	if (pa == 0)
	{
		return 0;
	}
	return PADDR_TO_KVADDR(pa); /* Convert back to kernel virtual address */
}

void free_kpages(vaddr_t addr)
{
	if (isCoremapActive())
	{
		paddr_t paddr = addr - MIPS_KSEG0;
		long first = paddr / PAGE_SIZE;
		KASSERT(nRamFrames > first);
		freeppages(paddr, coremap[first].allocSize);
	}
}
