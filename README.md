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


## thread_startup()
E' la funzione che viene chiamata per un context switch, questa funzione a sua volta chiama:
- as_activate()
- exorcise()
- entrypoint()
- thread_exit()

# Cosa succede quando si crea un processo
- runprogram() (kern/syscall) chiama as_create() (kern/mips/vm/dumbvm ma per noi sarà addrspace) che crea una struttura addrspace e la azzera
- runprogram() chiama as_activate() (dumbvm cancella dalla tlb tutte le associazioni presenti precedentemente)
- runprogram() chiama load_elf()
- load_elf() chiama as_define_region()  

Variabili utili: PAGE_FRAME 0xfffff000   global mask for getting page number from addr definita in vm.h
                 PAGE_SIZE  0x00001000   dimensione di una pagina

Passi di as_define_region() per ottenere il numero di pagine:
1. (iniziale)                   |   xxxxx|xxxxxxxx|xxx    |        |
2. sz += vaddr & ~PAGE_FRAME;   |222xxxxx|xxxxxxxx|xxx    |        | => tiene conto del fatto che l'indirizzo di partenza non è allineato all'inizio della pagina quindi le pagine da richiedere sono di più per accomodare la parte finale di spazio richiesto
3. sz = (sz + PAGE_SIZE - 1);   |111xxxxx|xxxxxxxx|xxx2222|222     | => aggiungo una pagina perchè se facessi il passo 4 dopo il passo 2, otterrei 2 pagine invece di 3 (al passo 5)
4. sz = sz & PAGE_FRAME;        |111xxxxx|xxxxxxxx|xxx2222|        |
5. sz = sz / PAGE_SIZE;                                              => Ottengo il numero di pagine (divisione esatta senza resto)

