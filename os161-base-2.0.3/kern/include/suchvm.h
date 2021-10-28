#ifndef _SUCHVM_H_
#define _SUCHVM_H_

#include <vm.h>

/* Always have 72k of user stack
 * (this must be > 64K so argument blocks of size ARG_MAX will fit) */
#define SUCHVM_STACKPAGES 18

void suchvm_can_sleep(void);
void vm_bootstrap(void);
void vm_shutdown(void):

#endif