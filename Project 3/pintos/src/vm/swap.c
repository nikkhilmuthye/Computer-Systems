#ifdef USERPROG
#include "threads/thread.h"
#include <debug.h>
#include <stddef.h>
#include <random.h>
#include <stdio.h>
#include <string.h>
#include "threads/malloc.h"
#include "threads/flags.h"
#include "threads/interrupt.h"
#include "threads/intr-stubs.h"
#include "threads/palloc.h"
#include "threads/switch.h"
#include "threads/synch.h"
#include "threads/vaddr.h"
#include "userprog/process.h"
#include "userprog/syscall.h"
#include "frame.h"
#include "page.h"
#include "swap.h"
#include "devices/block.h"
#include "lib/kernel/bitmap.h"
#include "userprog/pagedir.h"
#include "filesys/filesys.h"

struct lock swap_lock;

struct bitmap *swap_bitmap;
 
struct block *swap_device;

static bool 
update_on_swap_out (struct frame *, enum swap_status, uint32_t);

static bool
update_on_swap_in (struct sup_page *, uint32_t *);

void
swap_init (void)
{
  swap_device = block_get_role (BLOCK_SWAP);
  if (swap_device != NULL)
    swap_bitmap = bitmap_create ((size_t) swap_device->size);
  lock_init(&swap_lock); 
}
/*
unsigned 
swp_hash_func (const struct hash_elem *f_, void *aux)
{
  const struct swap_page *f = hash_entry (f_, struct swap_page, elem);
  return hash_bytes (&f->vaddr, sizeof f->vaddr);
}

    
bool
swp_less_func (const struct hash_elem *a_,
               const struct hash_elem *b_, 
               void *aux)
{   
  const struct swap_page *a = hash_entry (a_, struct swap_page, elem);
  const struct swap_page *b = hash_entry (b_, struct swap_page, elem);

  return a->vaddr < b->vaddr;
}


struct swap_page *
swap_page_lookup (struct thread *t, void *vaddr)
{
  struct swap_page swp;
  swp.vaddr = pg_round_down(vaddr);

  struct hash_elem *e = hash_find(&t->swap_table, &swp.elem);
  if (e == NULL)
    {
      return NULL;
    }
  return hash_entry (e, struct swap_page, elem);
}
*/


bool
swap_out (struct frame *frame)
{
  struct block *current_device; 
  uint32_t swaddr = 0;
  uint32_t write_bytes;
  enum swap_status sw_status;
  bool success = false;

  lock_acquire (&swap_lock);

  uint8_t *vaddr = frame->vaddr;
  struct thread *cur = frame->process;
  
  struct sup_page * spte = sup_page_lookup (cur, frame->vaddr);

  if (vaddr == NULL)
  {
    /* Invalied Virtual address */
    success = false;
  }  
  else if (spte->ptype == CODE)
  {
    sw_status = IN_FILE;
  }
  else if (spte->ptype == DATA)
  {
    //if (pagedir_is_dirty (cur->pagedir, vaddr))
    //{
      swaddr = bitmap_scan_and_flip (swap_bitmap, 0, BLOCKSPERPAGE, false);

      /* Write to disk or swap device */
      int i;
      for (i = 0; i < BLOCKSPERPAGE; i++)
        block_write (swap_device, swaddr + i, frame->phy_addr + BLOCK_SECTOR_SIZE * i);

      sw_status = IN_SWAP;
    //}
    //else
      //sw_status = IN_FILE;
  }
  else if (spte->ptype == MMAP)
  {
    /* Write memory mapped file to disk */
    if (pagedir_is_dirty (cur->pagedir, vaddr))
    {
      lock_acquire (&file_lock);
      write_bytes = file_write_at (spte->file, frame->phy_addr, spte->read_bytes, spte->ofs);
      lock_release (&file_lock);
    }
    sw_status = IN_FILE;
  }

  /* Update the frame and */
  success = update_on_swap_out (frame, sw_status, swaddr);

  lock_release (&swap_lock);

  return success;
}


static bool
update_on_swap_out (struct frame *frame, enum swap_status status, uint32_t swaddr)
{
  struct sup_page *spte = sup_page_lookup (frame->process, frame->vaddr);

  if (spte == NULL)
    return false;

  spte->paddr = NULL;
  spte->status = status;
  spte->swaddr = swaddr;

  //frame->process = NULL;
  //frame->vaddr = NULL;
  //frame->free = true;

  pagedir_clear_page (frame->process->pagedir, frame->vaddr);

  return true;
}

bool
swap_in (struct sup_page *spte)
{
  uint32_t *kpage, i;
  bool success = false;
  
  kpage = palloc_get_frame (PAL_USER);

  bool writable = spte->ptype == CODE ? false : true;
  
  install_frame (spte->vaddr, kpage, writable, spte->file, spte->ofs,
  		 spte->ptype, spte->read_bytes, spte->status);

  //lock_acquire (&swap_lock);

  if (spte->status == IN_FILE)
  {
    //int read_bytes = file_read_at (spte->file, spte->paddr, spte->read_bytes, spte->ofs);
    int read_bytes = file_read_at (spte->file, kpage, spte->read_bytes, spte->ofs);

    //if (spte->ptype == CODE && read_bytes < PGSIZE)
      //memset (kpage + read_bytes, 0, PGSIZE - read_bytes);
  }
  else if (spte->status == IN_SWAP)
  {
    for (i = 0; i < BLOCKSPERPAGE; i++)
    {
      //block_read (swap_device, spte->swaddr, spte->vaddr + BLOCK_SECTOR_SIZE * i);
      block_read (swap_device, spte->swaddr, kpage + BLOCK_SECTOR_SIZE * i);
    }

    bitmap_scan_and_flip (swap_bitmap, spte->swaddr, BLOCKSPERPAGE, true);
    pagedir_set_dirty (running_thread ()->pagedir, spte->vaddr, true);
  } /***
  else
  {
	palloc_free_frame (kpage);
	return false;
  } ***/

  success = update_on_swap_in (spte, kpage);
  //lock_release (&swap_lock);

  return success;

}

static bool
update_on_swap_in (struct sup_page *sp, uint32_t *paddr)
{
  if (sp == NULL)
    return false;

  sp->paddr = paddr;
  sp->status = IN_MEMORY;
  sp->swaddr = NULL;

  return true;
}

void
swap_bitmap_update (uint32_t swaddr, bool flag)
{
  swaddr = bitmap_scan_and_flip (swap_bitmap, swaddr, BLOCKSPERPAGE, flag);
}
#endif
