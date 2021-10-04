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

/*
 * Thread list functions, rather dull.
 */

#include <types.h>
#include <lib.h>
#include <allocqueue.h>

void
allocqueuenode_init(struct allocqueuenode *aqn, struct coremap_entry *t)
{
	DEBUGASSERT(aqn != NULL);
	KASSERT(t != NULL);

	aqn->aqn_next = NULL;
	aqn->aqn_prev = NULL;
	aqn->aqn_self = t;
}

void
allocqueuenode_cleanup(struct allocqueuenode *aqn)
{
	DEBUGASSERT(aqn != NULL);

	KASSERT(aqn->aqn_next == NULL);
	KASSERT(aqn->aqn_prev == NULL);
	KASSERT(aqn->aqn_self != NULL);
}

void
allocqueue_init(struct allocqueue *aq)
{
	DEBUGASSERT(aq != NULL);

	aq->aq_head.aqn_next = &aq->aq_tail;
	aq->aq_head.aqn_prev = NULL;
	aq->aq_tail.aqn_next = NULL;
	aq->aq_tail.aqn_prev = &aq->aq_head;
	aq->aq_head.aqn_self = NULL;
	aq->aq_tail.aqn_self = NULL;
	aq->aq_count = 0;
}

void
allocqueue_cleanup(struct allocqueue *aq)
{
	DEBUGASSERT(aq != NULL);
	DEBUGASSERT(aq->aq_head.aqn_next == &aq->aq_tail);
	DEBUGASSERT(aq->aq_head.aqn_prev == NULL);
	DEBUGASSERT(aq->aq_tail.aqn_next == NULL);
	DEBUGASSERT(aq->aq_tail.aqn_prev == &aq->aq_head);
	DEBUGASSERT(aq->aq_head.aqn_self == NULL);
	DEBUGASSERT(aq->aq_tail.aqn_self == NULL);

	KASSERT(allocqueue_isempty(aq));
	KASSERT(aq->aq_count == 0);

	/* nothing (else) to do */
}

bool
allocqueue_isempty(struct allocqueue *aq)
{
	DEBUGASSERT(aq != NULL);

	return (aq->aq_count == 0);
}

////////////////////////////////////////////////////////////
// internal

/*
 * Do insertion. Doesn't update aq_count.
 */
static
void
allocqueue_insertafternode(struct allocqueuenode *onlist, struct allocqueuenode *addee)
{
	DEBUGASSERT(addee->aqn_prev == NULL);
	DEBUGASSERT(addee->aqn_next == NULL);

	addee->aqn_prev = onlist;
	addee->aqn_next = onlist->aqn_next;
	addee->aqn_prev->aqn_next = addee;
	addee->aqn_next->aqn_prev = addee;
}

/*
 * Do insertion. Doesn't update aq_count.
 */
static
void
allocqueue_insertbeforenode(struct allocqueuenode *addee, struct allocqueuenode *onlist)
{
	DEBUGASSERT(addee->aqn_prev == NULL);
	DEBUGASSERT(addee->aqn_next == NULL);

	addee->aqn_prev = onlist->aqn_prev;
	addee->aqn_next = onlist;
	addee->aqn_prev->aqn_next = addee;
	addee->aqn_next->aqn_prev = addee;
}

/*
 * Do removal. Doesn't update aq_count.
 */
static
void
allocqueue_removenode(struct allocqueuenode *aqn)
{
	DEBUGASSERT(aqn != NULL);
	DEBUGASSERT(aqn->aqn_prev != NULL);
	DEBUGASSERT(aqn->aqn_next != NULL);

	aqn->aqn_prev->aqn_next = aqn->aqn_next;
	aqn->aqn_next->aqn_prev = aqn->aqn_prev;
	aqn->aqn_prev = NULL;
	aqn->aqn_next = NULL;
}

////////////////////////////////////////////////////////////
// public

void
allocqueue_addhead(struct allocqueue *aq, struct coremap_entry *t)
{
	DEBUGASSERT(aq != NULL);
	DEBUGASSERT(t != NULL);

    struct allocqueuenode *addee;
    addee = kmalloc(sizeof(struct allocqueuenode));
    allocqueuenode_init(addee, t);

    allocqueue_insertafternode(&aq->aq_head, addee);
	aq->aq_count++;
}

void
allocqueue_addtail(struct allocqueue *aq, struct coremap_entry *t)
{
	DEBUGASSERT(aq != NULL);
	DEBUGASSERT(t != NULL);

    struct allocqueuenode *addee;
    addee = kmalloc(sizeof(struct allocqueuenode));
    allocqueuenode_init(addee, t);

	allocqueue_insertbeforenode(addee, &aq->aq_tail);
	aq->aq_count++;
}

struct coremap_entry *
allocqueue_remhead(struct allocqueue *aq)
{
	struct allocqueuenode *aqn;

	DEBUGASSERT(aq != NULL);

	aqn = aq->aq_head.aqn_next;
	if (aqn->aqn_next == NULL) {
		/* list was empty  */
		return NULL;
	}
	allocqueue_removenode(aqn);
	DEBUGASSERT(aq->aq_count > 0);
	aq->aq_count--;
	return aqn->aqn_self;
}

struct coremap_entry *
allocqueue_remtail(struct allocqueue *aq)
{
	struct allocqueuenode *aqn;

	DEBUGASSERT(aq != NULL);

	aqn = aq->aq_tail.aqn_prev;
	if (aqn->aqn_prev == NULL) {
		/* list was empty  */
		return NULL;
	}
	allocqueue_removenode(aqn);
	DEBUGASSERT(aq->aq_count > 0);
	aq->aq_count--;
	return aqn->aqn_self;
}

void
allocqueue_insertafter(struct allocqueue *aq,
		       struct coremap_entry *onlist, struct coremap_entry *addee)
{
	allocqueue_insertafternode(&onlist->t_listnode, addee);
	aq->aq_count++;
}

void
allocqueue_insertbefore(struct allocqueue *aq,
			struct coremap_entry *addee, struct coremap_entry *onlist)
{
	allocqueue_insertbeforenode(addee, &onlist->t_listnode);
	aq->aq_count++;
}

void
allocqueue_remove(struct allocqueue *aq, struct coremap_entry *t)
{
	allocqueue_removenode(&t->t_listnode);
	DEBUGASSERT(aq->aq_count > 0);
	aq->aq_count--;
}
