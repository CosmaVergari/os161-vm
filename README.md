# os161-vm

- Dobbiamo modificare addrspace.c in kern/vm/
- Dobbiamo modificare addrspace.h in kern/include per contenere qualche cosa della page table
- Dove mettiamo la page table?

Page table è associata ad un processo, la mettiamo dentro addrspace struct?

- Conviene mettere l'implementazione della VM in addrspace.c e non in files a parte (perchè os161 chiama i metodi come definiti in addrspace.c)
- Potremmo spostare questi metodi?

- Il kernel non ha bisogno di paging perchè è monolitico, bosmen

- Parte del load_segment() contenuto in /kern/syscall/loadelf.c va inserita nella syscall della page fault della TLB, per caricare la pagina da file

- Possibile soluzione: in struct addrspace ci creiamo un array di pagine per ogni segmento dell ELF, l'array è lungo numero di byte del segmento / grandezza pagina. Per ogni elemento dell'array dobbiamo tenere traccia dell'associazione con un frame.

FLOW di accesso alla memoria di un programma:
programma fa accesso -> va alla tlb -> tlb miss -> leggo la page table (array in addrspace struct) -> se valore nell'array è NULL (esempio) -> carico da file la pagina in una pagina libera -> policy per trovare pagina libera

- Per trovare una pagina libera si tiene una free list con inserimento in testa quando si libera una pagina, ed estrazione in testa quando si vuole occupare una pagina. (non usiamo solo una variabile perchè bisogna tenere traccia delle pagine libere)

- Usiamo tlb_write e tlb_read perchè dobbiamo fare round-robin nella TLB e ci serve l'indice
