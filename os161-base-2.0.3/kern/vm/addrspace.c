/*
 * Copyright (c) 2000, 2001, 2002, 2003, 2004, 2005, 2008, 2009
 *	The President and Fellows of Harvard College.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE UNIVERSITY AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE UNIVERSITY OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#include <types.h>
#include <kern/errno.h>
#include <lib.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <spl.h>
#include <mips/tlb.h>
#include <segments.h>
#include <suchvm.h>

/*
 * Note! If OPT_DUMBVM is set, as is the case until you start the VM
 * assignment, this file is not compiled or linked or in any way
 * used. The cheesy hack versions in dumbvm.c are used instead.
 */

struct addrspace *
as_create(void)
{
	struct addrspace *as;

	as = kmalloc(sizeof(struct addrspace));
	if (as == NULL)
	{
		return NULL;
	}

	as->seg1 = NULL;
	as->seg2 = NULL;
	as->seg_stack = NULL;

	return as;
}

int as_copy(struct addrspace *old, struct addrspace **ret)
{
	struct addrspace *newas;

	KASSERT(old != NULL);
	KASSERT(old->seg1 != NULL);
	KASSERT(old->seg2 != NULL);
	KASSERT(old->seg_stack != NULL);

	newas = as_create();
	if (newas == NULL)
	{
		return ENOMEM;
	}

	if (!seg_copy(old->seg1, &(newas->seg1)))
	{
		as_destroy(newas);
		return ENOMEM;
	}
	if (!seg_copy(old->seg2, &(newas->seg2)))
	{
		seg_destroy(newas->seg1);
		as_destroy(newas);
		return ENOMEM;
	}
	if (!seg_copy(old->seg_stack, &(newas->seg_stack)))
	{

		seg_destroy(newas->seg1);
		seg_destroy(newas->seg2);
		as_destroy(newas);
		return ENOMEM;
	}

	*ret = newas;
	return 0;
}

void as_destroy(struct addrspace *as)
{
	/* TODO
	 * Clean up as needed.
	 */
	KASSERT(as != NULL);

	seg_destroy(as->seg1);
	seg_destroy(as->seg2);
	seg_destroy(as->seg_stack);

	kfree(as);
}

void as_activate(void)
{
	struct addrspace *as;
	unsigned int i;
	int spl;

	as = proc_getas();
	if (as == NULL)
	{
		/*
		 * Kernel thread without an address space; leave the
		 * prior address space in place.
		 */
		return;
	}

	/* Disable interrupts on this CPU while frobbing the TLB. */
	spl = splhigh();

	for (i = 0; i < NUM_TLB; i++)
	{
		tlb_write(TLBHI_INVALID(i), TLBLO_INVALID(), i);
	}

	splx(spl);
}

void as_deactivate(void)
{
	/*
	 * Write this. For many designs it won't need to actually do
	 * anything. See proc.c for an explanation of why it (might)
	 * be needed.
	 */
}

/*
 * Set up a segment at virtual address VADDR of size MEMSIZE. The
 * segment in memory extends from VADDR up to (but not including)
 * VADDR+MEMSIZE.
 *
 * The READABLE, WRITEABLE, and EXECUTABLE flags are set if read,
 * write, or execute permission should be set on the segment. At the
 * moment, these are ignored. When you write the VM system, you may
 * want to implement them.
 */
int as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize,
					 off_t offset, struct vnode *v, int readable, int writeable, int executable)
{
	size_t npages;

	KASSERT(as != NULL);

	/* Align the region. First, the base... */
	memsize += vaddr & ~(vaddr_t)PAGE_FRAME;
	vaddr &= PAGE_FRAME;

	/* ...and now the length. */
	memsize = (memsize + PAGE_SIZE - 1) & PAGE_FRAME;

	npages = memsize / PAGE_SIZE;

	/*
	 * readable 	100
	 * writable 	010
	 * executable 	001
	*/
	if (as->seg1 == NULL)
	{
		as->seg1 = seg_create();
		if (as->seg1 == NULL)
		{
			return ENOMEM;
		}
		seg_define(as->seg1, vaddr, memsize, offset, npages, v, readable, writeable, executable);
		return 0;
	}
	if (as->seg2 == NULL)
	{
		as->seg2 = seg_create();
		if (as->seg2 == NULL)
		{
			return ENOMEM;
		}
		seg_define(as->seg2, vaddr, memsize, offset, npages, v, readable, writeable, executable);
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
	 * // TODO do we need to implement it?
	 */
	kprintf("dumbvm: Warning: too many regions\n");
	return ENOSYS;
}

int as_prepare_load(struct addrspace *as)
{
	/*	as_prepare_load is called only once
	 * 	here prepare for segment and the relative page table, 
	 *	that are allocated and not loaded with values
	 */

	if (seg_prepare(as->seg1) != 0)
	{
		return ENOMEM;
	}

	if (seg_prepare(as->seg2) != 0)
	{
		return ENOMEM;
	}

	return 0;
}

int as_complete_load(struct addrspace *as)
{
	// TODO: Check if needed

	(void)as;
	return 0;
}

int as_define_stack(struct addrspace *as, vaddr_t *stackptr)
{
	KASSERT(as != NULL);
	/* Check if the stack has not been already created */
	KASSERT(as->seg_stack == NULL);

	/*
	 * In os161 the stack grows downwards from 0x80000000 (USERSTACK).
	 * However in the segment we save the **base** virtual address we need
	 * to compute it
	 */

	as->seg_stack = seg_create();
	if (as->seg_stack == NULL)
	{
		return ENOMEM;
	}
	if (seg_define_stack(as->seg_stack, USERSTACK - (SUCHVM_STACKPAGES * PAGE_SIZE), SUCHVM_STACKPAGES) != 0)
	{
		return ENOMEM;
	}

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}
