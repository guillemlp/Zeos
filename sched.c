/*
 * sched.c - initializes struct for task 0 anda task 1
 */

#include <sched.h>
#include <mm.h>
#include <io.h>

// van aqui declarats??
 struct list_head freequeue;
 struct list_head readyqueue;
 struct task_struct *idle_task;

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
	//1  Get an available task_union from the freequeue 
	//   to contain the characteristics of this process
	struct list_head * first = list_first( &freequeue );
	//struct union task_union * realelement = list_entry( first, union task_union, list );
	struct task_struct * firsttask = list_head_to_task_struct(first);
	list_del( first ); 

	//2  Assign PID 0 to the process
	firsttask->PID = 0;

	//3  Initialize field dir_pages_baseAaddr with a
	//   new directory to store the process address 
	//   space using the allocate_DIR routine.
	allocate_DIR(firsttask);

	//4 Initialize an execution context for the procees to restore it when it gets assigned the cpu
	//  (see section 4.5) and executes cpu_idle.


	//6 Initialize the global variable idle_task, which will help to get easily the task_struct of the
	//  idle process.

	idle_task = firsttask;


}

void init_task1(void) {
}


void init_sched(){

	// free queue declaration
	//extern struct list_head freequeue;
	
	// initialization freequeue
	INIT_LIST_HEAD( &freequeue );

	// add all process structs
	int i;
	for (i = 0; i < NR_TASKS; ++i) {
		list_add(&(task[i].task.list), &freequeue);
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

