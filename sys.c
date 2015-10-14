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

extern struct list_head freequeue;
extern int freePID;

int check_fd(int fd, int permissions)
{
  if (fd!=1) return -9; /*EBADF*/
  if (permissions!=ESCRIPTURA) return -13; /*EACCES*/
  return 0;
}

int sys_ni_syscall() {
	return -38; /*ENOSYS*/
}

int sys_getpid() {
	return current()->PID;
}

int ret_from_fork() {
	return 0;
}

int sys_fork() {
	// Get a free task_struct for the process. If there is no space for a new 
	// process, an error will be returned.
	if (list_empty(&freequeue)) return -1;
	// Inherit system data: copy the parent’s task_union to the child. Determine 
	// whether it is necessary to modify the page table of the parent to access 
	// the child’s system data. The copy_data function can be used to copy.

	struct list_head * lh = list_first(&freequeue);
  	union task_union *tsku_fill = (union task_union*)list_head_to_task_struct(lh);
  	union task_union *tsku_current = (union task_union*)current();    
  	list_del(lh);

	struct list_head * child_lh = list_first( &freequeue );
 	list_del(child_lh);
	struct task_struct * child_ts = list_head_to_task_struct(child_lh);
 	struct task_struct * father_ts = current();
 	union task_union * child_tu = (union task_union*) child_ts;
 	union task_union * father_tu = (union task_union*) father_ts;
 	//union task_union *child_tu = (union task_union*)
 	copy_data(father_ts, child_ts, KERNEL_STACK_SIZE*4);
 	// Initialize field dir_pages_baseAddr with a new directory to store the 
 	// process address space using the allocate_DIR routine.
 	allocate_DIR(child_ts);
 	// Search physical pages in which to map logical pages for data+stack of the
 	// child process (using the alloc_frames function). If there is no enough free 
 	// pages, an error will be return.
 	int pag, aux;
 	int new_frames[NUM_PAG_DATA];
	for (pag=0; pag<NUM_PAG_DATA; pag++){
    	new_frames[pag] = alloc_frame();
    	if (new_frames[pag] == -1) {
      		for (aux = pag-1; aux >= 0; --aux) free_frame(new_frames[aux]);
      		return -12;  // ENOMEM 
    	}
  	}
  	// Inherit user data

  	// Create new address space: Access page table of the child process through 
  	// the directory field in the task_struct to initialize it (get_PT routine can be used):
	// A) Page table entries for the system code and data and for the user code can be a
	// copy of the page table entries of the parent process (they will be shared)
	// B) Page table entries for the user data+stack have to point to new allocated pages
	// which hold this region
	page_table_entry * taulaP_child = get_PT(child_ts);
	page_table_entry * taulaP_father = get_PT(father_ts);
	// Copy the user data+stack pages from the parent process to the child process. The
	// child’s physical pages cannot be directly accessed because they are not mapped in
	// the parent’s page table. In addition, they cannot be mapped directly because the
	// logical parent process pages are the same. They must therefore be mapped in new
	// entries of the page table temporally (only for the copy). Thus, both pages can be
	// accessed simultaneously as follows:
	// A) Use temporal free entries on the page table of the parent. Use the set_ss_pag and
	// del_ss_pag functions.
	// B) Copy data+stack pages.
	// C) Free temporal entries in the page table and flush the TLB to really disable the
	// parent process to access the child pages.
	/*for (pag=0; pag<NUM_PAG_DATA; pag++) {
		int frame = get_frame(taulaP_father, pag);
		set_ss_pag(taulaP_father, pag, frame);
	}*/
	int DATA_END = NUM_PAG_KERNEL + NUM_PAG_CODE + NUM_PAG_DATA + 1; // +1 perquè comencem en 0 la pag
	for (pag=0; pag<NUM_PAG_DATA; pag++) {
		set_ss_pag(taulaP_father, DATA_END+pag, new_frames[pag]);
		set_ss_pag(taulaP_child, PAG_LOG_INIT_CODE+pag, new_frames[pag]);
		copy_data((int*)((DATA_END+pag)*PAGE_SIZE),(int*)((PAG_LOG_INIT_CODE+pag)*PAGE_SIZE),PAGE_SIZE);
		del_ss_pag(taulaP_father,DATA_END+pag);
	}
	// Assign a new PID to the process. The PID must be different from its position in the
	// task_array table.
 	int child_pid = freePID;
 	child_ts->PID = child_pid;
 	freePID = freePID + 1;
 	// Initialize the fields of the task_struct that are not common to the child.
	// Think about the register or registers that will not be common in the returning of the
	// child process and modify its content in the system stack so that each one receive its
	// values when the context is restored.
 	// ?????? no fa falta no????
 	// h) Prepare the child stack with a content that emulates the result of a call to task_switch.
	// This process will be executed at some point by issuing a call to task_switch. Therefore
	// the child stack must have the same content as the task_switch expects to find, so it will
	// be able to restore its context in a known position. The stack of this new process must
	// be forged so it can be restored at some point in the future by a task_switch. In fact this
	// new process has to a) restore its hardware context and b) continue the execution of the
	// user process, so you must create a routine ret_from_fork which does exactly this. And
	// use it as the restore point like in the idle process initialization 4.4.
	//union task_union * child_tu = (union task_union*) child_ts; 	//   |              |
	int addr_esp = (KERNEL_STACK_SIZE-19); 						  	//-19|	  0       | (serveix per que el kernel stack apunti a aquesta direccio emulant )              |
	int addr_ret_from_fork = (KERNEL_STACK_SIZE-18);				//-18|@ret_from_fork|
 	child_tu->stack[addr_esp] = 0;									//-17|   @handler   |
 	child_tu->stack[addr_ret_from_fork] = &ret_from_fork;			//   |11 de save all|
 	child_ts->kernel_stack = child_tu->stack[addr_esp];				//   |5  de reg hw  |(ss,esp,flags,cd,eip) |
 	// i) Insert the new process into the ready list: readyqueue. This list will contain all processes
	// that are ready to execute but there is no processor to run them.
 	list_add_tail(child_lh, child_ts);
 	// j) Return the pid of the child process.
	return child_pid;
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


