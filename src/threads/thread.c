#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "threads/fixed_math.h"
#ifdef USERPROG
#include "userprog/process.h"
#endif

/* Random value for struct thread's `magic' member.
   Used to detect stack overflow.  See the big comment at the top
   of thread.h for details. */
#define THREAD_MAGIC 0xcd6abf4b

/* List of processes in THREAD_READY state, that is, processes
   that are ready to run but not actually running. */
static struct list ready_list;

/* List of all processes.  Processes are added to this list
   when they are first scheduled and removed when they exit. */
struct list all_list;
struct list prique1;
struct list prique2;
struct list prique3;
struct list prique4;
/* PintOs 2-3 mlqs option */
struct list *current_ready_list;
int load_avg;
int ready_list_index = 0;
struct list *prique_list[4] = {&prique1, &prique2, &prique3, &prique4};
/* Idle thread. */
static struct thread *idle_thread;

/* Initial thread, the thread running init.c:main(). */
static struct thread *initial_thread;

/* Lock used by allocate_tid(). */
static struct lock tid_lock;

/* Stack frame for kernel_thread(). */
struct kernel_thread_frame 
  {
    void *eip;                  /* Return address. */
    thread_func *function;      /* Function to call. */
    void *aux;                  /* Auxiliary data for function. */
  };

/* Statistics. */
static long long idle_ticks;    /* # of timer ticks spent idle. */
static long long kernel_ticks;  /* # of timer ticks in kernel threads. */
static long long user_ticks;    /* # of timer ticks in user programs. */

/* Scheduling. */
#define TIME_SLICE 4            /* # of timer ticks to give each thread. */
static unsigned thread_ticks;   /* # of timer ticks since last yield. */

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
bool thread_mlfqs;
static void kernel_thread (thread_func *, void *aux);

static void idle (void *aux UNUSED);
static struct thread *running_thread (void);
static struct thread *next_thread_to_run (void);
static void init_thread (struct thread *, const char *name, int priority);
static bool is_thread (struct thread *) UNUSED;
static void *alloc_frame (struct thread *, size_t size);
static void schedule (void);
void thread_schedule_tail (struct thread *prev);
static bool check_priority_current(struct thread *t);
void check_next_priority_queue (void);
/* pintos 2-3 mlqs */
void add_recent_cpu(struct thread *t);
void update_threads_priority();
void update_threads_recentCpu();
void update_threads_loadAvg();
void update_thread_priority(struct thread *t);

static tid_t allocate_tid (void);

/* Initializes the threading system by transforming the code
   that's currently running into a thread.  This can't work in
   general and it is possible in this case only because loader.S
   was careful to put the bottom of the stack at a page boundary.

   Also initializes the run queue and the tid lock.

   After calling this function, be sure to initialize the page
   allocator before trying to create any threads with
   thread_create().

   It is not safe to call thread_current() until this function
   finishes. */
void
thread_init (void) 
{
  ASSERT (intr_get_level () == INTR_OFF);

  lock_init (&tid_lock);
  list_init (&ready_list);
  list_init (&all_list);
/*
  if(!thread_mlfqs) { 
    current_ready_list = &ready_list;
  }
  else {
    list_init(&prique1);
    list_init(&prique2);
    list_init(&prique3);
    list_init(&prique4);
    current_ready_list = &prique3;
  }
*/
  /* Set up a thread structure for the running thread. */
  initial_thread = running_thread ();
  init_thread (initial_thread, "main", PRI_DEFAULT);
  initial_thread->status = THREAD_RUNNING;
  initial_thread->tid = allocate_tid ();
}

/* Starts preemptive thread scheduling by enabling interrupts.
   Also creates the idle thread. */
void
thread_start (void) 
{
   load_avg = 0;
  /* Create the idle thread. */
  struct semaphore idle_started;
  sema_init (&idle_started, 0);
  thread_create ("idle", PRI_MIN, idle, &idle_started);

  /* Start preemptive thread scheduling. */
  intr_enable ();

  /* Wait for the idle thread to initialize idle_thread. */
  sema_down (&idle_started);
}

/* Called by the timer interrupt handler at each timer tick.
   Thus, this function runs in an external interrupt context. */
void
thread_tick (void) 
{
  struct thread *t = thread_current ();

  /* Update statistics. 
update Pintod 2-3*/
  if (t == idle_thread) {
    idle_ticks++;
}
#ifdef USERPROG
  else if (t->pagedir != NULL)
    user_ticks++;
#endif
  else
    kernel_ticks++;
//check_next_priority_queue();
  /* Enforce preemption. */
  if (++thread_ticks >= TIME_SLICE)
    intr_yield_on_return ();
}
/* Pintos 2-3 */
void 
check_next_priority_queue (void)
{
  int i = 0;
  for(i=ready_list_index; i<4; i++) {
    if(!list_empty(prique_list[i])){
      current_ready_list = prique_list[i];
      break;
    }
    ready_list_index++;
  }

  if(ready_list_index >3) 
    ready_list_index = 0;

}
/* Prints thread statistics. */
void
thread_print_stats (void) 
{
  printf ("Thread: %lld idle ticks, %lld kernel ticks, %lld user ticks\n",
          idle_ticks, kernel_ticks, user_ticks);
}

/* Creates a new kernel thread named NAME with the given initial
   PRIORITY, which executes FUNCTION passing AUX as the argument,
   and adds it to the ready queue.  Returns the thread identifier
   for the new thread, or TID_ERROR if creation fails.

   If thread_start() has been called, then the new thread may be
   scheduled before thread_create() returns.  It could even exit
   before thread_create() returns.  Contrariwise, the original
   thread may run for any amount of time before the new thread is
   scheduled.  Use a semaphore or some other form of
   synchronization if you need to ensure ordering.

   The code provided sets the new thread's `priority' member to
   PRIORITY, but no actual priority scheduling is implemented.
   Priority scheduling is the goal of Problem 1-3. */
tid_t
thread_create (const char *name, int priority,
               thread_func *function, void *aux) 
{
  struct thread *t;
  struct kernel_thread_frame *kf;
  struct switch_entry_frame *ef;
  struct switch_threads_frame *sf;
  tid_t tid;
  enum intr_level old_level;

  ASSERT (function != NULL);

  /* Allocate thread. */
  t = palloc_get_page (PAL_ZERO);
  if (t == NULL)
    return TID_ERROR;

  /* Initialize thread. */
  if (thread_mlfqs) {
 
    init_thread (t, name, PRI_DEFAULT);
  } else { 
    init_thread (t, name, priority);
  }
  
  tid = t->tid = allocate_tid ();

  /* Prepare thread for first run by initializing its stack.
     Do this atomically so intermediate values for the 'stack' 
     member cannot be observed. */
  old_level = intr_disable ();

  /* Stack frame for kernel_thread(). */
  kf = alloc_frame (t, sizeof *kf);
  kf->eip = NULL;
  kf->function = function;
  kf->aux = aux;

  /* Stack frame for switch_entry(). */
  ef = alloc_frame (t, sizeof *ef);
  ef->eip = (void (*) (void)) kernel_thread;

  /* Stack frame for switch_threads(). */
  sf = alloc_frame (t, sizeof *sf);
  sf->eip = switch_entry;
  sf->ebp = 0;

  intr_set_level (old_level);

  /* Add to run queue. */
  thread_unblock (t);
  if(check_priority_current(t))
    thread_yield();


  return tid;
}
/* bool list_less_func -pintos2 */
bool
list_less_func_impl(const struct list_elem *e1, const struct list_elem *e2, void *aux UNUSED) {
  struct thread *t1 = list_entry(e1, struct thread, elem);
  struct thread *t2 = list_entry(e2, struct thread, elem);
  if(t1 -> priority > t2 ->priority)
    return true;
  else
    return false;
}
/* priority check - pintos2
true - yield() call
false - yield() call pass
The function returns true or false depending on whether the thread_yield() function should be called
 */

static bool
check_priority_readylist(struct list *ready_list) {
   if (list_empty (&ready_list))
      return false ;
   
   struct list_elem *first_elem = list_front(&ready_list);
   struct thread *first_thread = list_entry(first_elem, struct thread, elem);

   int first_pri = first_thread -> priority ;

   struct thread *current_thread = thread_current();
   int current_pri = current_thread -> priority ;

   if(current_pri < first_pri)
      return true;
   return false;
} 
/* Pintos 2-1*/
static bool
check_priority_current(struct thread *t) {

   struct thread *current_thread = thread_current();
   int current_pri = current_thread -> priority ;
   int new_pri = t -> priority ;

   if(current_pri < new_pri)
      return true;
   return false;
}
/* Puts the current thread to sleep.  It will not be scheduled
   again until awoken by thread_unblock().

   This function must be called with interrupts turned off.  It
   is usually a better idea to use one of the synchronization
   primitives in synch.h. */
void
thread_block (void) 
{
  ASSERT (!intr_context ());
  ASSERT (intr_get_level () == INTR_OFF);

  thread_current ()->status = THREAD_BLOCKED;
  schedule ();
}

/* Transitions a blocked thread T to the ready-to-run state.
   This is an error if T is not blocked.  (Use thread_yield() to
   make the running thread ready.)

   This function does not preempt the running thread.  This can
   be important: if the caller had disabled interrupts itself,
   it may expect that it can atomically unblock a thread and
   update other data. */
void
thread_unblock (struct thread *t) 
{
  enum intr_level old_level;

  ASSERT (is_thread (t));

  old_level = intr_disable ();
/*
int priority = t->priority;
int index = 0;
if (priority >= 0 && priority <= 15) {
    index = 0;
  } else if (priority >= 16 && priority <= 31) {
    index =  1;
  } else if (priority >= 32 && priority <= 47) {
    index =  2;
  } else {
    index =  3;
  }
*/
    list_insert_ordered(&ready_list, &t->elem, list_less_func_impl, NULL);
 /* else
    list_push_back (&ready_list, &t->elem);*/
  t->status = THREAD_READY;
  intr_set_level (old_level);

}

/* Returns the name of the running thread. */
const char *
thread_name (void) 
{
  return thread_current ()->name;
}

/* Returns the running thread.
   This is running_thread() plus a couple of sanity checks.
   See the big comment at the top of thread.h for details. */
struct thread *
thread_current (void) 
{
  struct thread *t = running_thread ();
  
  /* Make sure T is really a thread.
     If either of these assertions fire, then your thread may
     have overflowed its stack.  Each thread has less than 4 kB
     of stack, so a few big automatic arrays or moderate
     recursion can cause stack overflow. */
  ASSERT (is_thread (t));
  ASSERT (t->status == THREAD_RUNNING);

  return t;
}

/* Returns the running thread's tid. */
tid_t
thread_tid (void) 
{
  return thread_current ()->tid;
}

/* Deschedules the current thread and destroys it.  Never
   returns to the caller. */
void
thread_exit (void) 
{
  ASSERT (!intr_context ());

#ifdef USERPROG
  process_exit ();
#endif

  /* Remove thread from all threads list, set our status to dying,
     and schedule another process.  That process will destroy us
     when it calls thread_schedule_tail(). */
  intr_disable ();
  list_remove (&thread_current()->allelem);
  thread_current ()->status = THREAD_DYING;
  schedule ();
  NOT_REACHED ();
}

/* Yields the CPU.  The current thread is not put to sleep and
   may be scheduled again immediately at the scheduler's whim. */
void
thread_yield (void) 
{
  struct thread *cur = thread_current ();
  enum intr_level old_level;
  
  ASSERT (!intr_context ());

  old_level = intr_disable ();
  if (cur != idle_thread) {
    list_insert_ordered(&ready_list, &cur->elem, list_less_func_impl, NULL);
} 
  cur->status = THREAD_READY;
  schedule ();
  intr_set_level (old_level);
}

/* Invoke function 'func' on all threads, passing along 'aux'.
   This function must be called with interrupts off. */
void
thread_foreach (thread_action_func *func, void *aux)
{
  struct list_elem *e;

  ASSERT (intr_get_level () == INTR_OFF);

  for (e = list_begin (&all_list); e != list_end (&all_list);
       e = list_next (e))
    {
      struct thread *t = list_entry (e, struct thread, allelem);
      func (t, aux);
    }
}

/* Sets the current thread's priority to NEW_PRIORITY. */
void
thread_set_priority (int new_priority) 
{
  if (thread_mlfqs)
    return; 
  else {
  thread_current ()->priority = new_priority;
  if(check_priority_readylist(&ready_list))
    thread_yield();
  }
}

/* Returns the current thread's priority. */
int
thread_get_priority (void) 
{
  return thread_current ()->priority;
}

/* Sets the current thread's nice value to NICE. */
void
thread_set_nice (int new_nice) 
{
  enum intr_level old_level = intr_disable ();
  /* Pintos2-3. */
  thread_current () -> nice = new_nice;
  update_thread_priority(thread_current());
  intr_set_level (old_level);
  
}

/* Returns the current thread's nice value. */
int
thread_get_nice (void) 
{
  enum intr_level old_level = intr_disable ();
  int nice = thread_current ()-> nice;
  intr_set_level (old_level);
  return nice;
}

/* Returns 100 times the system load average. */
int
thread_get_load_avg (void) 
{
  enum intr_level old_level = intr_disable ();
  int ten_loadAvg = convert_to_integer_round_to_nearest(fixed_point_multiply_integer(load_avg, 100));
  intr_set_level (old_level);
  return ten_loadAvg;
}

/* Returns 100 times the current thread's recent_cpu value. */
int
thread_get_recent_cpu (void) 
{
  enum intr_level old_level = intr_disable ();
  int ten_recentCpu = convert_to_integer_round_to_nearest(fixed_point_multiply_integer(thread_current()->recent_cpu, 100));
  intr_set_level (old_level);
  return ten_recentCpu;
}
/* Pintos 2-3 */
void
add_recent_cpu(struct thread *t) {
if ( t ==idle_thread)
    return;
t->recent_cpu = fixed_point_add_integer(t->recent_cpu, 1);
}

/* Pintos 2-3 */
void
update_thread_priority(struct thread *t) {
int new_priority;
int recent_cpu_saved;
if (t == idle_thread) 
   return ;
new_priority = convert_to_integer_round_to_nearest(
                  fixed_point_subtract(
                    fixed_point_subtract_integer(
                      convert_to_fixed_point(PRI_MAX), 
                      fixed_point_divide_integer(t->recent_cpu, 4)
                    ), 
                    t->nice * 2
                  )
                );

    // Check if the new_priority is within the bounds of PRI_MIN and PRI_MAX
    if (new_priority < PRI_MIN) {
      t->priority = PRI_MIN;
    } else if (new_priority > PRI_MAX) {
      t->priority = PRI_MAX;
    } else {
      t->priority = new_priority;
    }

   // Print the recent_cpu and priority values for debugging
    //printf("Recent CPU: %d\n", recent_cpu_saved);
    //printf("Thread Priority: %d\n", t->priority);
}
/* Pintos 2-3 */

void
update_threads_priority() {
int new_priority;
int recent_cpu_saved;
struct list_elem *e;
// Iterate through all threads and update their priorities.
for (e = list_begin (&all_list); e != list_end (&all_list); e = list_next (e)) {
    struct thread *t = list_entry (e, struct thread, allelem);
    if (t == idle_thread) 
    continue ;
new_priority =convert_to_integer_round_to_nearest(
                  fixed_point_subtract(
                    fixed_point_subtract_integer(
                      convert_to_fixed_point(PRI_MAX), 
                      fixed_point_divide_integer(t->recent_cpu, 4)
                    ), 
                    t->nice * 2
                  )
                );

    //queue change
    //int old_queue_index  = search_priority_queue(t->priority);

    // Check if the new_priority is within the bounds of PRI_MIN and PRI_MAX
    if (new_priority < PRI_MIN) {
      t->priority = PRI_MIN;
    } else if (new_priority > PRI_MAX) {
      t->priority = PRI_MAX;
    } else {
      t->priority = new_priority;
    }

    }
}

/* update recent_cpu Pintos 2-3 */
void
update_threads_recentCpu() {
//recent_cpu = (2load_avg)/(2load_avg + 1) * recent_cpu + nice
//(2load_avg)/(2load_avg + 1) -> this common all threads so only one calculate and save the result
struct list_elem *e;
/*int save_common = fixed_point_divide(
      fixed_point_multiply_integer(load_avg, 2),
      fixed_point_add_integer(fixed_point_multiply_integer(load_avg, 2), 1));*/

    for (e = list_begin(&all_list); e != list_end(&all_list);
         e = list_next(e)) {
      struct thread *t = list_entry(e, struct thread, allelem);
          if (t == idle_thread)
            continue ;
       t->recent_cpu = fixed_point_add_integer(
    fixed_point_multiply(
        fixed_point_divide(
            fixed_point_multiply_integer(load_avg, 2),
            fixed_point_add_integer(
                fixed_point_multiply_integer(load_avg, 2), 1)),
        t->recent_cpu),
    t->nice);


       //printf("recent_cpu_after: %d\n", result);
    }
}

/* update load_avg Pintos 2-3 */
void
update_threads_loadAvg() {
//int blocked_threads = list_size(&blocked_list);
//int all_threads = list_size(&all_list);
//int ready_threads = all_threads - blocked_threads ;

int ready_threads = list_size(&ready_list);
//int ready_threads = list_size(&all_list);
//printf("ready_threads: %d\n", ready_threads);
//(59/60)*load_avg + (1/60)*ready_threads
 if (thread_current () == idle_thread)
    ready_threads = list_size (&ready_list);
  else
    ready_threads = list_size (&ready_list) + 1;



load_avg = fixed_point_add(
      fixed_point_multiply(
          fixed_point_divide(
              convert_to_fixed_point(59),
              convert_to_fixed_point(60)),
          load_avg),
      fixed_point_divide_integer(
          fixed_point_multiply_integer(
              convert_to_fixed_point(1),
              ready_threads),
          60));
//printf("recent_ready_threads: %d\n", ready_threads);
//printf("recent_load_avg_after: %d\n", load_avg);
}

/* Idle thread.  Executes when no other thread is ready to run.

   The idle thread is initially put on the ready list by
   thread_start().  It will be scheduled once initially, at which
   point it initializes idle_thread, "up"s the semaphore passed
   to it to enable thread_start() to continue, and immediately
   blocks.  After that, the idle thread never appears in the
   ready list.  It is returned by next_thread_to_run() as a
   special case when the ready list is empty. */
static void
idle (void *idle_started_ UNUSED) 
{
  struct semaphore *idle_started = idle_started_;
  idle_thread = thread_current ();
  sema_up (idle_started);

  for (;;) 
    {
      /* Let someone else run. */
      intr_disable ();
      thread_block ();

      /* Re-enable interrupts and wait for the next one.

         The `sti' instruction disables interrupts until the
         completion of the next instruction, so these two
         instructions are executed atomically.  This atomicity is
         important; otherwise, an interrupt could be handled
         between re-enabling interrupts and waiting for the next
         one to occur, wasting as much as one clock tick worth of
         time.

         See [IA32-v2a] "HLT", [IA32-v2b] "STI", and [IA32-v3a]
         7.11.1 "HLT Instruction". */
      asm volatile ("sti; hlt" : : : "memory");
    }
}

/* Function used as the basis for a kernel thread. */
static void
kernel_thread (thread_func *function, void *aux) 
{
  ASSERT (function != NULL);

  intr_enable ();       /* The scheduler runs with interrupts off. */
  function (aux);       /* Execute the thread function. */
  thread_exit ();       /* If function() returns, kill the thread. */
}

/* Returns the running thread. */
struct thread *
running_thread (void) 
{
  uint32_t *esp;

  /* Copy the CPU's stack pointer into `esp', and then round that
     down to the start of a page.  Because `struct thread' is
     always at the beginning of a page and the stack pointer is
     somewhere in the middle, this locates the curent thread. */
  asm ("mov %%esp, %0" : "=g" (esp));
  return pg_round_down (esp);
}

/* Returns true if T appears to point to a valid thread. */
static bool
is_thread (struct thread *t)
{
  return t != NULL && t->magic == THREAD_MAGIC;
}

/* Does basic initialization of T as a blocked thread named
   NAME. */
static void
init_thread (struct thread *t, const char *name, int priority)
{
  ASSERT (t != NULL);
  ASSERT (PRI_MIN <= priority && priority <= PRI_MAX);
  ASSERT (name != NULL);

  memset (t, 0, sizeof *t);
  t->status = THREAD_BLOCKED;
  strlcpy (t->name, name, sizeof t->name);
  t->stack = (uint8_t *) t + PGSIZE;
  t->priority = priority;
  t->magic = THREAD_MAGIC;
  t->nice = 0;
  t->recent_cpu = 0; 
  list_push_back (&all_list, &t->allelem);
}

/* Allocates a SIZE-byte frame at the top of thread T's stack and
   returns a pointer to the frame's base. */
static void *
alloc_frame (struct thread *t, size_t size) 
{
  /* Stack data is always allocated in word-size units. */
  ASSERT (is_thread (t));
  ASSERT (size % sizeof (uint32_t) == 0);

  t->stack -= size;
  return t->stack;
}

/* Chooses and returns the next thread to be scheduled.  Should
   return a thread from the run queue, unless the run queue is
   empty.  (If the running thread can continue running, then it
   will be in the run queue.)  If the run queue is empty, return
   idle_thread. */
static struct thread *
next_thread_to_run (void) 
{
  if (list_empty (&ready_list))
    return idle_thread;
  else
    return list_entry (list_pop_front (&ready_list), struct thread, elem);
}

/* Completes a thread switch by activating the new thread's page
   tables, and, if the previous thread is dying, destroying it.

   At this function's invocation, we just switched from thread
   PREV, the new thread is already running, and interrupts are
   still disabled.  This function is normally invoked by
   thread_schedule() as its final action before returning, but
   the first time a thread is scheduled it is called by
   switch_entry() (see switch.S).

   It's not safe to call printf() until the thread switch is
   complete.  In practice that means that printf()s should be
   added at the end of the function.

   After this function and its caller returns, the thread switch
   is complete. */
void
thread_schedule_tail (struct thread *prev)
{
  struct thread *cur = running_thread ();
  
  ASSERT (intr_get_level () == INTR_OFF);

  /* Mark us as running. */
  cur->status = THREAD_RUNNING;

  /* Start new time slice. */
  thread_ticks = 0;

#ifdef USERPROG
  /* Activate the new address space. */
  process_activate ();
#endif

  /* If the thread we switched from is dying, destroy its struct
     thread.  This must happen late so that thread_exit() doesn't
     pull out the rug under itself.  (We don't free
     initial_thread because its memory was not obtained via
     palloc().) */
  if (prev != NULL && prev->status == THREAD_DYING && prev != initial_thread) 
    {
      ASSERT (prev != cur);
      palloc_free_page (prev);
    }
}

/* Schedules a new process.  At entry, interrupts must be off and
   the running process's state must have been changed from
   running to some other state.  This function finds another
   thread to run and switches to it.

   It's not safe to call printf() until thread_schedule_tail()
   has completed. */
static void
schedule (void) 
{
  struct thread *cur = running_thread ();
  struct thread *next = next_thread_to_run ();
  struct thread *prev = NULL;

  ASSERT (intr_get_level () == INTR_OFF);
  ASSERT (cur->status != THREAD_RUNNING);
  ASSERT (is_thread (next));

  if (cur != next)
    prev = switch_threads (cur, next);
  thread_schedule_tail (prev);
}

/* Returns a tid to use for a new thread. */
static tid_t
allocate_tid (void) 
{
  static tid_t next_tid = 1;
  tid_t tid;

  lock_acquire (&tid_lock);
  tid = next_tid++;
  lock_release (&tid_lock);

  return tid;
}

/* Offset of `stack' member within `struct thread'.
   Used by switch.S, which can't figure it out on its own. */
uint32_t thread_stack_ofs = offsetof (struct thread, stack);
