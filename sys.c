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
extern struct list_head readyqueue;
extern int freePID;
extern struct task_struct * fill_task; 

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
	struct list_head *child_lh = NULL;
	union task_union *child_tu;
	union task_union *father_tu;
	struct task_struct *child_ts;
	struct task_struct *father_ts;

	if (list_empty(&freequeue)) return -1;
	// obtenim list_head lliure
	child_lh = list_first(&freequeue);
 	list_del(child_lh);
 	// inicialitzacio task struct i task union
	child_ts = list_head_to_task_struct(child_lh);
	child_tu = (union task_union*) list_head_to_task_struct(child_lh);
 	father_ts = current();
 	father_tu = (union task_union*) current();
 	// necesari per mi
 	fill_task = child_ts;

 	// copy data father child
 	copy_data(father_tu, child_tu, sizeof(union task_union));
 	
 	// new page directory
 	allocate_DIR(child_ts);

 	page_table_entry *child_PT = get_PT(child_ts);
	page_table_entry *father_PT = get_PT(father_ts);

 	// Allocate page for DATA+STACK
 	int new_frame, pag, i;
 	for (pag=0; pag<NUM_PAG_DATA; pag++) {
 		new_frame = alloc_frame();
 		if (new_frame != -1) {
 			set_ss_pag(child_PT, PAG_LOG_INIT_DATA+pag, new_frame);
 		}
 		else { // no free pages petar tot
 			for (i=0; i<pag; i++) {
 				free_frame(get_frame(child_PT,PAG_LOG_INIT_DATA+i));
 				del_ss_pag(child_PT, PAG_LOG_INIT_DATA+i);
 			}
 			list_add_tail(child_lh, &freequeue);
 			return -12;
 		}
 	}

	// copiem system
	for (pag=0; pag<NUM_PAG_KERNEL; pag++) {
		set_ss_pag(child_PT,pag,get_frame(father_PT,pag));
	}
	
	// copiem codi
	for (pag=0; pag<NUM_PAG_CODE; pag++) {
		set_ss_pag(child_PT,PAG_LOG_INIT_CODE+pag,get_frame(father_PT,PAG_LOG_INIT_CODE+pag));
	}
	
	for (pag=NUM_PAG_KERNEL+NUM_PAG_CODE; pag<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; pag++) {
	    set_ss_pag(father_PT, pag+NUM_PAG_DATA, get_frame(child_PT, pag));
	    copy_data((void*)((pag)*PAGE_SIZE), (void*)((pag+NUM_PAG_DATA)*PAGE_SIZE), PAGE_SIZE);
	    del_ss_pag(father_PT, pag+NUM_PAG_DATA);
  	}		
	set_cr3(father_ts->dir_pages_baseAddr);

	// Assign a new PID to the process. The PID must be different from its position in the
	// task_array table.
 	int child_pid = freePID;
 	child_ts->PID = child_pid;
 	freePID = freePID + 1;

 	child_tu->stack[KERNEL_STACK_SIZE-18] = &ret_from_fork;
  	child_tu->stack[KERNEL_STACK_SIZE-19] = 0;
  	child_ts->kernel_stack = &child_tu->stack[KERNEL_STACK_SIZE-19];

	list_add_tail(&(child_ts->list), &readyqueue);

	return child_pid;
}

int sys_fork2() {
	// Get a free task_struct for the process. If there is no space for a new 
	// process, an error will be returned.
	if (list_empty(&freequeue)) return -1;
	// Inherit system data: copy the parent’s task_union to the child. Determine 
	// whether it is necessary to modify the page table of the parent to access 
	// the child’s system data. The copy_data function can be used to copy.

	struct list_head * child_lh = list_first( &freequeue );
 	list_del(child_lh);

	struct task_struct * child_ts = list_head_to_task_struct(child_lh);
 	struct task_struct * father_ts = current();
 	union task_union *father_tu = (union task_union*) current();
 	union task_union *child_tu = (union task_union*) list_head_to_task_struct(child_lh);
 	
 	fill_task = child_ts;
 	
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
	for (pag=0; pag<NUM_PAG_KERNEL + NUM_PAG_CODE; pag++) {
		int frame = get_frame(taulaP_father, pag);
		set_ss_pag(taulaP_child, pag, frame);
	}
	
	int END = NUM_PAG_KERNEL + NUM_PAG_CODE + NUM_PAG_DATA + 1; // +1 perquè comencem en 0 la pag
	for (pag=0; pag<NUM_PAG_DATA; pag++) {
		set_ss_pag(taulaP_child, NUM_PAG_KERNEL + NUM_PAG_CODE + pag, new_frames[pag]);
		set_ss_pag(taulaP_father, END + pag, new_frames[pag]);
		copy_data((void*)((NUM_PAG_KERNEL + NUM_PAG_CODE + pag)*PAGE_SIZE),(void*)((END + pag)*PAGE_SIZE),PAGE_SIZE);
		del_ss_pag(taulaP_father,END + pag);
	}
	set_cr3(father_ts->dir_pages_baseAddr);
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
 	child_ts->kernel_stack = &child_tu->stack[addr_esp];				//   |5  de reg hw  |(ss,esp,flags,cd,eip) |
 	// i) Insert the new process into the ready list: readyqueue. This list will contain all processes
	// that are ready to execute but there is no processor to run them.
 	list_add_tail(child_lh, child_ts);
 	// j) Return the pid of the child process.
	return child_pid;
}

int sys_fork3() {
	// Get a free task_struct for the process. If there is no space for a new 
	// process, an error will be returned.
	if (list_empty(&freequeue)) return -1;
	// Inherit system data: copy the parent’s task_union to the child. Determine 
	// whether it is necessary to modify the page table of the parent to access 
	// the child’s system data. The copy_data function can be used to copy.
	struct list_head * child_lh = list_first( &freequeue );
 	list_del(child_lh);
	union task_union *uchild = (union task_union*) list_head_to_task_struct(child_lh);
	union task_union *ufather = (union task_union*) current();
 	fill_task = &uchild->task;
	//struct task_struct * child_ts = list_head_to_task_struct(child_lh);
 	//struct task_struct * father_ts = current();
 	copy_data(ufather, uchild, sizeof(union task_union));
 	// Initialize field dir_pages_baseAddr with a new directory to store the 
 	// process address space using the allocate_DIR routine.
 	allocate_DIR((struct task_struct*)uchild);
 	// Search physical pages in which to map logical pages for data+stack of the
 	// child process (using the alloc_frames function). If there is no enough free 
 	// pages, an error will be return.
	// Allocate new pages

	 /* Allocate pages for DATA+STACK */
  int new_ph_pag, pag, i;
  page_table_entry *process_PT = get_PT(&uchild->task);
  for (pag=0; pag<NUM_PAG_DATA; pag++)
  {
    new_ph_pag=alloc_frame();
    if (new_ph_pag!=-1) /* One page allocated */
    {
      set_ss_pag(process_PT, PAG_LOG_INIT_DATA+pag, new_ph_pag);
    }
    else /* No more free pages left. Deallocate everything */
    {
      /* Deallocate allocated pages. Up to pag. */
      for (i=0; i<pag; i++)
      {
        free_frame(get_frame(process_PT, PAG_LOG_INIT_DATA+i));
        del_ss_pag(process_PT, PAG_LOG_INIT_DATA+i);
      }
      /* Deallocate task_struct */
      list_add_tail(child_lh, &freequeue);
      
      /* Return error */
      return -12; 
    }
  }
/* Copy parent's SYSTEM and CODE to child. */
  page_table_entry *parent_PT = get_PT(current());
  for (pag=0; pag<NUM_PAG_KERNEL; pag++)
  {
    set_ss_pag(process_PT, pag, get_frame(parent_PT, pag));
  }
  for (pag=0; pag<NUM_PAG_CODE; pag++)
  {
    set_ss_pag(process_PT, PAG_LOG_INIT_CODE+pag, get_frame(parent_PT, PAG_LOG_INIT_CODE+pag));
  }
  /* Copy parent's DATA to child. We will use TOTAL_PAGES-1 as a temp logical page to map to */
  for (pag=NUM_PAG_KERNEL+NUM_PAG_CODE; pag<NUM_PAG_KERNEL+NUM_PAG_CODE+NUM_PAG_DATA; pag++)
  {
    /* Map one child page to parent's address space. */
    set_ss_pag(parent_PT, pag+NUM_PAG_DATA, get_frame(process_PT, pag));
    copy_data((void*)(pag<<12), (void*)((pag+NUM_PAG_DATA)<<12), PAGE_SIZE);
    del_ss_pag(parent_PT, pag+NUM_PAG_DATA);
  }
  /* Deny access to the child's memory space */
  set_cr3(get_DIR(current()));
  int child_pid = freePID;
 	//child_ts->PID = child_pid;
 	freePID = freePID + 1;
  uchild->task.PID=child_pid;
  //uchild->task.state=ST_READY;
  
  int register_ebp;		/* frame pointer */
  /* Map Parent's ebp to child's stack */
  __asm__ __volatile__ (
    "movl %%ebp, %0\n\t"
      : "=g" (register_ebp)
      : );
  register_ebp=(register_ebp - (int)current()) + (int)(uchild);

  uchild->task.kernel_stack=register_ebp + sizeof(DWord);
  
  DWord temp_ebp=*(DWord*)register_ebp;
  /* Prepare child stack for context switch */
  uchild->task.kernel_stack-=sizeof(DWord);
  *(DWord*)(uchild->task.kernel_stack)=(DWord)&ret_from_fork;
  uchild->task.kernel_stack-=sizeof(DWord);
  *(DWord*)(uchild->task.kernel_stack)=temp_ebp;

  /* Set stats to 0 */
  //init_stats(&(uchild->task.p_stats));

  /* Queue child process into readyqueue */
  //uchild->task.state=ST_READY;
  list_add_tail(&(uchild->task.list), &readyqueue);
  
  return uchild->task.PID;
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








	/*struct list_head * lh = list_first(&freequeue);
  	union task_union *tsku_fill = (union task_union*)list_head_to_task_struct(lh);
  	union task_union *tsku_current = (union task_union*)current();    
  	list_del(lh);*/

	/*struct list_head * child_lh = list_first( &freequeue );
 	list_del(child_lh);
	struct task_struct * child_ts = list_head_to_task_struct(child_lh);
 	struct task_struct * father_ts = current();
 	union task_union * child_tu = (union task_union*) child_ts;
 	union task_union * father_tu = (union task_union*) father_ts;
 	//union task_union *child_tu = (union task_union*)
 	*/