#include <types.h>
#include <kern/unistd.h>
#include <clock.h>
#include <copyinout.h>
#include <syscall.h>
#include <lib.h>
#include <proc.h>
#include <thread.h>
#include <addrspace.h>

/*
 * Exit from current process
 */
void sys__exit(int status)
{
    /* Thread structure is nullified, proc_destroy and as_destroy are called*/
    thread_exit();

    panic("thread_exit returned (should not happen)\n");
    (void)status; // TODO: status handling
}