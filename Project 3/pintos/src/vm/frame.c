
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


static struct list lru_queue;

static struct lock frame_lock, delete_lock;

static struct hash frame_table;

//static struct frame *evict_frame (void);

static struct list_elem * e = NULL;

void
frame_init (void)
{
  list_init (&lru_queue);
  lock_init (&frame_lock);
  lock_init (&delete_lock);
  hash_init (&frame_table, hash_func, less_func, NULL);
}


unsigned
hash_func (const struct hash_elem *f_, void *aux)
{
  const struct frame *f = hash_entry (f_, struct frame, frame_elem);
  return hash_bytes (&f->phy_addr, sizeof f->phy_addr);
}


bool
less_func (const struct hash_elem *a_,
           const struct hash_elem *b_,
           void *aux)
{
  const struct frame *a = hash_entry (a_, struct frame, frame_elem);
  const struct frame *b = hash_entry (b_, struct frame, frame_elem);

  return a->phy_addr < b->phy_addr;
}


static struct frame *
get_available_frame (void)
{
  struct frame *frame;
  struct list_elem *e;

  for (e = list_begin (&lru_queue);
       e != list_end (&lru_queue);
       e = list_next (e))
  {
     frame = list_entry (e, struct frame, q_elem);

     if (frame->free)
       return frame;
  }
  return NULL;
}


void *
palloc_get_frame (enum palloc_flags flags)
{
  struct frame *f;
  uint8_t *kpage;

  lock_acquire (&frame_lock);
  f = get_available_frame ();

  //if (flags == PAL_USER)
  //{

    if (f == NULL)
    {
      kpage = palloc_get_page (flags);
      if (kpage == NULL)
      {
        /* Evict frame */
        //lock_acquire (&frame_lock);
        f = evict_frame ();
        list_remove (&f->q_elem);
        struct hash_elem *temp = hash_delete (&frame_table, &f->frame_elem);
        //lock_release (&frame_lock);
        // kpage = palloc_get_page (flags);
      }
      else
      {
        f = (struct frame *) malloc (sizeof (struct frame));
        f->phy_addr = kpage;
        //list_init(&f->user_pages);
      }
    }
    else
    {
      list_remove (&f->q_elem);
      struct hash_elem *temp = hash_delete (&frame_table, &f->frame_elem);
    }

    f->free = false;
    list_push_back (&lru_queue, &f->q_elem);
    hash_insert (&frame_table, &f->frame_elem);
  //}
  lock_release (&frame_lock);

  return (void *)f->phy_addr;

}

struct frame *
evict_frame (void)
{
  struct frame *f = NULL;
  struct frame *victim;
  struct sup_page *spte;
  //struct list_elem *initial = e;
  struct thread *cur = running_thread ();
  // f = (list_entry (list_begin (&lru_queue);

  if (e == NULL)
  {
    e = (list_begin (&lru_queue));
  }
  while (true)
  {
    f = list_entry (e, struct frame, q_elem);

    spte = sup_page_lookup (f->process, f->vaddr);

    if (!spte->pinned && !pagedir_is_accessed(f->process->pagedir, f->vaddr))
    {
      victim = f;
      f->free == true;
      break;
    }
   
    if (!spte->pinned)
      pagedir_set_accessed(f->process->pagedir, f->vaddr, false);
    e = list_next (e);
    //if (is_tail (e))
    if (e == list_end (&lru_queue))
      e = (list_begin (&lru_queue));
  }

  bool status = swap_out (victim);

  if (status)
  {
    pagedir_clear_page(victim->process->pagedir, victim->vaddr);
    victim->process = NULL;
    victim->vaddr = NULL;
    victim->write = false;
    victim->free = true;

    //memset (victim->phy_addr, 0, PGSIZE);
    return victim;
  }

  return NULL;

}


bool
install_frame (uint32_t *upage, uint32_t *kpage, bool writable, struct file *f,
	       off_t ofs, enum page_type pt, uint32_t read_bytes, enum swap_status status)
{
  /* Lookup hash table */
  struct frame *frame = look_up_frame (kpage);

  /* Insert record in FRAME_TABLE entry. */
  if (frame != NULL)
  {
    frame->process = running_thread ();
    frame->phy_addr = kpage;
    frame->vaddr = upage;
    frame->write = writable;
    frame->free = false;

    /* Insert record in SUPPLEMENT_TABLE. */
    sup_page_update (upage, kpage, f, ofs, pt, read_bytes, status);

    return install_page (upage, kpage, writable);
  }

  return false;

}


struct frame *
look_up_frame (uint32_t *paddr)
{
  struct frame f;
  struct frame *e;
  f.phy_addr = paddr;
  struct hash_elem *h_elem = hash_find (&frame_table, &(f.frame_elem));

  if (h_elem == NULL)
    return NULL;

  e = hash_entry (h_elem, struct frame, frame_elem);
  return e;
}

void
free_frame (uint32_t *paddr)
{
  bool lock_held = false;
  /*if (!lock_held_by_current_thread (&delete_lock))
  {
     lock_held = true;*/
     //lock_acquire (&delete_lock);
  //}

  struct frame *f;
  struct thread *cur = running_thread ();
  struct list_elem *e; 

  struct frame frm;
  frm.phy_addr = paddr;
  struct hash_elem *h_elem = hash_find (&frame_table, &(frm.frame_elem));

  f = hash_entry (h_elem, struct frame, frame_elem);

/***
  struct hash_elem *temp = hash_delete (&frame_table, &f->frame_elem);
  list_remove (&f->q_elem);
  free (f);
***/

  for (e = list_begin(&lru_queue); e = !list_end (&lru_queue); e = list_next(e))
  {
    f = list_entry (e, struct frame, q_elem);

    //if (f->process == NULL)
      //continue;

    if (f->process->tid == cur->tid && f->phy_addr == paddr)
    {
      f->free = true;
      memset (paddr, 0, PGSIZE);
      break;
    }
  }

  //if (lock_held)
    //lock_release (&delete_lock);

}


void 
write_back_mmap (void)
{
  struct frame *f;
  struct thread *cur = running_thread ();
  struct list_elem *e;
  struct sup_page *spte;
  uint32_t write_bytes;
  for (e = list_begin(&lru_queue); e != list_end (&lru_queue); e = list_next(e))
  {
    f = list_entry (e, struct frame, q_elem);
    if(f->process->tid == cur->tid)
    {
      spte = sup_page_lookup (cur, f->vaddr);
      if (spte->ptype == MMAP && spte->status == IN_MEMORY)
         write_bytes = file_write_at (spte->file, spte->paddr, spte->read_bytes, spte->ofs);
    }
  }
}


#endif
