#include "userprog/exception.h"
#include <inttypes.h>
#include <stdio.h>
#include "userprog/gdt.h"
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#ifdef VM
#include "vm/page.h"
#include "vm/frame.h"
#include "vm/swap.h"
#endif
/* Number of page faults processed. */
static long long page_fault_cnt;

static struct lock exception_lock;

static void kill (struct intr_frame *);
static void page_fault (struct intr_frame *);

/* Registers handlers for interrupts that can be caused by user
   programs.

   In a real Unix-like OS, most of these interrupts would be
   passed along to the user process in the form of signals, as
   described in [SV-386] 3-24 and 3-25, but we don't implement
   signals.  Instead, we'll make them simply kill the user
   process.

   Page faults are an exception.  Here they are treated the same
   way as other exceptions, but this will need to change to
   implement virtual memory.

   Refer to [IA32-v3a] section 5.15 "Exception and Interrupt
   Reference" for a description of each of these exceptions. */
void
exception_init (void) 
{
  /* These exceptions can be raised explicitly by a user program,
     e.g. via the INT, INT3, INTO, and BOUND instructions.  Thus,
     we set DPL==3, meaning that user programs are allowed to
     invoke them via these instructions. */
  intr_register_int (3, 3, INTR_ON, kill, "#BP Breakpoint Exception");
  intr_register_int (4, 3, INTR_ON, kill, "#OF Overflow Exception");
  intr_register_int (5, 3, INTR_ON, kill,
                     "#BR BOUND Range Exceeded Exception");

  /* These exceptions have DPL==0, preventing user processes from
     invoking them via the INT instruction.  They can still be
     caused indirectly, e.g. #DE can be caused by dividing by
     0.  */
  intr_register_int (0, 0, INTR_ON, kill, "#DE Divide Error");
  intr_register_int (1, 0, INTR_ON, kill, "#DB Debug Exception");
  intr_register_int (6, 0, INTR_ON, kill, "#UD Invalid Opcode Exception");
  intr_register_int (7, 0, INTR_ON, kill,
                     "#NM Device Not Available Exception");
  intr_register_int (11, 0, INTR_ON, kill, "#NP Segment Not Present");
  intr_register_int (12, 0, INTR_ON, kill, "#SS Stack Fault Exception");
  intr_register_int (13, 0, INTR_ON, kill, "#GP General Protection Exception");
  intr_register_int (16, 0, INTR_ON, kill, "#MF x87 FPU Floating-Point Error");
  intr_register_int (19, 0, INTR_ON, kill,
                     "#XF SIMD Floating-Point Exception");

  /* Most exceptions can be handled with interrupts turned on.
     We need to disable interrupts for page faults because the
     fault address is stored in CR2 and needs to be preserved. */
  intr_register_int (14, 0, INTR_OFF, page_fault, "#PF Page-Fault Exception");

  /* Start of Project 3 */
  lock_init (&exception_lock);
  /* End of Project 3 */
}

/* Prints exception statistics. */
void
exception_print_stats (void) 
{
  printf ("Exception: %lld page faults\n", page_fault_cnt);
}

/* Handler for an exception (probably) caused by a user process. */
static void
kill (struct intr_frame *f) 
{
  /* This interrupt is one (probably) caused by a user process.
     For example, the process might have tried to access unmapped
     virtual memory (a page fault).  For now, we simply kill the
     user process.  Later, we'll want to handle page faults in
     the kernel.  Real Unix-like operating systems pass most
     exceptions back to the process via signals, but we don't
     implement them. */
     
  /* The interrupt frame's code segment value tells us where the
     exception originated. */
  switch (f->cs)
    {
    case SEL_UCSEG:
      /* User's code segment, so it's a user exception, as we
         expected.  Kill the user process.  */
      printf ("%s: dying due to interrupt %#04x (%s).\n",
              thread_name (), f->vec_no, intr_name (f->vec_no));
      intr_dump_frame (f);
      thread_exit (); 

    case SEL_KCSEG:
      /* Kernel's code segment, which indicates a kernel bug.
         Kernel code shouldn't throw exceptions.  (Page faults
         may cause kernel exceptions--but they shouldn't arrive
         here.)  Panic the kernel to make the point.  */
      intr_dump_frame (f);
      PANIC ("Kernel bug - unexpected interrupt in kernel"); 

    default:
      /* Some other code segment?  Shouldn't happen.  Panic the
         kernel. */
      printf ("Interrupt %#04x (%s) in unknown segment %04x\n",
             f->vec_no, intr_name (f->vec_no), f->cs);
      thread_exit ();
    }
}

/* Page fault handler.  This is a skeleton that must be filled in
   to implement virtual memory.  Some solutions to project 2 may
   also require modifying this code.

   At entry, the address that faulted is in CR2 (Control Register
   2) and information about the fault, formatted as described in
   the PF_* macros in exception.h, is in F's error_code member.  The
   example code here shows how to parse that information.  You
   can find more information about both of these in the
   description of "Interrupt 14--Page Fault Exception (#PF)" in
   [IA32-v3a] section 5.15 "Exception and Interrupt Reference". */
static void
page_fault (struct intr_frame *f) 
{
  bool not_present;  /* True: not-present page, false: writing r/o page. */
  bool write;        /* True: access was write, false: access was read. */
  bool user;         /* True: access by user, false: access by kernel. */
  void *fault_addr;  /* Fault address. */
  bool success = false;
  struct thread *cur = running_thread ();
  uint32_t *prev_page;

  /* Obtain faulting address, the virtual address that was
     accessed to cause the fault.  It may point to code or to
     data.  It is not necessarily the address of the instruction
     that caused the fault (that's f->eip).
     See [IA32-v2a] "MOV--Move to/from Control Registers" and
     [IA32-v3a] 5.15 "Interrupt 14--Page Fault Exception
     (#PF)". */
  asm ("movl %%cr2, %0" : "=r" (fault_addr));

  /* Turn interrupts back on (they were only off so that we could
     be assured of reading CR2 before it changed). */
  intr_enable ();

  /* Count page faults. */
  page_fault_cnt++;

  /* Determine cause. */
  not_present = (f->error_code & PF_P) == 0;
  write = (f->error_code & PF_W) != 0;
  user = (f->error_code & PF_U) != 0;
  
  /* Start of Project 3 */

  uint32_t *vaddr = pg_round_down (fault_addr);
  void *esp = user? f->esp : cur->fault_esp;  

  if (not_present && is_user_vaddr(fault_addr))
  {
     /* Get the entry from the SUPPLEMENTAL_TABLE. */
     struct sup_page *spte = sup_page_lookup (cur, fault_addr); 
     if (spte != NULL)
     {
       spte->pinned = true;
       if (spte->ptype == CODE && spte->status == IN_FILE)
       {
         bool writable = (spte->ptype == CODE) ? false : true ;
         /* Get the entry from Supplemental_Page_Table for input fault_addr. */
	 //enum page_type pgtype = spte->writable ? DATA : CODE;
	 //lock_acquire (&exception_lock);
	 //lock_acquire (&file_lock);
	 success = load_segment (spte->file, spte->ofs, spte->vaddr, spte->read_bytes,
	 			 (PGSIZE-spte->read_bytes), writable);
         spte->pinned = false;
	 //lock_release (&file_lock);
	 //lock_release (&exception_lock);
       }
       /***
       else if (spte->ptype == DATA && spte->status == IN_FILE)
       {
         file_read_at (spte->file, spte->vaddr, spte->read_bytes, spte->ofs);
       }
       ***/
       else // if (spte->status == IN_SWAP || spte->status == )
       {
         /* Getting the page in the Swap Disk back into the memory */
         // swp_pg = swap_page_lookup (cur, fault_addr);
	 //lock_acquire (&exception_lock);
	 success = swap_in (spte);
         spte->pinned = false;
	 //lock_release (&exception_lock);
       }
     }
    else
    {
      if( //((cur->stack_seg.start_addr + MAX_STACK_SIZE) < fault_addr)
         ((PHYS_BASE - MAX_STACK_SIZE) < (uint32_t *)fault_addr)
	   && fault_addr >= (esp - 32))
      {
        /* Grow the stack. */
        uint32_t  *paddr = palloc_get_frame (PAL_USER);
        memset (paddr, 0, PGSIZE);
        success = install_frame (vaddr, paddr, true, NULL, 0, DATA, PGSIZE, IN_MEMORY);
        cur->stack_seg.end_addr = vaddr;

	spte = sup_page_lookup (cur, vaddr);
	spte->pinned = true;
        //sup_page_install (vaddr, paddr, NULL, 0, DATA, PGSIZE, IN_MEMORY); 
      }
      /***
      else
      {
	* Grow heap size if fault_addr is on next page of last allotted heap page. *
        prev_page = pg_round_down (vaddr-1);
	struct sup_page *spte_prev = sup_page_lookup (cur, prev_page);
	if (spte_prev != NULL)
	{
	  uint32_t  *paddr = palloc_get_frame (PAL_USER);
	  success = install_frame (vaddr, paddr, true, NULL, 0, DATA, 0, IN_MEMORY);
	}
      }
      ***/
    }
  }
  /* End of Project 3 */
  
  if(!success)
    /* When a page fault occurs, we simply exit with status as -1. */
    user_exit(-1);				/* Project 2 */

  return;
}

