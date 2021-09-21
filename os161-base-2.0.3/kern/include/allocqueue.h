#ifndef ALLOCQUEUE_H
#define ALLOCQUEUE_H

#include <coremap.h>

struct allocqueue_node {
    struct allocqueue_node *aq_next;
    struct coremap_entry *cm_entry;
};
 
struct allocqueue {
    struct allocqueue_node *aq_head;
    struct allocqueue_node *aq_tail;
};

 /* Initialize and clean up a alloc queue node. */
void allocqueue_node_init(struct allocqueue_node *aqn, struct coremap_entry *self);
void allocqueue_node_cleanup(struct allocqueue_node *aqn);

/* Initialize and clean up an entire alloc queue. Must be empty at cleanup. */
struct allocqueue* allocqueue_init(void);
void allocqueue_cleanup(struct allocqueue *aq);

/* Add and remove: at ends */
void allocqueue_addtail(struct allocqueue *aq, struct coremap_entry *entry);
void allocqueue_remhead(struct allocqueue *aq);

#endif /* ALLOCQUEUE_H */