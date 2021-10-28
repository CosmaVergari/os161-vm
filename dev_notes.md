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

Dubbi Cosma:
- La page table ha dimensione fissa?
- L'heap è grande 16M, basta per tutti i processi e page tables?
- Tool per visualizzare la memoria in os161


- La funzione kmalloc() chiama alloc_npages che è in dumbvm -> va modificata.
- alloc_npages chiede pagine a muzzo, serve una gestione ad un livello più basso: coremap e basta


Descrizione delle funzioni che dobbiamo implementare in addrspace.c (preso da addrspace.h):

     as_create - create a new empty address space. You need to make
                 sure this gets called in all the right places. You
                 may find you want to change the argument list. May
                 return NULL on out-of-memory error.
 
     as_copy   - create a new address space that is an exact copy of
                 an old one. Probably calls as_create to get a new
                 empty address space and fill it in, but that's up to
                 you.
 
     as_activate - make curproc's address space the one currently
                 "seen" by the processor.
 
     as_deactivate - unload curproc's address space so it isn't
                 currently "seen" by the processor. This is used to
                 avoid potentially "seeing" it while it's being
                 destroyed.
 
     as_destroy - dispose of an address space. You may need to change
                 the way this works if implementing user-level threads.
 
     as_define_region - set up a region of memory within the address
                 space.
 
     as_prepare_load - this is called before actually loading from an
                 executable into the address space.
 
     as_complete_load - this is called when loading from an executable
                 is complete.
 
     as_define_stack - set up the stack region in the address space.
                 (Normally called *after* as_complete_load().) Hands
                 back the initial stack pointer for the new process.


- La pagina che verrà rimpiazzata quando non c'è più spazio sarà una pagina possibilmente anche
di un processo diverso da quello che sta richiedendo la pagina.
- Siccome il page replacement verrà gestitò da coremap, conviene tenere in coremap per ogni pagina fisica un riferimento a quale processo appartiene, in modo da poter modificare le page tables del processo vittima.


Updates:
- coremap deve gestire anche lo swap in e out da file -> in parte, l'altra parte lo fa swap.c
- Dobbiamo tenere la allocSize salvata
- entry_type gestisce tutto


# 11/9/2021
Aggiornamenti:
- coremap chiama ram_stealmem quando ha bisogno di memoria ram. Non viene gestito il caso in cui addr == 0, ossia quando non c'è più ram disponibile.

Domanda: ma quando tutte le pagine sono occupate chi salva su disco coremap o suchvm? -> coremap

Nota: bisogna tenersi l'indice dell'ultimo elemento salvato nella tlb per capire dove salvare l'indirizzo della nuova pagina nella TLB. Al context switch forse questo contatore dev'essere resettato? -> No
Nota: free list al posto di tenere un array di pagine libere ed occupate nella coremap?

- Non si fa lo swap delle pagine del kernel

- TODO: Rimandiamo caricare la prima pagina dell'entrypoint a dopo
- TODO: Implementare la syscall exit per fare la destroy dell'address space del processo.

- Address error on load?? quando chiama VOP_READ dopo aver allocato una pagina
Definizione su os161 harvard sito: ADEL -- address error on load. An address error is either an unaligned access or an attempt to access kernel memory while in user mode. Software should respond by treating the condition as a failure. 
- Siccome l'indirizzo virtuale è nel segmento user (<0x80000000), allora il problema è un accesso non allineato

- NOTA: vfs_close in runprogram va tolto

- Problema: viene fatto un page fault su un indirizzo virtuale che è appena fuori dall'indirizzo max di un segmento (e.g. 0xa0 su un max di 0x9f) e vm_fault ritorna un EFAULT. Non dovrebbe proprio fare questo accesso giusto?

Updates 14/9 (Cosma):
- seg_get_paddr non si può separare da load_page perchè load_page va chiamata solo quando la pagina non è popolata nella pagetable
- il setup di iov_ubase in load_page va differenziato nei tre casi
- La copia della pagina dal buffer in kernel nella pagina dell'utente non va a buon fine (viene chiamata copyfail() definita in copyinout.c)

# 14/9/2021
TLB flow:
Quando sta caricando la prima pagina)
1. TLB miss sull'entrypoint
2. Page table miss in vm_fault()
3. Avvia il caricamento della pagina da file con emufs_doread()
4. Il thread viene deschedulato finchè non ha letto la pagina da file
5. thread_switch() viene chiamato al ritorno in esecuzione
6. thread_switch() chiama as_activate()
7. as_Activate() svuota la TLB
8. uiomove() per copiare la pagina dal kernel buffer all'indirizzo user level (non funziona per ora) causa un'altra TLB miss
9. TLB miss viene risolta leggendo la page table, quindi senza caricare da file, quindi senza chiamare as_activate, quindi senza svuotare la TLB
fine

La TLB quando memcpy copia in memoria quella che emufs_doread() ha letto in un kernel buffer da file, causa una TLB miss in un'area di memoria che è readonly, quindi la TLB si incazza e blocca la copia.
- Soluzione: Fare degli stati in cui la vm tenga traccia di quale miss stiamo (orribile), far fare la write anche in una pagina Read only se è il kernel a farla
Soluzioni?
- Mettere temporaneamente il RW in quella pagina finchè non la carica: far impostare la tlb alla funzione che carica la pagina (ma come?)
- Caricare la pagina prima nel kernel e poi fare una memmove nell'indirizzo fisico che è già noto. -> SOLUZIONE ADOTTATA!

TODO: Implementare syscall exit e swapping
TODO: Spostare riferimento elf node in as (opzionale)

# 17/9/2021 Il paging funziona

# 18/9 Il paging non funziona
- La variabile palindrome nel file di test palin è lunga 8000 char ma nel programma sys_write viene chiamata solo con 4096 caratteri. Perchè?
- Le chiamate alle printf dove c'è solo testo vengono tradotte in assembly come chiamate a *puts*, mentre quella con la variabile palindrome viene tradotta in *printf*. Quella con printf di palindrome è l'unica che funziona e le chiamate a sys_write causate da puts invece sono con lunghezza 0 o 1 a occhio. Il puntatore che riceve sys_write dovrebbe essere giusto, è solo sbagliata la lunghezza. Problema di passaggio parametri? -> RISOLTO (problema nella gestione degli indirizzi fisici in seg_load_page)

# 19/9
- Nel programma palin, viene stampata solo la prima pagina (4096 caratteri) della variabile palindrome e il test della palindromia infatti fallisce.
- Dobbiamo caricare tutte le pagine dei segmenti data? Mi sembra strano.

Problema: La prima volta che c'è una vm_fault in seg2 è sulla 2° pagina del segmento. La 1° pagina di seg2 contiene i primi 4096 byte della variabile palindrome. I restanti byte della variabile sono nella 2° pagina di seg2 che quindi dovrebbe essere caricata. Il fatto è che questa vm_fault è di tipo write, quindi per come è scritta ora la vm_fault non esegue la seg_load_page() e quindi non carica mai la pagina da file!!! (quindi strlen ritorna 4096, quindi stampa solo i primi 4096 bytes)


# 20/9 SWAPFILE

Flow SWAP OUT 
- vm_fault
- si deve salavre il p_addr nella pagetable e scrivere la pagina in memoria...
- nessun p_addr disponibile
- serve lo swap out di una pagina 
- viene ritornato il p_addr liberato 
- la pagina che ci serviva viene salvata a quel p_addr
- la pagina dello swap out viene salvata nello swapfile ma dovrebbe essere invalidata la entry nella pt per far
    in modo che quando venga fatto lo swap in si sappia che quella entry è stata vittima dello swap out

Flow SWAP IN 
- la pagina richiesta ha già un p_addr salvato nella pagetable (*Nota Cosma: Secondo me p_addr non serve più nella pagetable perchè verrà assegnato un nuovo frame quando si fa lo swap in*)
- nella entry della page_table corrispondente, si nota che la pagina è stata vittima di uno swap out
- si carica la pagina in memoria dallo swap_file ( potrebbe avvenire un nuovo swap out se non ci sono abbastanza pagine)


CONTESTO IN CUI AVVIENE LO SWAP OUT
- Tutte le pagine sono state assegnate
    - Il risultato della steal_mem è nessuna pagina libera  --> dopo aver provato con le pagine della coremap
    - La coremap è al completo

PROBLEMI 
- La pagina che fa swap in deve essere ricaricata allo stesso p_addr in cui era prima dello swap_out? (*Nota Cosma: secondo me no*)
- Tutti gli as dovrebbero tener traccia dello swapfile
    - Si tiene il puntatore al file aperto dentro il file swapfile.c? (*Nota Cosma: Secondo me qui*)
    - Si tiene il puntatore al file aperto dentro suchvm.c?
    - Quando il file deve essere aperto? --> Suppongo quando venga attivata la coremap? (*Nota Cosma: Sì in teoria quando viene chiamata vm_bootstrap()*)
    - Ogni volta lo swapfile va aperto e chiuso? (*Nota Cosma: Secondo me è sempre aperto, oppure è chiuso finchè non si verifica il primo swap out*)
- Siccome potrebbe esserci un context switch serve che l'operazione di swap in - swap out sia  
    - protetta da un lock (*Nota Cosma: Secondo me lock, però dobbiamo fare attenzione ai deadlock, perchè se poi un altro processo tenta di fare swap out, probabile perchè la memoria è piena per tutti i processi, e tenta di acquisire il lock siamo fregati. Forse un **wait channel** ?*)
                -- o --
    - disattivando gli interrupt  --> questa funzionerebbe su nostra implemetazione poichè è a singola cpu


ROBA DA MODIFICARE per SWAP OUT
- chiamata allo swap out in coremap.c in getppage_user
- nel file swapfile.c funzione swap_out 
    - identifica la pagine per lo swap_out  (possiamo utilizzare un round robin come nella tlb o anche fare swap out di una pagina random) (*Nota Cosma: Secondo me conviene round robin*)
    - utilizzando un file diverso per la vittima ci sono due soluzioni :
        - Passare la coremap e identificare la vittima dentro la swap_out --> quindi ritornare il paddr
        - Passare direttamente la vittima alla swap out ( più semplice) ( sotto forma di coremap entry) (*Nota Cosma: Secondo me questa soluzion, swapfile.c secondo me tiene traccia di quali pagine su disco sono libere e quali sono occupate, la corrispondenza vaddr e offset nello swapfile è contenuta in pt o coremap_entry*)
            - si dovrebbe tenere una variabile per il round robin come nella tlb
            - la swap out si occuperebbe solo del salvataggio nella pagina vittima nel file
            - in questo caso l'invalidazione della entry nella pt potrebbe essere fatta da :
                - swapout --> visto che abbiamo la coremap entry --> dobbiamo ricavarci il segment e invalidare la relativa entry nella pt
                - dentro coremap facendo la stessa operazione  ( forse la prima soluzione è più pulita)

ROBA DA MODIFICARE per SWAP IN
- nella pt servirebbe una flag per indicare che la relativa pagina è stata vittima di uno swap out
- si chiama la funzione swap in a cui si passa il vaddr
    - la chiamata può avvenire in diversi punti:
        - in pt.c nella funzione pt_get_entry in cui si fa un controllo se c'è stato lo swap_out (*Nota Cosma: Secondo me qui perchè la gestione dello swap credo che convenga tenerla a un livello di astrazione più basso*)
        - in suchvm.c  nella funzione vm_fault in cui si controlla se la pagina è stata vittima di swap out precedentemente grazie al flag impostato
- la funzione swap in deve ricaricare la pagina in memoria 
    -( dovrebbe riusare le funzioni per la ricerca di spazio in memoria e quindi potrebbe riavvenire uno swap out)
- quando si ritorna la entry nella pt dovrebbe essere di nuovo valida
- la entry nello swapfile soggetta a swap in
    - viene eliminata dallo swap file ??? 
    - viene tenuta ma invalidata ed in caso di swap out si controllano tutte le entry invalidate ??? (*Nota Cosma: Secondo me sì, cioè nelle strutture dati di swapfile.c che tengono traccia di quali pagine nello swapfile sono piene bisogna impostare la flag su free*)



CONTESTO IN CUI AVVIENE LO SWAP PT.2
- spazio della memoria virtuale di un singolo processo più grande dello spazio fisico 
- dopo context switch lo spazio virtuale di un secondo processo è più grande dello spazio fisico disponibile (*Nota Cosma: Finchè lo spazio virtuale è più grande non è un problema, il problema è quando tutte le pagine di questo spazio virtuale vengono utilizzate e quindi devono essere caricate dal demand paging, però penso sia improbabile*)

- Ora è necessario che lo swap out venga fatto con pagine dello stesso processo o vanno bene anche le pagine di altri processi? (*Nota Cosma: Bella domanda, credo anche di altri processi perchè se la memoria è già piena quando un processo carica la sua prima pagina non si avvierà mai*)
    - Nel primo caso siamo sempre nello stesso as 
    - Nel secondo l'as viene ricavato dalla coremap entry


DUBBI DALLA TRACCIA 
- E' riportato This file will be limited to 9 MB (i.e., 9 * 1024 * 1024 bytes).
    - Ma ogni pagina non ha dimensione 4096 ??? (*Nota Cosma: Sisi, non penso c'entri molto quello che dice nella dimensione*)
    - Dobbiamo salvare solo la coremap entry ???
        - in questo caso se si trova il vaddr che si cercava per vedere se è giusto fare swap in si controlla che appartenga all'as corretto (*Nota Cosma: Secondo me sì*)


Idea: implementare memory mapping di un file e utilizzare vop_mmap per mappare lo swap file in memoria

Cambio idee:
- struct allocqueue simile a threadlist che contenesse la lista di nodi che sono stati allocati
- numero in coremap che va in ordine crescente in ordine cronologico
- allocqueue in coremap implementata come indici nelle singole coremap_entry -> IMPLEMENTATA

La allocqueue è implementata con gli indici all'interno delle coremap_entry (next_allocated, prev_allocated). Ogni entry contiene due indici: uno che punta alla entry che è stata allocata prima di quella considerata e uno che punta alla entry che è stata allocata dopo.

Problemi:
* testbin/bloat si blocca con TLB miss on store (potrebbe essere perchè non abbiamo implementato la sbrk ancora?) => Inutilizzabile se non si implementa l'allocazione dinamica della memoria
* testbin/zero fallisce perchè il blocco .bss non è azzerato, ma quindi questo test prevede che il **contenuto** del blocco .bss sia azzerato e non solo i contorni del segmento? => Esatto ma questo viene fatto da exec che non abbiamo implementato.

Test adatti: **testbin/huge**, **testbin/mat**

Problema: huge non funziona: TLB miss on store quando cerca di scrivere in un'area di memoria che probabilmente non è nella page table
Questo è anticipato da un messaggio "segments.c - short read from file, truncated?". Bisogna debuggare vm_fault.

Problema: file size non viene passata correttamente da load_elf a as_define region
TODO: Passare il corretto file size
TODO: Riempire di zeri le regioni di memoria con memsize > filesize

TODO: Aggiornare documentazione seg_load_page

Problema: ci sono molti page fault sulle stesse pagine => Causato dalla syscall write che mentre scrive su console deschedula il processo e al ritorno chiama la thread_switch che svuota la tlb.

Problema: bordello nella coremap dopo lo swapout della pagina 0x7e000
Test: vedere come funziona la lista nella coremap quando rimpiazza una pagina dopo lo swap


# Update: 26/10
Si è rotto tutto.
testbin/sort non funziona, a un certo punto il faultaddress diventa 0x0
testbin/sort segments:
 - code: 0x400000-0x403000
         filesize: 10112
         fileoffset: 0
         basevaddr = 0x400000
         npages=3
- data: 0x412000-0x533000
         filesize: 176
         fileoffset: 10112
         basevaddr = 0x412000
         npages=289
- stack: 0x7ffee000-0x80000000
         filesize: 0
         fileoffset: 0
         basevaddr = 0x7ffee000
         npages=18


page_faults
0x400180 => code
0x412838 => data
0x400198 => code
0x7fffffe8 => stack


Capito il problema: dentro as_define_region allineavamo l'indirizzo virtuale alla pagina, non va bene perchè una pagina virtuale può iniziare a metà della pagina fisica (com'è nel caso di sort). Infatti in sort caricava la pagina dei dati nel punto sbagliato e per questo sfanculava tutto perchè non riusciva a controllare una condizione per andare avanti.

Per sistemare il problema non possiamo più avere una granularità della lunghezza di un segmento a livello n° di pagine ma a livello memsize, quindi dobbiamo cambiare la struttura segment per tenere traccia della memsize. 

TODO: Dobbiamo rivedere anche il caricamento della pagina (seg_load_page)

Cambio page table per convertire gli indirizzi delle pagine in indici con indirizzi page-aligned
Problema: nella funzione getppage_user quando viene fatto lo swap out, si determina il segmento della pagina *vittima* attraverso la funzione as_find_segment. Il problema è che la funzione as_find_segment vuole un indirizzo virtuale, noi lo prendiamo dalla coremap dove sono tutti page_aligned, e quindi se un segmento non ha un base_vaddr che è page_aligned (inizia a metà pagina) allora ritorna un errore. 

Soluzioni:
- togliere controllo sui boundary precisi e farli sulle pagine all'interno di as_find_segment
- creare una funzione simile ad as_find_segment ma che controlla sulle pagine
- salvare nella coremap i base_vaddr (Mi sembra una minchiata)

TODO: Controllare se i limiti dichiarati dei segmenti si sovrappongono (e.g. grande segmento data che oltrepassa i limiti di stack)

Gli overflow delle variabili passate da elf li controlliamo?