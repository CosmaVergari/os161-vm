/*
 * Copyright (c) 2009
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

#ifndef _allocqueue_H_
#define _allocqueue_H_


struct thread;	/* from <thread.h> */

/*
 * AmigaOS-style linked list of threads.
 *
 * The two allocqueuenodes in the allocqueue structure are always on
 * the list, as bookends; this removes all the special cases in the
 * list handling code. However, this means that iterating starts with
 * the "second" element in the list (aq_head.aqn_next, or
 * aq_tail.aqn_prev) and it ends at the last element that's actually a
 * thread.
 *
 * Note that this means that assigning or memcpying allocqueue
 * structures will break them. Don't do that...
 *
 * ->aqn_self always points to the thread that contains the
 * allocqueuenode. We could avoid this if we wanted to instead use
 *
 *    (struct coremap_entry *)((char *)node - offsetof(struct thread, t_listnode))
 *
 * to get the thread pointer. But that's gross.
 */

struct allocqueuenode {
	struct allocqueuenode *aqn_prev;
	struct allocqueuenode *aqn_next;
	struct coremap_entry *aqn_self;
};

struct allocqueue {
	struct allocqueuenode aq_head;
	struct allocqueuenode aq_tail;
	unsigned aq_count;
};

/* Initialize and clean up a thread list node. */
void allocqueuenode_init(struct allocqueuenode *aqn, struct coremap_entry *self);
void allocqueuenode_cleanup(struct allocqueuenode *aqn);

/* Initialize and clean up a thread list. Must be empty at cleanup. */
void allocqueue_init(struct allocqueue *aq);
void allocqueue_cleanup(struct allocqueue *aq);

/* Check if it's empty */
bool allocqueue_isempty(struct allocqueue *aq);

/* Add and remove: at ends */
void allocqueue_addhead(struct allocqueue *aq, struct coremap_entry *t);
void allocqueue_addtail(struct allocqueue *aq, struct coremap_entry *t);
struct coremap_entry *allocqueue_remhead(struct allocqueue *aq);
struct coremap_entry *allocqueue_remtail(struct allocqueue *aq);

/* Add and remove: in middle. (aq is needed to maintain ->aq_count.) */
void allocqueue_insertafter(struct allocqueue *aq,
			    struct coremap_entry *onlist, struct coremap_entry *addee);
void allocqueue_insertbefore(struct allocqueue *aq,
			     struct coremap_entry *addee, struct coremap_entry *onlist);
void allocqueue_remove(struct allocqueue *aq, struct coremap_entry *t);

#endif /* _allocqueue_H_ */
