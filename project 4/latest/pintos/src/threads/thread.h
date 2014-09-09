#ifndef THREADS_THREAD_H
#define THREADS_THREAD_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "synch.h"
#include "filesys/directory.h"

/* States in a thread's life cycle. */
enum thread_status
  {
    THREAD_RUNNING,     /* Running thread. */
    THREAD_READY,       /* Not running but ready to run. */
    THREAD_BLOCKED,     /* Waiting for an event to trigger. */
    THREAD_DYING        /* About to be destroyed. */
  };

/* Thread identifier type.
   You can redefine this to whatever type you like. */
typedef int tid_t;
#define TID_ERROR ((tid_t) -1)          /* Error value for tid_t. */

/* Thread priorities. */
#define PRI_MIN 0                       /* Lowest priority. */
#define PRI_DEFAULT 31                  /* Default priority. */
#define PRI_MAX 63                      /* Highest priority. */

/* Start of Project 2 */
struct fd_name
  {
     int fd;				/* File descriptor of the open file */
     char * file_name;			/* File name of the open file */
     struct file * file;		/* File pointer of the open file. */
     struct dir *dir;			/* Directory pointer if the fd represents directory. */
     bool is_dir;			/* Boolean to identify if the fd represents directory. */
     struct list_elem elem;		/* Element in open_files list. */
  };

struct open_file
  {
     char file_name[50];		/* File name of the open file */
     int exec_cnt;			/* Number of times file is executing. */
     int open_cnt;			/* Number of times file is opened. */
     struct lock lock;			/* Lock for write synchronization. */
     struct list_elem elem;		/* Element in all_files list. */
  };

struct child_process
  {
     tid_t pid;				/* pid of the child thread. */
     enum thread_status child_status;	/* Current child process status. */
     struct thread *process;		/* Child process thread. */
     int status;			/* Return status for parent process. */
     struct condition child_cond;	/* Condition variable for child process. */
     struct list_elem elem;
  };
/* End of Project 2 */

/* A kernel thread or user process.

   Each thread structure is stored in its own 4 kB page.  The
   thread structure itself sits at the very bottom of the page
   (at offset 0).  The rest of the page is reserved for the
   thread's kernel stack, which grows downward from the top of
   the page (at offset 4 kB).  Here's an illustration:

        4 kB +---------------------------------+
             |          kernel stack           |
             |                |                |
             |                |                |
             |                V                |
             |         grows downward          |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             |                                 |
             +---------------------------------+
             |              magic              |
             |                :                |
             |                :                |
             |               name              |
             |              status             |
        0 kB +---------------------------------+

   The upshot of this is twofold:

      1. First, `struct thread' must not be allowed to grow too
         big.  If it does, then there will not be enough room for
         the kernel stack.  Our base `struct thread' is only a
         few bytes in size.  It probably should stay well under 1
         kB.

      2. Second, kernel stacks must not be allowed to grow too
         large.  If a stack overflows, it will corrupt the thread
         state.  Thus, kernel functions should not allocate large
         structures or arrays as non-static local variables.  Use
         dynamic allocation with malloc() or palloc_get_page()
         instead.

   The first symptom of either of these problems will probably be
   an assertion failure in thread_current(), which checks that
   the `magic' member of the running thread's `struct thread' is
   set to THREAD_MAGIC.  Stack overflow will normally change this
   value, triggering the assertion. */
/* The `elem' member has a dual purpose.  It can be an element in
   the run queue (thread.c), or it can be an element in a
   semaphore wait list (synch.c).  It can be used these two ways
   only because they are mutually exclusive: only a thread in the
   ready state is on the run queue, whereas only a thread in the
   blocked state is on a semaphore wait list. */
struct thread
  {
    /* Owned by thread.c. */
    tid_t tid;                          /* Thread identifier. */
    enum thread_status status;          /* Thread state. */
    char name[16];                      /* Name (for debugging purposes). */
    uint8_t *stack;                     /* Saved stack pointer. */
    int priority;                       /* Priority. */
    struct list_elem allelem;           /* List element for all threads list. */

    /* Shared between thread.c and synch.c. */
    struct list_elem elem;              /* List element. */

    /* Start of Project 4 */
    struct list_elem blockelem;         /* List element in blocked_list. */
    int64_t wake_time;			/* Thread starts running at wake_time. */
    /* End of Project 4 */

#ifdef USERPROG
    /* Owned by userprog/process.c. */
    uint32_t *pagedir;                  /* Page directory. */
    /* Start of Project 2 */
    struct list open_files;		/* List of open files. */
    struct list child_processes;	/* List of all child processes of
    					 * current process. */
    bool orphan;			/* True if process is marked as orphan. */
    struct thread *parent;		/* Thread pointer to parent process. */
    struct lock lock_tid;		/* Lock for condition variable. */
    int fd_count;			/* Number of open file descriptors for
    					 * current thread. */
    /* End of Project 2 */
#endif

    /* Start of Project 4 */
    struct dir *dir;
    /* End of Project 4 */

    /* Owned by thread.c. */
    unsigned magic;                     /* Detects stack overflow. */
  };

/* If false (default), use round-robin scheduler.
   If true, use multi-level feedback queue scheduler.
   Controlled by kernel command-line option "-o mlfqs". */
extern bool thread_mlfqs;

void thread_init (void);
void thread_start (void);

void thread_tick (void);
void thread_print_stats (void);

typedef void thread_func (void *aux);
tid_t thread_create (const char *name, int priority, thread_func *, void *);

void thread_block (void);
void thread_unblock (struct thread *);

struct thread *thread_current (void);
tid_t thread_tid (void);
const char *thread_name (void);

void thread_exit (void) NO_RETURN;
void thread_yield (void);

/* Performs some operation on thread t, given auxiliary data AUX. */
typedef void thread_action_func (struct thread *t, void *aux);
void thread_foreach (thread_action_func *, void *);

int thread_get_priority (void);
void thread_set_priority (int);

int thread_get_nice (void);
void thread_set_nice (int);
int thread_get_recent_cpu (void);
int thread_get_load_avg (void);

/* Start of Project 2 */
struct fd_name * get_fd_data (int);
struct child_process * get_wait_child (struct thread *, tid_t);
struct open_file * get_file_open (const char *);
void check_remove_file (const char *, bool);
void add_new_file (const char *, bool);
/* End of Project 2 */

#endif /* threads/thread.h */
