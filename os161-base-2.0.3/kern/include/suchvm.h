#ifndef _SUCHVM_H_
#define _SUCHVM_H_

void suchvm_can_sleep(void);
void vm_tlbshootdown(const struct tlbshootdown *ts);
int vm_fault(int faulttype, vaddr_t faultaddress);

#endif