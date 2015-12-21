/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>
#include <libc.h>  

// global variables
struct list_head freequeue;
struct list_head readyqueue;    


// structs
struct keyboard_buffer key_buffer;
struct sem_struct list_sem[20]; // list semaphores

struct task_struct *idle_task;
struct task_struct *init_task;
struct task_struct *fill_task;
int freePID;
int quantum_remaining = 0;
#define DEFAULT_QUANTUM 10


/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS+2]
  __attribute__((__section__(".data.task")));

union task_union *task = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */


struct task_struct *list_head_to_task_struct(struct list_head *l) {
  return list_entry( l, struct task_struct, list);
}

extern struct list_head blocked;


/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t)  {
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t)  {
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}

// previos implementation of allocate dir
int allocate_DIR_old(struct task_struct *t)  {
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}
// new implementation of allocation dir
// looks for a free dir
int allocate_DIR(struct task_struct *t)  {
	int i;
	for (i = 0; i < NR_TASKS; ++i) {
		if (contDir[i] == 0) { // if == 0 --> found
			contDir[i] = 1;
			t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[i]; 
			return 1;
		}
	}
	// no more directories empty --> errno??
	return -1;
}
int calculate_dir_pos(struct task_struct *t) {

	return ((int)t->dir_pages_baseAddr - (int)&dir_pages[0])/sizeof(dir_pages[0]);
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");
	sys_write(1,"idle",4);
	while(1)
	{
		;//sys_write(1,"idle",4);
	}
}

void init_stats(struct stats *s) {
	s->user_ticks = 0;
	s->system_ticks = 0;
	s->blocked_ticks = 0;
	s->ready_ticks = 0;
	s->elapsed_total_ticks = get_ticks();
	s->total_trans = 0;
	s->remaining_ticks = get_ticks();
}

void update_stats(unsigned long *v, unsigned long *elapsed) {
  unsigned long current_ticks;
  current_ticks=get_ticks();
  *v += current_ticks - *elapsed;
  *elapsed=current_ticks;
}

// Scheduling
int get_quantum(struct task_struct *t) {
	return t->total_quantum;
}
void set_quantum(struct task_struct *t, int new_quantum) {
	t->total_quantum = new_quantum;
}
void update_sched_data_rr() {
	quantum_remaining--;
}
int needs_sched_rr() {
	// quantum out --> readyqueue not empty --> need sched
	if ((quantum_remaining==0) && (!list_empty(&readyqueue))) return 1;
	// if quantum out ---> assign new quantum(all)
	if (quantum_remaining==0) quantum_remaining = get_quantum(current());
	return 0;
}
// Function to select the next process to execute, to extract it from the ready queue and to invoke
// the context switch process. This function should always be executed after updating the state
// of the current process (after calling function update_process_state_rr).
// Update the readyqueue, if current process is not the idle process, by inserting the current
// process at the end of the readyqueue.
// – Extract the first process of the readyqueue, which will become the current process;
// – And perform a context switch to this selected process. Remember that when a process
// returns to the execution stage after a context switch, its quantum ticks must be restored.
void sched_next_rr() {

	struct list_head *e;
	struct task_struct *t;

	//e = list_first(&readyqueue);
	if (!list_empty(&readyqueue)) {
		e = list_first(&readyqueue);
		list_del(e);
		t = list_head_to_task_struct(e);
	}
	else { // ready queue empty ---> idle
		t = idle_task;
	}
	t->state=ST_RUN;
	quantum_remaining=get_quantum(t); 


	update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
	update_stats(&(t->p_stats.ready_ticks), &(t->p_stats.elapsed_total_ticks));
	t->p_stats.total_trans++;

	task_switch((union task_union*)t);

}
// If the current state is not running, then this function deletes the process from its current queue.
// If the new state of the process is not running, then this function inserts the process into a suitable queue 
// If the new state of the process is running, then the queue parameter shoud be NULL.
void update_process_state_rr(struct task_struct *t, struct list_head *dest) {
	if (t->state != ST_RUN) list_del(&(t->list));
	if (dest != NULL) {
		list_add_tail(&(t->list), dest);
		if (dest != &readyqueue) t->state = ST_BLOCKED;
		else {
			t->state = ST_READY;
			update_stats(&(current()->p_stats.system_ticks), &(current()->p_stats.elapsed_total_ticks));
		}
		
	}
	else t->state = ST_RUN;	

}  

void schedule() {
	update_sched_data_rr();
	if (needs_sched_rr()) {
		update_process_state_rr(current(), &readyqueue);
		sched_next_rr();
	}
}


void init_idle (void) {
	// Get an available task_union from the freequeue 
	// to contain the characteristics of this process
	struct list_head *first = list_first( &freequeue );
	struct task_struct *idletask = list_head_to_task_struct(first);
	list_del( first ); 
	// Assign PID 0 to the process
	idletask->PID = 0;

	idletask->heap_start = NULL;
	idletask->bytesHeap = 0;
	idletask->numPagesHeap = 0;
	// Initialize field dir_pages_baseAaddr with a
	// new directory to store the process address 
	// space using the allocate_DIR routine.
	allocate_DIR(idletask);
	// Initialize an execution context for the procees to restore it 
	// when it gets assigned the cpu and executes cpu_idle.
	union task_union * idle_task_union = (union task_union *) idletask;
	// Store in the stack of the idle process the address of the code 
	// that it will execute (address of the cpu_idle function).
	idle_task_union->stack[KERNEL_STACK_SIZE-1] = &cpu_idle;
	// Store in the stack the initial value that we want to assign to 
	// register ebp when undoing the dynamic link (it can be 0),
	idle_task_union->stack[KERNEL_STACK_SIZE-2] = 0;
	// Finally, we need to keep (in a field of its task_struct) the 
	// position of the stack where we have stored the initial value for
	// the ebp register. This value will be loaded in the esp
	// register when undoing the dynamic link.
	idletask->kernel_stack = &idle_task_union->stack[KERNEL_STACK_SIZE-2];
	//6 Initialize the global variable idle_task, which will help to get
	// easily the task_struct of the idle process.
	idle_task = idletask;

	// set quantum
	idletask->total_quantum = DEFAULT_QUANTUM;

	// init stats
	init_stats(&idletask->p_stats);

}

void init_task2(void) {
	struct list_head *l = list_first(&freequeue);
	list_del(l);
	struct task_struct *c = list_head_to_task_struct(l);
	union task_union *uc = (union task_union*)c;

	c->PID=1;

	c->heap_start = NULL;
	c->bytesHeap = 0;
	c->numPagesHeap = 0;

	c->total_quantum=DEFAULT_QUANTUM;

	c->state=ST_RUN;

	quantum_remaining=c->total_quantum;

	init_stats(&c->p_stats);

	allocate_DIR(c);

	set_user_pages(c);

	tss.esp0=(DWord)&(uc->stack[KERNEL_STACK_SIZE]);

	set_cr3(c->dir_pages_baseAddr);

	freePID = 2;
	init_task = c;
}

void init_task1(void) {
	// Get an available task_union from the freequeue 
	// to contain the characteristics of this process
	struct list_head *init_lh = list_first( &freequeue );
	struct task_struct *inittask = list_head_to_task_struct(init_lh);
	list_del( init_lh ); 
	// Assign PID 1 to the process
	inittask->PID = 1;
	inittask->heap_start = NULL;
	inittask->bytesHeap = 0;
	inittask->numPagesHeap = 0;
	// Initialize field dir_pages_baseAaddr with a
	// new directory to store the process address 
	// space using the allocate_DIR routine.
	allocate_DIR(inittask);
	// Complete the initialization of its address space, by using the 
	// function set_user_pages.  This function allocates physical pages
	// to hold the user address space (both code and data pages) and 
	// adds to the page table the logical-to-physical translation for
	// these pages. Remind that the region that supports the kernel 
	// address space is already configure for all the possible processes
	//  by the function init_mm.
	set_user_pages( inittask );
	union task_union * init_union = (union task_union *)inittask;
	// Update the TSS to make it point to the new_task system stack.
	tss.esp0 = &init_union->stack[KERNEL_STACK_SIZE];
	// Set its page directory as the current page directory in the system,
	// by using the set_cr3 function (see file mm.c).
	set_cr3(inittask->dir_pages_baseAddr);

	freePID = 2;
	init_task = inittask;

	// set quantum 
	inittask->total_quantum=DEFAULT_QUANTUM;

	init_stats(&inittask->p_stats);

	quantum_remaining=inittask->total_quantum;

	inittask->state = ST_RUN;
}

void inner_task_switch(union task_union *t) {
	// Update the TSS to make it point to the new_task system stack.
	tss.esp0 = &t->stack[KERNEL_STACK_SIZE];
	// Change the user address space by updating the current page 
	// directory: use the set_cr3 funtion to set the cr3 register to point 
	// to the page directory of the new_task.
	set_cr3(t->task.dir_pages_baseAddr);
	// (3)Store the current value of the EBP register in the PCB. EBP has the 
	// address of the current system stack where the inner_task_switch routine 
	// begins (the dynamic link).
	// (4)Change the current system stack by setting ESP register to point to the
	// stored value in the new PCB.
	// (5)Restore the EBP register from the stack.
	// (6)Return to the routine that called this one using the instruction RET
	void * old = current()->kernel_stack;
	void * new = (t->task).kernel_stack;
	__asm__ __volatile__(
		"movl %%ebp, %0;"
		"movl %1, %%esp;"
		"popl %%ebp;"
		"ret;"
		:
		: "g" (old), "g" (new) );
}

void task_switch(union task_union *t) {
	// save edi esi ebx
	asm("pushl %esi;"
		"pushl %edi;"
		"pushl %ebx;");
	// call inner task switch
	inner_task_switch(t);
	// restore edi esi ebx
	asm("popl %ebx;"
		"popl %edi;"
		"popl %esi;");

}


void init_sched(){
	
	// initialization freequeue
	INIT_LIST_HEAD( &freequeue );

	// initialization readyqueue
	INIT_LIST_HEAD( &readyqueue );
	

	// add all process structs to frequeue
	int i;
	for (i = 0; i < NR_TASKS; ++i) {
		list_add_tail(&(task[i].task.list), &freequeue);
		// we used this loop for initialize the contDir
		contDir[i] = 0;
	}

	init_keyboard_buffer();
	//add_key('a');
	//add_key('b');
	//add_key('c');
	//add_key('d');
	//print_buffer(4);

}

struct task_struct* current() {
	int ret_value;
  
	__asm__ __volatile__(
		"movl %%esp, %0"
		: "=g" (ret_value)
	);
	return (struct task_struct*)(ret_value&0xfffff000);
}

// keyboard queue operations

void init_keyboard_buffer() {
	key_buffer.punter_read = 0;
	key_buffer.punter_write = 0;
	key_buffer.write_keys = 0;
	// initialization keyboardqueue
	INIT_LIST_HEAD( &key_buffer.keyboardqueue );	
}
int can_read(int count) {
	// return 1 if can read
	// o otherwise
	return key_buffer.write_keys >= count;
}
int add_key(char c) {
	// return 1 if can read
	// o otherwise
	if (is_full()) return -1;
	key_buffer.pressed_keys[key_buffer.punter_write++] = c;
	key_buffer.write_keys++;
	//print_buffer(key_buffer.write_keys);
	return 0;
}
void print_buffer(int n) {
	sys_write(1,"dins: ",6);
	char buf[2];
	itoa(remaining(),buf);
	sys_write(1,buf,2);
	sys_write(1,key_buffer.pressed_keys,n);
}
int remaining() {
	return MAX_PRESSED_KEYS - key_buffer.write_keys;
}
int is_full() {
	return key_buffer.write_keys == MAX_PRESSED_KEYS;
}
void copy_all(char *buf) {
	int inici = key_buffer.punter_read;
	int inici_num = MAX_PRESSED_KEYS - inici;
	copy_to_user(&key_buffer.pressed_keys[inici], buf, inici_num);
	copy_to_user(&key_buffer.pressed_keys[0], buf+(inici_num), inici);
	key_buffer.punter_read = 0;
	key_buffer.punter_write = 0;
	key_buffer.write_keys = 0;
}
void copy(char *buf, int cont) {
	int inici = key_buffer.punter_read;
	int inici_num = MAX_PRESSED_KEYS - inici;
	if (inici_num >= cont) {
		copy_to_user(&key_buffer.pressed_keys[inici], buf, cont);
		key_buffer.punter_read += cont;
		if (key_buffer.punter_read == MAX_PRESSED_KEYS) {
			key_buffer.punter_read = 0;
		}
	}
	else {
		copy_to_user(&key_buffer.pressed_keys[inici], buf, inici_num);
		int restants = cont-inici_num;
		key_buffer.punter_read = restants;
		copy_to_user(&key_buffer.pressed_keys[0], buf+(inici_num), restants);
	}
	key_buffer.write_keys -= cont;
}
int threads_waiting() {
	return !list_empty(&key_buffer.keyboardqueue);
}
