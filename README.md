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

- Usiamo tlb_write e tlb_read perchè dobbiamo fare round-robin nella TLB e ci serve **l'indice**



# 20/8/2021
## Eseguiamo testbin/palin e seguiamo il flow di esecuzione con il debugger

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

as_define_region aggiunge nella struttura addrspace quali sono i boundaries di un segmento di memoria **senza allocarla**. L'allocazione è eseguita da as_prepare_load() attraverso getppages()
as_complete_load() non fa un cazz
- entrypoint viene riassegnato in load_elf() una volta finita la lettura di tutti i segmenti

-  Nella struct addrspace invece c'è l'indirizzo fisico del primo frame dedicato allo stack e non l'indirizzo virtuale di partenza dello stack. Questo viene assegnato da as_define_stack() alla variabile *stackptr e sarà poi passato come argomento al processo in runprogram().
- enter_new_process() riempie il trap frame che verrà utilizzato per passare in user mode (interrupt)

- Usare testbin/sort per testare tutto

- All'uscita di un programma, viene chiamato thread_exit() da thread_startup()
- Dovremmo implementare la sys_exit() che dovrà chiamare proc_destroy() che a sua volta chiama as_deactivate() che dovrebbe deallocare le pagine.

- as_destroy() dealloca solo la struttura addrspace

- dumbvm contiene già una funzione che riceve l'interrupt dalla TLB ed è vm_fault(). Possiamo prendere spunto per i passi da fare. 

- Soluzione: page table nella struct addrspace implementata come array. L'array è indicizzato con il numero della pagina al quale si vuole accedere e contiene l'indirizzo iniziale del frame allocato. Se il frame non è allocato allora lo alloco e carico da file quello che serve.
- Nella struttura addrspace ci dobbiamo salvare anche l'offset di partenza dei diversi segmenti nel file perchè dobbiamo caricare dal file solo una pagina, ma all'interno del file i segmenti stanno sparsi.

# 24/8/2021

 /* Fault-type arguments to **vm_fault**() */
 #define VM_FAULT_READ        0    /* A read was attempted */
 #define VM_FAULT_WRITE       1    /* A write was attempted */
 #define VM_FAULT_READONLY    2    /* A write to a readonly page was attempted*/

 VM_FAULT_READONLY viene utilizzata quando avviene l'eccezione con codice EX_MOD



 PIANO D'AZIONE!!!!
 1) Definiamo come fare la page table : FATTO
 2) Capire come funziona kmalloc e kfree
 3) Capire cosa intende con segments : Sono i segments dell'ELF (insieme di pagine)
 4) IMplementare la page table per ogni processo (la struttura dati)
 5) Sistemare load_elf() (come popolare la struttura dati della page table)
 6) Capire come salvare nella TLB gli indirizzi
 7) IMplementare la vm_fault
 7.1) Controllare la page table del processo
 7.2) Caricare da file
 8) Swapfile (inculata)
 9) IMplementare il log dei dati

 Cosa salvare nella struttura segment? (os161-userprocess)
 * Each ELF segment describes a contiguous region of the virtual address space.
 * For each segment, the ELF file includes a segment image and a header, which describes:
    - the virtual address of the start of the segment
    - the length of the segment in the virtual address space
    - the location of the start of the image in the ELF file
    - the length of the image in the ELF file

| Text  |xxxx| Data |xxxx| Stack     |

2G   4 pagine
1G   2 pagine

| A |   | B |   |