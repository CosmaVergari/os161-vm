---
title: os161-suchvm
author: Francesco Capano (s284739), Cosma Alex Vergari (s284922)
date: Ciccio
---

Strategia: spiegare approfonditamente come funziona il demand paging e swapping e mostrando quale componente implementa quella funzionalità. 
Se c'è da precisare qualcosa su un componente si fa in un capitolo a parte.

# Theorical introduction

This is an implementation of the project 1 by G. Cabodi. It implements demand paging, swapping and provides statistics on the performance of the virtual memory manager. It completely replaces DUMBVM.

## On-demand paging
The memory is divided in pages (hence paging) and frames. Pages are indexed by a virtual address and each of them has a physical frame assigned.

The on-demand paging technique creates this correspondence page by page and only when a page is needed by a process. This event is triggered by the TLB.

# os161 process implementation
Per descrivere come os161 runna i processi e dove siamo intervenuti noi. Rimanda ai capitoli di addressed problems.

# Implementation
<!-- Qua parliamo di come abbiamo realizzato le funzionalità principali -->

# Addressed problems
Qua descriviamo i problemi che abbiamo risolto

# Tests
Che cosa fanno i test e che passano.

# Improvements
Non è stato implementato la capacità multi-processor.
Opzionale
