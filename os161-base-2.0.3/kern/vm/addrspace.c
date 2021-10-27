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
#include <vfs.h>
#include <addrspace.h>
#include <vm.h>
#include <proc.h>
#include <spl.h>
#include <mips/tlb.h>
#include <segments.h>
#include <suchvm.h>

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
	struct vnode *v;
	KASSERT(as != NULL);

	/* 
	* Destroy the defined segments, close the ELF file 
	* and free the structure 
	*/
	v = as->seg1->elf_vnode;
	if (as -> seg1 != NULL) seg_destroy(as->seg1);
	if (as -> seg2 != NULL)seg_destroy(as->seg2);
	if (as -> seg_stack != NULL)seg_destroy(as->seg_stack);
	if (v != NULL) vfs_close(v);

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

	/* Invalidate the whole TLB */
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
	 * We didn't need it.
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
int as_define_region(struct addrspace *as, vaddr_t vaddr, size_t memsize, size_t file_size,
					 off_t offset, struct vnode *v, int readable, int writeable, int executable)
{
	size_t npages;

	KASSERT(as != NULL);
	KASSERT(v != NULL);

	/* Compute the length of the segment as number of pages */
	npages = memsize + (vaddr & ~(vaddr_t)PAGE_FRAME);
	npages = (npages + PAGE_SIZE - 1) & PAGE_FRAME;
	npages = npages / PAGE_SIZE;

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
		seg_define(as->seg1, vaddr, file_size, offset, memsize, npages, v, readable, writeable, executable);
		return 0;
	}
	if (as->seg2 == NULL)
	{
		as->seg2 = seg_create();
		if (as->seg2 == NULL)
		{
			return ENOMEM;
		}
		seg_define(as->seg2, vaddr, file_size, offset, memsize, npages, v, readable, writeable, executable);
		return 0;
	}

	/*
	 * Support for more than two regions is not available.
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
	if (seg_define_stack(as->seg_stack, 
	    				 USERSTACK - (SUCHVM_STACKPAGES * PAGE_SIZE), 
						 SUCHVM_STACKPAGES) != 0)
	{
		return ENOMEM;
	}

	/* Initial user-level stack pointer */
	*stackptr = USERSTACK;

	return 0;
}

struct prog_segment* as_find_segment(struct addrspace *as, vaddr_t vaddr) {
	
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	struct prog_segment *ps;

	/* Check if address space is fully populated */
    KASSERT(as != NULL);
    KASSERT(as->seg1 != NULL);
    KASSERT(as->seg2 != NULL);
    KASSERT(as->seg_stack != NULL);
    KASSERT(as->seg1->pagetable != NULL);
    KASSERT(as->seg2->pagetable != NULL);
    KASSERT(as->seg_stack->pagetable != NULL);

    /* 
     * Check if the fault address is within the boundaries of
     * the current address space
     */
    vbase1 = as->seg1->base_vaddr;
    vtop1 = vbase1 + as->seg1->mem_size;
    vbase2 = as->seg2->base_vaddr;
    vtop2 = vbase2 + as->seg2->mem_size;
    stackbase = USERSTACK - SUCHVM_STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;
    if (vaddr >= vbase1 && vaddr < vtop1)
    {
        ps = as->seg1;
    }
    else if (vaddr >= vbase2 && vaddr < vtop2)
    {
        ps = as->seg2;
    }
    else if (vaddr >= stackbase && vaddr < stacktop)
    {
        ps = as->seg_stack;
    }
    else
    {
        return NULL;
    }

	return ps;
}

struct prog_segment* as_find_segment_coarse(struct addrspace *as, vaddr_t vaddr) {
	
	vaddr_t vbase1, vtop1, vbase2, vtop2, stackbase, stacktop;
	struct prog_segment *ps;

	/* Check if address space is fully populated */
    KASSERT(as != NULL);
    KASSERT(as->seg1 != NULL);
    KASSERT(as->seg2 != NULL);
    KASSERT(as->seg_stack != NULL);
    KASSERT(as->seg1->pagetable != NULL);
    KASSERT(as->seg2->pagetable != NULL);
    KASSERT(as->seg_stack->pagetable != NULL);

    /* 
     * Check if the fault address is within the boundaries of
     * the current address space (page granularity)
     */
    vbase1 = (as->seg1->base_vaddr & PAGE_FRAME);
    vtop1 = vbase1 + (as->seg1->n_pages * PAGE_SIZE);
    vbase2 = (as->seg2->base_vaddr & PAGE_FRAME);
    vtop2 = vbase2 + (as->seg2->n_pages * PAGE_SIZE);
    stackbase = USERSTACK - SUCHVM_STACKPAGES * PAGE_SIZE;
    stacktop = USERSTACK;
    if (vaddr >= vbase1 && vaddr < vtop1)
    {
        ps = as->seg1;
    }
    else if (vaddr >= vbase2 && vaddr < vtop2)
    {
        ps = as->seg2;
    }
    else if (vaddr >= stackbase && vaddr < stacktop)
    {
        ps = as->seg_stack;
    }
    else
    {
        return NULL;
    }

	return ps;
}