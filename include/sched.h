/*
 * sched.h - Estructures i macros pel tractament de processos
 */

#ifndef __SCHED_H__
#define __SCHED_H__

#include <list.h>
#include <types.h>
#include <stats.h> 
#include <mm_address.h>

#define NR_TASKS      10
#define KERNEL_STACK_SIZE	1024
#define MAX_PRESSED_KEYS 50 

enum state_t { ST_RUN, ST_READY, ST_BLOCKED };

struct task_struct {
  int PID;			/* Process ID. This MUST be the first field of the struct. */
  page_table_entry * dir_pages_baseAddr;
  struct list_head list; /* pointer list_head structure */
  void * kernel_stack; /* to keep the position of the stack where we stored the ebp*/
  int total_quantum;
  enum state_t state;
  struct stats p_stats;		/* Process stats */
};

struct keyboard_buffer {
	char pressed_keys[MAX_PRESSED_KEYS];
	int punter_read;
	int punter_write;
	int write_keys;
};

union task_union {
  struct task_struct task;
  unsigned long stack[KERNEL_STACK_SIZE];    /* pila de sistema, per procés */
};

struct sem_struct {
	int owner; // PID owner
	int counter;  // counter of
	struct list_head blocked;
};



extern union task_union protected_tasks[NR_TASKS+2];
extern union task_union *task; /* Vector de tasques */
//extern struct task_struct *idle_task;


#define KERNEL_ESP(t)       	(DWord) &(t)->stack[KERNEL_STACK_SIZE]

#define INITIAL_ESP       	KERNEL_ESP(&task[1])

/* Inicialitza les dades del proces inicial */
void init_task1(void);

void init_idle(void);

void init_sched(void);

struct task_struct * current();

void task_switch(union task_union *new);

struct task_struct *list_head_to_task_struct(struct list_head *l);

int allocate_DIR(struct task_struct *t);

page_table_entry * get_PT (struct task_struct *t) ;

page_table_entry * get_DIR (struct task_struct *t) ;

/* Headers for the scheduling policy */
void sched_next_rr();
void update_process_state_rr(struct task_struct *t, struct list_head *dest);
int needs_sched_rr();
void update_sched_data_rr();
int get_quantum(struct task_struct *t);
void set_quantum(struct task_struct *t, int new_quantum);

void update_stats(unsigned long *v, unsigned long *elapsed);
 
// headers keyboard buffer 
void init_keyboard_buffer();
int can_read(int count);
int remaining();
int is_full();
void copy_all(char *buf);
void copy(char *buf, int cont);

#endif  /* __SCHED_H__ */
