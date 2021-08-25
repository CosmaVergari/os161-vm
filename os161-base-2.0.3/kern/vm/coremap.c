#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <suchvm.h>
#include <coremap.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static struct spinlock freemem_lock = SPINLOCK_INITIALIZER;

static unsigned char *freeRamFrames = NULL; // Can be converted in bitmap
static unsigned long *allocSize = NULL;
static int nRamFrames = 0;

static int allocTableActive = 0;

static int isTableActive()
{
	int active;
	spinlock_acquire(&freemem_lock);
	active = allocTableActive;
	spinlock_release(&freemem_lock);
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
	/* alloc freeRamFrame and allocSize */
	freeRamFrames = kmalloc(sizeof(unsigned char) * nRamFrames);
	if (freeRamFrames == NULL)
		return;
	allocSize = kmalloc(sizeof(unsigned long) * nRamFrames);
	if (allocSize == NULL)
	{
		/* reset to disable this vm management */
		freeRamFrames = NULL;
		return;
	}

	for (i = 0; i < nRamFrames; i++)
	{
		freeRamFrames[i] = (unsigned char)0;
		allocSize[i] = 0;
	}

	spinlock_acquire(&freemem_lock);
	allocTableActive = 1;
	spinlock_release(&freemem_lock);
}

/* 
 *  Search in freeRamFrames if there is a slot npages long
 *  of *freed* frames that can be occupied.
 */
static paddr_t getfreeppages(unsigned long npages)
{
	paddr_t addr;
	long i, first, found;
	long np = (long)npages;

	if (!isTableActive())
		return 0;

	spinlock_acquire(&freemem_lock);
	for (i = 0, first = found = -1; i < nRamFrames; i++)
	{
		if (freeRamFrames[i])
		{
			if (i == 0 || !freeRamFrames[i - 1])
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
			freeRamFrames[i] = (unsigned char)0;
		}
		allocSize[found] = np;
		addr = (paddr_t)found * PAGE_SIZE;
	}
	else
	{
		addr = 0;
	}

	spinlock_release(&freemem_lock);

	return addr;
}

/*
 *	Get pages to occupy, first search in free pages otherwise
 *	call ram_stealmem()
 */
static paddr_t getppages(unsigned long npages)
{
	paddr_t addr;

	/* try freed pages first, managed by coremap */
	addr = getfreeppages(npages);
	if (addr == 0)
	{
		/* call stealmem if nothing found */
		spinlock_acquire(&stealmem_lock);
		addr = ram_stealmem(npages);
		spinlock_release(&stealmem_lock);
	}
	if (addr != 0 && isTableActive())
	{
		spinlock_acquire(&freemem_lock);
		allocSize[addr / PAGE_SIZE] = npages;
		spinlock_release(&freemem_lock);
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

	if (!isTableActive())
		return 0;
	first = addr / PAGE_SIZE;
	KASSERT(allocSize != NULL);
	KASSERT(nRamFrames > first);

	spinlock_acquire(&freemem_lock);
	for (i = first; i < first + np; i++)
	{
		freeRamFrames[i] = (unsigned char)1;
	}
	spinlock_release(&freemem_lock);

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
	if (isTableActive())
	{
		paddr_t paddr = addr - MIPS_KSEG0;
		long first = paddr / PAGE_SIZE;
		KASSERT(allocSize != NULL);
		KASSERT(nRamFrames > first);
		freeppages(paddr, allocSize[first]);
	}
}
