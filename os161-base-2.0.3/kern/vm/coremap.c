#include <types.h>
#include <kern/errno.h>
#include <spinlock.h>
#include <current.h>
#include <cpu.h>
#include <proc.h>
#include <addrspace.h>
#include <suchvm.h>
#include <coremap.h>
#include <swapfile.h>

static struct spinlock stealmem_lock = SPINLOCK_INITIALIZER;

static struct spinlock coremap_lock = SPINLOCK_INITIALIZER;
static struct coremap_entry *coremap = NULL;
static int nRamFrames = 0;
static int coremapActive = 0;

static struct spinlock victim_lock = SPINLOCK_INITIALIZER;
static unsigned long const_invalid_reference = 0;
static unsigned long victim = 0;
static unsigned long last_alloc = 0;

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
		coremap[i].next_allocated = const_invalid_reference;
		coremap[i].prev_allocated = const_invalid_reference;
	}

	const_invalid_reference = nRamFrames;
	last_alloc = const_invalid_reference;
	victim = const_invalid_reference;

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
		coremap[addr / PAGE_SIZE].allocSize = npages;
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
		coremap[i].vaddr = 0;
		coremap[i].as = NULL;
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

/* Paging support */

/*
 *  Only called by the user to alloc 1 page 
 *	First search in free pages otherwise call ram_stealmem()
 */
static paddr_t getppage_user(vaddr_t associated_vaddr)
{
	struct addrspace *as;
	struct prog_segment *victim_ps;
	paddr_t addr;
	unsigned long last_temp, victim_temp, newvictim;
	off_t swapfile_offset;
	int result;

	as = proc_getas();
	KASSERT(as != NULL); /* get_a_page shouldn't be called before VM is initialized */

	/* The virtual address should be that of the beginning of a page */
	KASSERT((associated_vaddr & PAGE_FRAME) == associated_vaddr);

	/* try freed pages first that are already managed by coremap */
	addr = getfreeppages(1, COREMAP_BUSY_USER, as, associated_vaddr);
	if (addr == 0)
	{
		/* call stealmem if nothing found */
		spinlock_acquire(&stealmem_lock);
		addr = ram_stealmem(1);
		spinlock_release(&stealmem_lock);
	}

	/* Update the coremap to track the newly obtained page from ram_stealmem */
	if (isCoremapActive())
	{
		spinlock_acquire(&victim_lock);
		last_temp = last_alloc;
		victim_temp = victim;
		spinlock_release(&victim_lock);

		if (addr != 0)
		{
			spinlock_acquire(&coremap_lock);
			int page_i = (addr / PAGE_SIZE);
			coremap[page_i].entry_type = COREMAP_BUSY_USER;
			coremap[page_i].allocSize = 1;
			coremap[page_i].as = as;
			coremap[page_i].vaddr = associated_vaddr;

			if (last_temp != const_invalid_reference)
			{
				// Means that a page has been already allocated and last_temp is valid
				coremap[last_temp].next_allocated = page_i;
				coremap[page_i].prev_allocated = last_temp;
				coremap[page_i].next_allocated = const_invalid_reference;
			}
			else
			{
				coremap[page_i].prev_allocated = const_invalid_reference;
				coremap[page_i].next_allocated = const_invalid_reference;
			}
			spinlock_release(&coremap_lock);

			spinlock_acquire(&victim_lock);
			if (victim_temp == const_invalid_reference)
				victim = page_i;
			last_alloc = page_i;
			spinlock_release(&victim_lock);
		}
		else
		{
			/*   ** SWAP OUT **
			 *
			 * If returned addr is 0 (exhausted ram space) we need to swap. 
			 * Nella condizione per trovare una pagina vittima controllare per 
		     * entry_type che sia COREMAP_BUSY_USER-> DONE assicurato dalla allocqueue 
		     */

			
			result = swap_out(victim_temp, &swapfile_offset);
			if (result)
			{
				panic("Impossible to swap out a page to file\n");
			}

			spinlock_acquire(&coremap_lock);
			victim_ps = as_find_segment(coremap[victim_temp].as, coremap[victim_temp].vaddr);
			seg_swap_out(victim_ps, swapfile_offset, coremap[victim_temp].vaddr);
			
			addr = (paddr_t)victim_temp * PAGE_SIZE;

			/*
			 * Update the coremap information and the allocqueue (put occupied block)
			 * at the end of the queue
			 */
			KASSERT(coremap[victim_temp].entry_type == COREMAP_BUSY_USER);
			KASSERT(coremap[victim_temp].allocSize == 1);

			coremap[victim_temp].vaddr = associated_vaddr;
			coremap[victim_temp].as = as;

			newvictim = coremap[victim_temp].next_allocated;
			coremap[victim_temp].next_allocated = const_invalid_reference;
			coremap[victim_temp].prev_allocated = last_temp;
			spinlock_release(&coremap_lock);

			spinlock_acquire(&victim_lock);
			/* 
			 * Update the victim to the following block in allocqueue and
			 * last_alloc to the newly occupied frame
			 */
			KASSERT(newvictim != const_invalid_reference);
			last_alloc = victim_temp;
			victim = newvictim;
			spinlock_release(&victim_lock);
		}
	}

	return addr;
}

static void freeppage_user(paddr_t paddr)
{
	unsigned long last_new, victim_new;

	if (isCoremapActive())
	{
		unsigned long page_i = paddr / PAGE_SIZE;
		KASSERT((unsigned int)nRamFrames > page_i);
		KASSERT(coremap[page_i].allocSize == 1);

		/* Take old variables to change them in next conditions */
		spinlock_acquire(&victim_lock);
		last_new = last_alloc;
		victim_new = victim;
		spinlock_release(&victim_lock);

		/* Update the allocation queue and prepare the indexes */
		spinlock_acquire(&coremap_lock);
		if (coremap[page_i].prev_allocated == const_invalid_reference)
		{
			/* No elements before this in allocation queue */

			if (coremap[page_i].next_allocated == const_invalid_reference)
			{
				/* Only element in allocation queue */
				victim_new = const_invalid_reference;
				last_new = const_invalid_reference;
			}
			else
			{
				/* Shift the head forwards in allocation queue */
				KASSERT(page_i == victim_new);
				coremap[coremap[page_i].next_allocated].prev_allocated = const_invalid_reference;
				victim_new = coremap[page_i].next_allocated;
			}
		}
		else
		{
			/* There are elements before this one in allocation queue */

			if (coremap[page_i].next_allocated == const_invalid_reference)
			{
				/* Last one in allocation queue */
				KASSERT(page_i == last_new);
				coremap[coremap[page_i].prev_allocated].next_allocated = const_invalid_reference;
				last_new = coremap[page_i].prev_allocated;
			}
			else
			{
				/* In the middle of the allocation queue */
				coremap[coremap[page_i].next_allocated].prev_allocated = coremap[page_i].prev_allocated;
				coremap[coremap[page_i].prev_allocated].next_allocated = coremap[page_i].next_allocated;
			}
		}

		coremap[page_i].next_allocated = const_invalid_reference;
		coremap[page_i].prev_allocated = const_invalid_reference;

		spinlock_release(&coremap_lock);

		freeppages(paddr, 1);

		spinlock_acquire(&victim_lock);
		victim = victim_new;
		last_alloc = last_new;
		spinlock_release(&victim_lock);
	}
}

paddr_t alloc_upage(vaddr_t vaddr)
{
	paddr_t pa;

	suchvm_can_sleep();
	pa = getppage_user(vaddr);

	return pa;
}

void free_upage(paddr_t paddr)
{
	freeppage_user(paddr);
}