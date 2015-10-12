/*
 * sys.c - Syscalls implementation
 */
#include <devices.h>

#include <utils.h>

#include <io.h>

#include <mm.h>

#include <mm_address.h>

#include <sched.h>

#include <zeos_interrupt.h> 

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
// TODO check something!!!
int sys_gettime() {
	/*if (zeos_ticks == 1000000) {
		task_switch(idle_task);
	}*/
	return zeos_ticks;
}

// Write system call Service routine
int sys_write(int fd, char *buffer, int size) {
	// fd: file descriptor. In this case --> always 1
	// buffer: pointer to the bytes to write.
	// size; number of bytes
	// return negative number in case of error (indicating error)
	// else --> return number of bytes write
	// errors
	// check >= 0 no error
	// buffer != null
	int check = check_fd(fd,ESCRIPTURA);
	if (check < 0) return check;
	if (buffer == 0) return -14; // EFAULT Bad address
	if (size < 0) return -1;

	//not the best approach!!! u have to char[30] and copy print while true
	//char *aux;
	// copy data from user to kernel
	//int err = copy_from_user(&buffer, &aux, size);
	char buf[64];
	int num = 64;
	if (size < num) num = size;
	int wr = 0;
	int res = 0;
	int err = 0;
	while (wr < size && res >= 0) {
		res = copy_from_user(buffer, buf, num);
		if (res < 0) return res;
		res = sys_write_console(buf,num);
		if (res >= 0) {
			wr = wr + res;
		}
	}
	if (res < 0) return err;
	else {
		//int num = sys_write_console(aux,size);
		return num;
	}
}


