/*
 * Limited to std file pointer, no files.
 */
#include <types.h>
#include <kern/unistd.h>
#include <kern/errno.h>
#include <clock.h>
#include <syscall.h>
#include <current.h>
#include <lib.h>

/*Definition of userptr in types.h
 *
 *struct __userptr { char _dummy; };
 *typedef struct __userptr *userptr_t;
 */

/*
 * typed for stdin,stdout and stderr defind in kern/unistd.h
 */

int sys_read(int fd, userptr_t buf, size_t size){
	char *b = (char *) buf;
	int i;
	if( fd!= STDIN_FILENO){
/*#if OPT_FILE
    	return file_read(fd, buf, size);
#else*/
    	kprintf("sys_read supported only to stdin\n");
    	return -1;
//#endif
	}

	for ( i=0;i< (int)size ; i++ ){
		b[i] = getch();
		if (b[i] < 0)
			return i;
	}

	return (int)size;
}

int sys_write(int fd, userptr_t buf, size_t size){
	char *b = (char *)buf;

	if( fd != STDOUT_FILENO  &&	 fd != STDERR_FILENO ){
/*#if OPT_FILE
    	return file_write(fd, buf, size);
#else*/
    	kprintf("sys_read supported only to stdin\n");
    	return -1;
//#endif
	}

	for (int i=0;i < (int)size;i++)
		putch(b[i]);

	return (int) size;
}