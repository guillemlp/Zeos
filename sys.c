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
extern struct sem_struct list_sem[20];
extern int freePID;
extern struct task_struct * fill_task; 
extern int quantum_remaining;

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
void sys_exit() {
	int i;
	// substract 1 to counter dir vector
	//int pos = (&(current()->dir_pages_baseAddr)-(int)task)/sizeof(union task_union); 
	//sys_write_console(itoa((current()->dir_pages_baseAddr)),32);
	page_table_entry *current_PT = get_PT(current());
	// deallocate the prop physical page
	for (i=0; i<NUM_PAG_DATA; i++) {
		free_frame(get_frame(current_PT,PAG_LOG_INIT_DATA+i));
		del_ss_pag(current_PT, PAG_LOG_INIT_DATA+i);
	}
	int pos = calculate_dir_pos(current());
 	contDir[pos]--;
	// free task struct
	list_add_tail(&(current()->list), &freequeue);
	current()->PID=-1;	
	//restart execution of the next process
	sched_next_rr();
}

int ret_from_fork() {
	return 0;
}
int sys_clone(void (*function)(void), void *stack) {
	struct list_head *child_lh = NULL;
	union task_union *child_tu, *father_tu;
	struct task_struct *child_ts, *father_ts;
	// mirem si pcb's lliures
	if (list_empty(&freequeue)) return -1;
	
	// obtenim pcb lliure
	child_lh = list_first(&freequeue);
 	list_del(child_lh);
 	
 	// task struct i task union fill
 	child_ts = list_head_to_task_struct(child_lh);
	child_tu = (union task_union*) list_head_to_task_struct(child_lh);
 	// task struct i tasc union fill
 	father_ts = current();
 	father_tu = (union task_union*) current();

 	// copiem data apre fill
 	copy_data(father_tu, child_tu, sizeof(union task_union));

 	// modifiquem punt de tornada i stack usuari i 
  	child_tu->stack[KERNEL_STACK_SIZE-18] = &ret_from_fork;
  	child_tu->stack[KERNEL_STACK_SIZE-19] = 0;
  	child_ts->kernel_stack = &child_tu->stack[KERNEL_STACK_SIZE-19];
  	child_tu->stack[KERNEL_STACK_SIZE-5] = function;
 	child_tu->stack[KERNEL_STACK_SIZE-2] = stack;

 	// assignem nou pid
 	int child_pid = freePID;
 	child_ts->PID = child_pid;
 	freePID = freePID + 1;

 	// ++cont dir
 	int pos = calculate_dir_pos(father_ts);
 	contDir[pos]++;
 	


	// estat fill
	child_ts->state = ST_READY;
	child_ts->total_quantum = 10;
	list_add_tail(&(child_ts->list), &readyqueue);

	return child_pid;

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


	// estat fill
	child_ts->state = ST_READY;
	child_ts->total_quantum = 10;
	list_add_tail(&(child_ts->list), &readyqueue);

	return child_pid;
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
int sys_sem_init(int n_sem, unsigned int value) {
	if (n_sem < 0 || n_sem >= 20 || list_sem[n_sem].owner <= 0) return -1;
	list_sem[n_sem].owner = current()->PID;
	list_sem[n_sem].counter = value;
	INIT_LIST_HEAD( &list_sem[n_sem].blocked );
	return 0;
}
int sys_sem_wait(int n_sem) {
	if (n_sem < 0 || n_sem >= 20 || list_sem[n_sem].owner <= 0) return -1; // or it is destroyed
	list_sem[n_sem].counter--;
	if (list_sem[n_sem].counter < 0) {
		list_add_tail(&current()->list, &list_sem[n_sem].blocked);
	}
	return 0;
}
int sys_sem_signal(int n_sem) {
	if (n_sem < 0 || n_sem >= 20) return -12;
	list_sem[n_sem].counter++;
	if (list_sem[n_sem].counter <= 0) {
		list_add_tail(&current()->list, &readyqueue);
	}
	return 0;
}
int sys_sem_destroy() {
	return 0;
}

int sys_get_stats(int pid, struct stats *st)
{
  int i;
  
  if (!access_ok(VERIFY_WRITE, st, sizeof(struct stats))) return -12; 
  
  if (pid<0) return -12;
  for (i=0; i<NR_TASKS; i++)
  {
    if (task[i].task.PID==pid)
    {
      task[i].task.p_stats.remaining_ticks=quantum_remaining;
      copy_to_user(&(task[i].task.p_stats), st, sizeof(struct stats));
      return 0;
    }
  }
  return -12; /*ESRCH */
}


