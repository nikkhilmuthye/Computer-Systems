#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/malloc.h"
#include "threads/init.h"
#include "userprog/process.h"
#include "filesys/file.h"
#include "filesys/filesys.h"
#include "filesys/directory.h"
#include "devices/input.h"
#include "devices/shutdown.h"
#include "lib/user/syscall.h"
#include "lib/string.h"
#include "threads/vaddr.h"
#include "pagedir.h"
#include "syscall.h"
#include "lib/kernel/stdio.h"

static void syscall_handler (struct intr_frame *);

/* Start of Project 2 */
static void
validate_addr(void **);
/* End of Project 2 */

void
syscall_init (void) 
{
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /* Start of Project 2 */

  /* Check if the ESP is in above the user start address. */
  if ((void **)f->esp < (void *) USER_START_ADDR)
    user_exit (-1);
 
  int *syscall_num = f->esp;
  struct thread *cur = thread_current ();
  struct thread *parent;
  struct child_process *cur_process;
  int status, fd;
  struct file *fp;
  struct fd_name *temp_fd, *fd_name;
  char *file_name;
  off_t initial_size;
  void *buffer, *end_addr;
  uint32_t size, count;
  struct open_file *open_fp UNUSED;
  pid_t pid;


  switch (*syscall_num)
  {
    case SYS_HALT: shutdown_power_off ();
    		   break;

    case SYS_EXIT: if(is_kernel_vaddr((void *)(f->esp+4)))
                     user_exit(-1);
                   status =  *(int *)(f->esp+4);

		   /* Check if thread is not a orphan. If not orphan, then
		    * update it's status as THREAD_DYING in it's parent's
		    * child processes. */
                   if (!cur->orphan)
                   {
                     parent = cur->parent;
                     cur_process = get_wait_child (parent, cur->tid);
                     cur_process->child_status = THREAD_DYING;
		     cur_process->status = status;
                   }

		   /* Print the status and exit. */
		   user_exit(status);
    		   break;

    case SYS_EXEC:
                   file_name = *(char **)(f->esp+12);

		   /* Validate whether the file is in user space, whether
		    * the file name is not NULL and whether the file exits in
		    * the root directory. */
		   validate_addr((void **)(f->esp+12));

		   if (!strcmp (file_name, ""))
		     user_exit(-1);

		   f->eax = process_execute (file_name);
		   break;

    case SYS_WAIT: pid = *(int *)(f->esp+4);
		   f->eax = process_wait (pid);
		   break;

    case SYS_CREATE:
		   file_name = *(char **)(f->esp+16);
    		   initial_size = *(off_t *)(f->esp+20);
		   
		   /* Validate whether the file is in user space, whether
		    * the file name is not NULL and whether the file exits in
		    * the root directory. */
		   validate_addr((void **)(f->esp+16));

                   /* Validate whether the file_name is not empty. */
		   if (!strcmp (file_name, ""))
		     user_exit(-1);

                   /* Validate whether the length of the file_name is not 
		    * more than 14. */
		   if(strlen (file_name) > 14)
		   {
		     f->eax = 0;
		     break;
		   }
		   else
                     f->eax = filesys_create (file_name, initial_size);
                   break;

    case SYS_REMOVE:
                   file_name = *(char **)(f->esp+4);
                   /* Validate whether the file_name is not empty. */
                   if (!strcmp (file_name, ""))
                     user_exit(-1);
                   
		   f->eax = filesys_remove (file_name);
                   break;

    case SYS_OPEN: 
                   file_name = *(char **)(f->esp+4);

		   /* Validate whether the file is in user space, whether
                    * the file name is not NULL and whether the file exits in
                    * the root directory. */
		   validate_addr((void **)(f->esp+4));
		   struct open_file *open_fp;

    		   /* Check for valid file_name */
		   if(!strcmp (file_name, ""))
		   {
		     f->eax = -1;
		     break;
		   }
                   /* Validate whether the open files count is less than 64. */
		   if (cur->fd_count >= 64)
		   {
		     f->eax = -1;
		     break;
		   }

		   fp = filesys_open (file_name);

		   /* Validate whether the file exits in the directory. */
		   if (fp == NULL)
		   {
		     f->eax = -1;
		     break;
		   }

		   /* Insert file entry in current thread's open_files list. */
		   temp_fd = (struct fd_name *)malloc (sizeof(struct fd_name));
		   if (list_empty (&(cur->open_files)))
		   {
		     temp_fd->fd = 3;
		   }
		   else
		   {
		     temp_fd->fd = (list_entry (list_rbegin (&(cur->open_files)),
		     			     struct fd_name, elem))->fd + 1;
		   }
		   temp_fd->file_name = file_name;
		   temp_fd->file = fp;
		   list_push_back (&(cur->open_files), &(temp_fd->elem));
		   (cur->fd_count)++;
		   f->eax = temp_fd->fd;

		   /* Insert file entry in all_files global list. If present,
		    * increment open_cnt. */
		   add_new_file (file_name, OPEN);
		   break;

    case SYS_FILESIZE:
                   fd = *(int *) (f->esp+4);

		   /* Check if the file descriptor is greater than 3
		    * i.e. it is not the fd of STDIN, STDOUT or STDERR*/
		   if( fd > 2)
		   {
                     fd_name = get_fd_data (fd);
                     if (fd_name != NULL)
                       f->eax = file_length(fd_name->file);
		     else
		       f->eax = 0;
		   }
		   else
		     f->eax = -1;
                   break;
     case SYS_READ:
                   fd = *(int *) (f->esp+20);
		   buffer = *(char **)(f->esp+24);
		   size = *(uint32_t *)(f->esp+28);
		   
		   /* Validate whether the file is in user space, whether
                    * the file name is not NULL and whether the file exits in
                    * the root directory. */
		   validate_addr((void **)(f->esp+24));

		  /* Validate whether the file descriptor is of STDIN */ 
		  if (fd == 0)  
		     f->eax = input_getc ();
		  /* Validate whether the file descriptor is not of STDOT or 
		   * STDERR */ 
		   else if (fd == 1 || fd == 2)
		     f->eax = 0;
		   else
		   {
		     fd_name = get_fd_data (fd);
		    /* Validate whether the file descriptor is not NULL */ 
		     if (fd_name != NULL)
		     {
		       f->eax = file_read (fd_name->file, buffer, size);
		       end_addr = buffer + f->eax;
		      /* Validate whether the end address of the buffer is in
		       * user space */ 
		       validate_addr((void **)&end_addr);
		     }
		     else
		       f->eax = 0;
		   }
		   break;

    case SYS_WRITE:
                   fd = *(int *) (f->esp+20);
                   buffer = *(char **)(f->esp+24);
                   size = *(uint32_t *)(f->esp+28);
		   
		   /* Validate whether the stack is in user space, whether
                    * the file name is not NULL and whether the file exits in
                    * the root directory. */
		   validate_addr((void **)(f->esp+24));
		   end_addr = buffer + size;
		   
		   /* Validate whether the end address of the buffer is in
		    * user space */ 
		   validate_addr((void **)&end_addr);
		   count = size;

                   /* Check for valid file descriptor */
		   if (fd == 1)
		   {
		     while (size > 512)
		     {
		       putbuf (buffer, 512);
		       buffer += 512;
		       size -= 512;
		     } 
		     putbuf (buffer, size);
		     f->eax = count; 
		   }
                   else if (fd == 0 || fd == 2)
                     f->eax = 0;
                   else
                   {
		     fd_name = get_fd_data (fd);

                     /* File descriptor is correct. Get fd entry and acquire
		      * write lock from global list all_files to write to file. */

                     if (fd_name != NULL)
		     {
		       open_fp = get_file_open (fd_name->file_name);
		       
		       /* Check for if the current file is not currently 
		        * executing. */
		       if (open_fp->exec_cnt == 0)
		       {
		         lock_acquire (&open_fp->lock);
		         f->eax = file_write (fd_name->file, buffer, size);
		         lock_release (&open_fp->lock);
		       }
		       else
		         f->eax = 0;
		     }
		     else
		       f->eax = 0;
                   }
                   break;

    case SYS_SEEK:
                   fd = *(int *) (f->esp+16);
		   initial_size = *(off_t *)(f->esp+20);
		  /* Check if the file descriptor is greater than 3
		   * i.e. it is not the fd of STDIN, STDOUT or STDERR*/
		   if( fd > 2)
                   {
                     fd_name = get_fd_data (fd);
                     if (fd_name != NULL)
                       file_seek(fd_name->file, initial_size);
                   }
                   else
                     f->eax = -1;
                   break;

    case SYS_TELL:
                   fd = *(int *) (f->esp+4);
		  /* Check if the file descriptor is greater than 3
		   * i.e. it is not the fd of STDIN, STDOUT or STDERR*/
                   if( fd > 2)
                   {
                     fd_name = get_fd_data (fd);
                     if (fd_name != NULL)
                       f->eax = file_tell(fd_name->file);
                     else
                       f->eax = 0;
                   }
                   else
                     f->eax = -1;
                   break;


    case SYS_CLOSE:
                   fd = *(int *) (f->esp+4);
		   fd_name = get_fd_data (fd);
		   
		   /* Validate the input file descriptor. */
		   if (fd_name != NULL)
		   {
		     check_remove_file (fd_name->file_name, OPEN);
		     file_close (fd_name->file);
		     list_remove (&fd_name->elem);
		     free (fd_name);
		   }
		   break;
  }

}


/* Validates the given address to see if they lie in the user space.
 * If the address is in kernel space or if the address is not mapped
 * in the memory and the given address is not NULL, then the process
 * exists with a -1 return status*/
static void
validate_addr(void **vaddr)
{
  struct thread *cur = thread_current();
  if(  is_kernel_vaddr(*vaddr) 
       || (lookup_page (cur->pagedir, *vaddr, false) == NULL)
       || (*vaddr == NULL))
           user_exit(-1);
}

