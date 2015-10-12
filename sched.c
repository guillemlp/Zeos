/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

// global variables
 struct list_head freequeue;
 struct list_head readyqueue;
 struct task_struct * idle_task;

/**
 * Container for the Task array and 2 additional pages (the first and the last one)
 * to protect against out of bound accesses.
 */
union task_union protected_tasks[NR_TASKS+2]
  __attribute__((__section__(".data.task")));

union task_union *task = &protected_tasks[1]; /* == union task_union task[NR_TASKS] */


struct task_struct *list_head_to_task_struct(struct list_head *l)
{
  return list_entry( l, struct task_struct, list);
}

extern struct list_head blocked;


/* get_DIR - Returns the Page Directory address for task 't' */
page_table_entry * get_DIR (struct task_struct *t) 
{
	return t->dir_pages_baseAddr;
}

/* get_PT - Returns the Page Table address for task 't' */
page_table_entry * get_PT (struct task_struct *t) 
{
	return (page_table_entry *)(((unsigned int)(t->dir_pages_baseAddr->bits.pbase_addr))<<12);
}


int allocate_DIR(struct task_struct *t) 
{
	int pos;

	pos = ((int)t-(int)task)/sizeof(union task_union);

	t->dir_pages_baseAddr = (page_table_entry*) &dir_pages[pos]; 

	return 1;
}

void cpu_idle(void)
{
	__asm__ __volatile__("sti": : :"memory");

	while(1)
	{
	;
	}
}

void init_idle (void) {
	// Get an available task_union from the freequeue 
	// to contain the characteristics of this process
	struct list_head * first = list_first( &freequeue );
	struct task_struct * idletask = list_head_to_task_struct(first);
	list_del( first ); 
	// Assign PID 0 to the process
	idletask->PID = 0;
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
	idletask->pointer = idle_task_union->stack[KERNEL_STACK_SIZE-2];
	//6 Initialize the global variable idle_task, which will help to get
	// easily the task_struct of the idle process.
	idle_task = idletask;

}

void init_task1(void) {
	// Get an available task_union from the freequeue 
	// to contain the characteristics of this process
	struct list_head * init_lh = list_first( &freequeue );
	struct task_struct * inittask = list_head_to_task_struct(init_lh);
	list_del( init_lh ); 
	// Assign PID 1 to the process
	inittask->PID = 1;
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
	void * old = current()->pointer;
	void * new = (t->task).pointer;
	__asm__ __volatile__(
		"movl %%ebp, %%eax;"
		"movl %%ebx, %%ebx;"
		"popl %%ebp;"
		"ret;"
		:
		: "a" (old), "b" (new) );
	
}

void task_switch(union task_union *t) {
	// save edi esi ebx
	asm("pushl %esi;"
		"pushl %edi;"
		"pushl %ebx;");
	// call inner task switch
	inner_task_switch(t);
	// restore edi esi ebx
	asm("pushl %ebx;"
		"pushl %edi;"
		"pushl %ebx;");

}


void init_sched(){

	// free queue declaration
	//extern struct list_head freequeue;
	
	// initialization freequeue
	INIT_LIST_HEAD( &freequeue );

	// add all process structs
	int i;
	for (i = 0; i < NR_TASKS; ++i) {
		list_add_tail(&(task[i].task.list), &freequeue);
	}

	// ready queue
	//struct list_head readyqueue;

	// initialization readyqueue
	INIT_LIST_HEAD( &readyqueue );
}

struct task_struct* current() {
	int ret_value;
  
	__asm__ __volatile__(
		"movl %%esp, %0"
		: "=g" (ret_value)
	);
	return (struct task_struct*)(ret_value&0xfffff000);
}

