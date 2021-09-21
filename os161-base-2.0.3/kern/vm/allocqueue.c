#include <types.h>
#include <lib.h>
#include <coremap.h>
#include <allocqueue.h>

struct allocqueue_node *allocqueue_node_init(struct coremap_entry *self, struct allocqueue_node *next)
{
    struct allocqueue_node *new_node;

    KASSERT(self != NULL);

    new_node = (struct allocqueue_node *)kmalloc(sizeof(struct allocqueue_node));
    new_node->aq_next = next;
    new_node->cm_entry = self;
    return new_node;
}

void allocqueue_node_cleanup(struct allocqueue_node *aqn)
{
    KASSERT(aqn != NULL);
    KASSERT(aqn->aq_next == NULL);
    KASSERT(aqn->cm_entry != NULL);

    kfree(aqn);

    // TODO free node
}

struct allocqueue *allocqueue_init(void)
{
    struct allocqueue *aq;

    aq = (struct allocqueue *)kmalloc(sizeof(struct allocqueue));
    if (aq == NULL)
    {
        panic("Could not allocate the allocation queue\n");
    }
    aq->aq_head = NULL;
    aq->aq_tail = NULL;
}

void allocqueue_cleanup(struct allocqueue *aq)
{
    KASSERT(aq != NULL);
    KASSERT(aq->aq_head.aqn_next == &aq->aq_tail);
    KASSERT(aq->aq_tail.aqn_next == NULL);

    kfree(aq);
}

void allocqueue_addtail(struct allocqueue *aq, struct coremap_entry *entry)
{
    struct allocqueue_node *new_tail;

    KASSERT(aq != NULL);
    KASSERT(entry != NULL);

    new_tail = allocqueue_node_init(entry, NULL);
    if (new_tail == NULL)
    {
        panic("Impossible to alloc a page for allocation queue");
    }

    if (aq->aq_tail == NULL) 
        aq->aq_head = new_tail;
    } else {
        aq->aq_tail->aq_next = new_tail;
    }
    aq->aq_tail = new_tail;
}

struct coremap_entry *allocqueue_remhead(struct allocqueue *aq)
{
    struct allocqueue_node *old_head;
    struct coremap_entry *entry;

    KASSERT(aq != NULL);

    old_head = aq->aq_head;
    aq -> aq_head = old_head->aq_next;

    old_head->aq_next = NULL;
    entry = old_head->cm_entry;
    allocqueue_node_cleanup(old_head);

    return entry;
}
