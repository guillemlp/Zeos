/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#define LECTURA 0
#define ESCRIPTURA 1

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_ni_syscall()
{
	return -38; /*ENOSYS*/
}

int sys_getpid()
{
	return current()->PID;
}

int sys_fork()
{
  int PID=-1;

  // creates the child process
  
  return PID;
}

void sys_exit()
{  
}

// Write system call Service routine
int sys_write(int fd, char *buffer, int size) {
	// fd: file descriptor. In this case --> always 1
	// buffer: pointer to the bytes to write.
	// size; number of bytes
	// return negative number in case of error (indicating error)
	// else --> return number of bytes write
	int check = check_fd(fd,ESCRIPTURA);
	if (check < 0) {
		return check;
	}
	//check >= 0 no error
	if (buffer == 0) {
		return -1; //?? inventat
	}
	// buffer != null
	if (size < 0) {
		return -1;
	}
	char *aux;
	// copy data from user to kernel
	int err = copy_from_user(&buffer, &aux, size);
	if (err == -1) {
		return err;
	}
	else {
		int num = sys_write_console(aux,size);
		return num;
	}
}
