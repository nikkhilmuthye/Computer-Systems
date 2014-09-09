
#ifdef USERPROG
#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "frame.h"
#include "threads/malloc.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#ifdef VM
#include "userprog/pagedir.h"
#endif

// Queue to manage the frames in the memory.
static struct list lru_queue;

// Lock for the frame table. Helpful in synchronization.
static struct lock frame_lock, delete_lock;

// Used in the eviction algorithm. This pointer would act as the 
// clock head in the clock algorithm.
static struct list_elem * e = NULL;

/* Initialise the locks and the frame table. */
void
frame_init (void)
{
  list_init (&lru_queue);
  lock_init (&frame_lock);
  lock_init (&delete_lock);
}

/* Goes through the list and finds a frame whose free bit is true. */
static struct frame *
get_available_frame (void)
{
  struct frame *frame;
  struct list_elem *e;

  for (e = list_begin (&lru_queue);
       e != list_end (&lru_queue);
       e = list_next (e))
  {
     frame = list_entry (e, struct frame, elem);

     if (frame->free == true)
       return frame;
  }
  return NULL;
}

/* Wrapper for the palloc_get_page (). Along with the page allocation, we
 * have to allocate a frame for the virtual page as well. If there are no
 * free frames in the table, we would allocate a new frame. If the frame 
 * table is full, we would perform the clock algorithm to evict a suitable
 * frame from the frame table who has not been accessed for a while. */
void *
palloc_get_frame (enum palloc_flags flags)
{
  struct frame *f;
  void *kpage;
  bool lock_held = false;

  if (frame_lock.holder != running_thread ())
  {
    lock_acquire (&frame_lock);
    lock_held = true;
  }
  f = get_available_frame ();

  if (f == NULL)
  {
    kpage = palloc_get_page (flags);
    if (kpage == NULL)
    {
      /* Evict frame */
      //lock_acquire (&frame_lock);
      f = evict_frame ();
      kpage = f->phy_addr;
      list_remove (&f->elem);
      //lock_release (&frame_lock);
    }
    else
    {
      f = (struct frame *) malloc (sizeof (struct frame));
      f->phy_addr = kpage;
    }
  }
  else
  {
    kpage = f->phy_addr;
    list_remove (&f->elem);
  }

  f->free = false;
  list_push_back (&lru_queue, &f->elem);

  if (lock_held)
    lock_release (&frame_lock);

  return kpage;

}

/* When all the frames are allocated in the physical memory and there is 
 * no room left to create a new frame, we will have to find a suitable
 * frame which can be evicted and written into the swap disk until it is
 * required again. We have implmented the clock algorithm for selecting the 
 * suitable frame. */
struct frame *
evict_frame (void)
{
  struct frame *f = NULL;
  struct frame *victim;
  struct sup_page *spte;
  struct thread *cur = running_thread ();

  if (e == NULL)
  {
    e = (list_begin (&lru_queue));
  }
  while (true)
  {
    f = list_entry (e, struct frame, elem);

    spte = sup_page_lookup (f->process, f->vaddr);

    if (!spte->pinned && !pagedir_is_accessed(f->process->pagedir, f->vaddr))
    {
      victim = f;
      break;
    }
   
    if (!spte->pinned)
      pagedir_set_accessed(f->process->pagedir, f->vaddr, false);
    e = list_next (e);
    if (e == list_end (&lru_queue))
      e = (list_begin (&lru_queue));
  }

  bool status = swap_out_frame (victim);

  if (status)
  {
    //pagedir_clear_page(victim->process->pagedir, victim->vaddr);
    victim->process = NULL;
    victim->vaddr = NULL;
    victim->write = false;
    victim->free = true;

    //memset (victim->phy_addr, 0, PGSIZE);
    return victim;
  }

  return NULL;

}

/* Wrapper around the install_page (). We have to install the frame
 * alongwith the page. */
bool
install_frame (uint32_t *upage, uint32_t *kpage, bool writable, struct file *f,
	       off_t ofs, enum page_type pt, uint32_t read_bytes, 
               enum swap_status status)
{
  bool ret_status = false;
  bool lock_held = false;

  if (frame_lock.holder != running_thread ())
  {
    lock_acquire (&frame_lock);
    lock_held = true;
  }
  /* Lookup hash table */
  struct frame *frame = look_up_frame (kpage);

  /* Insert record in FRAME_TABLE entry. */
  if (frame != NULL)
  {
    frame->process = running_thread ();
    //frame->phy_addr = kpage;
    frame->vaddr = upage;
    frame->write = writable;
    frame->free = false;

    /* Insert record in SUPPLEMENT_TABLE. */
    sup_page_update (upage, kpage, f, ofs, pt, read_bytes, status);

    ret_status = install_page (upage, kpage, writable);
  }
  if (lock_held)
    lock_release (&frame_lock);

  return ret_status;
}

/* Used to find a particular frame in the frame table. */
struct frame *
look_up_frame (uint32_t *paddr)
{
  struct frame *f;

  struct list_elem *e;
  for (e = list_begin (&lru_queue); e != list_end (&lru_queue);
       e = list_next (e))
  {
    f = list_entry (e, struct frame, elem);
    if(f->phy_addr == paddr)
      return f;
  }
  return NULL;
}

/* Called from thread_exit (). We would set the free bit as true 
 * and memset the frame to 0.  */
void
free_frame (uint32_t *paddr)
{
  bool lock_held = false;
  if (!lock_held_by_current_thread (&delete_lock))
  {
     lock_acquire (&delete_lock);
     lock_held = true;
  }

  struct frame *f;
  struct thread *cur = running_thread ();
  struct list_elem *e; 


  f = look_up_frame (paddr);

  if (f !=  NULL)
  {
    f->free = true;
    memset (paddr, 0, PGSIZE);
  }

  if (lock_held)
    lock_release (&delete_lock);

}


/* Called from exception.c from the page_fault(). We need to 
 * write the datat which is dirty into the swap disk. If the data
 * on the page is not dirty, we write it back into the file.  */
bool swap_out_frame (struct frame *f)
{
  block_sector_t swaddr;
  struct sup_page *spte = sup_page_lookup (f->process, f->vaddr);

  if (spte->ptype == DATA)
  {
    swaddr = swap_out (f->phy_addr);
    spte->swaddr = swaddr;
    spte->status = IN_SWAP;
  }
  else if (pagedir_is_dirty(f->process->pagedir, f->vaddr))
  {
    if (spte->ptype != MMAP)
    {
      swaddr = swap_out (f->phy_addr);
      spte->swaddr = swaddr;
      spte->status = IN_SWAP;
    }
    else
    {
      file_write_at (spte->file, f->phy_addr, spte->read_bytes, spte->ofs);
      spte->status = IN_FILE;
    }
  }
  else
    spte->status = IN_FILE;

  pagedir_clear_page(f->process->pagedir, spte->vaddr);
  memset(f->phy_addr,0, PGSIZE);

  return true;
}

#endif
