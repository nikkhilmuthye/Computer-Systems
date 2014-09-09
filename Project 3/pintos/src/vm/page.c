
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
#include "page.h"
#include "frame.h"
#include "swap.h"


unsigned
sup_hash_func (const struct hash_elem *f_, void *aux)
{
  const struct sup_page *f = hash_entry (f_, struct sup_page, elem);
  return hash_bytes (&f->vaddr, sizeof f->vaddr);
}


bool
sup_less_func (const struct hash_elem *a_,
               const struct hash_elem *b_,
               void *aux)
{
  const struct sup_page *a = hash_entry (a_, struct sup_page, elem);
  const struct sup_page *b = hash_entry (b_, struct sup_page, elem);

  return a->vaddr < b->vaddr;
}

void
sup_page_install (uint32_t *vaddr, uint32_t *paddr, struct file *f,
		  off_t ofs, enum page_type pt, uint32_t read_bytes, enum swap_status status)
{
  /* Get Supplemental Page Table for current thread. */
  struct hash *h = &(running_thread ()->sup_page_table);

  /* Setup new record for hash table entry. */
  struct sup_page *sp = (struct sup_page *) malloc (sizeof (struct sup_page));
  sp->vaddr = vaddr;
  sp->paddr = paddr;
  sp->ptype = pt;
  sp->status = status;
  sp->file = f;
  sp->ofs = ofs;
  sp->read_bytes = read_bytes;
  sp->pinned = false;

  /* Insert new entry in hash table. */
  hash_insert (h, &sp->elem);
}

void
sup_page_update (uint32_t *vaddr, uint32_t *paddr, struct file *f,
                 off_t ofs, enum page_type pt, uint32_t read_bytes, enum swap_status status)
{
  struct sup_page *spte = sup_page_lookup (running_thread (), vaddr);

  if (spte != NULL)
  {
    spte->status = status;
    spte->paddr = paddr;
  }
  else
    sup_page_install (vaddr, paddr, f, ofs, pt, read_bytes, status);
}

void
load_sup_page_table (struct file *file, off_t ofs, uint32_t upage,
		     uint32_t read_bytes, uint32_t zero_bytes, bool writable)
{
  uint32_t vaddr, file_ofs;
  vaddr = upage;
  file_ofs = ofs;

  while (read_bytes > 0 || zero_bytes > 0)
  {
    size_t page_read_bytes = read_bytes < PGSIZE ? read_bytes : PGSIZE;
    size_t page_zero_bytes = PGSIZE - page_read_bytes;
    enum page_type pgtype = writable ? DATA : CODE;
    sup_page_install ((void *) vaddr, NULL, file, file_ofs, pgtype, page_read_bytes, IN_FILE);
    vaddr += PGSIZE;
    file_ofs += page_read_bytes;
    read_bytes -= page_read_bytes;
    zero_bytes -= page_zero_bytes;
  }
}

struct sup_page * 
sup_page_lookup (struct thread *t, uint32_t *vaddr)
{
  struct sup_page spte;
  spte.vaddr = pg_round_down (vaddr);

  struct hash_elem *e = hash_find(&t->sup_page_table, &spte.elem);
  if (e == NULL)
    {
      return NULL;
    }
  return hash_entry (e, struct sup_page, elem);
}

mmapid_t
mmap_allocate_spt (struct file *file, uint32_t *vaddr, uint32_t fsize)
{
  uint32_t upage, file_ofs;
  struct mmap_file *mmap;
  int pages = 0;
  struct thread *cur = running_thread ();
  upage = vaddr;
  file_ofs = 0;

  while (fsize > 0)
  {
    size_t file_read_bytes = fsize < PGSIZE ? fsize : PGSIZE;
    struct sup_page *spte = sup_page_lookup (cur, upage);

    if (spte == NULL)
    {
      sup_page_install ((void *) upage, NULL, file, file_ofs, 
      			MMAP, file_read_bytes, IN_FILE);
      upage += PGSIZE;
      fsize -= file_read_bytes;
      file_ofs += file_read_bytes;
      pages++;
    }
    else
    {
      sup_page_mmap_cleanup (vaddr, pages);
      file_close (file);
      return -1;
    }
  }

  mmap = (struct mmap_file *) malloc (sizeof (struct mmap_file));
  mmap->file = file;
  mmap->start_addr = vaddr;
  mmap->pages = pages;

  if (list_empty (&(cur->mmap_files)))
  {
    mmap->mmid = 1;
  }
  else
  {
    mmap->mmid = (list_entry (list_rbegin (&(cur->mmap_files)),
    		  	     struct mmap_file, elem))->mmid + 1;
  }
  list_push_back (&(cur->mmap_files), &(mmap->elem));

  return (mmapid_t) mmap->mmid;

}

void
sup_page_mmap_cleanup (uint32_t vaddr, int pages)
{
  int i;
  struct sup_page *spte;
  struct thread *cur = running_thread ();

  for (i = 0; i < pages; i++)
  {
    spte = sup_page_lookup (cur, vaddr);
    if (spte->status == IN_MEMORY)
      free_frame (spte->paddr);
    hash_delete (&cur->sup_page_table, &spte->elem);
    vaddr += PGSIZE;
    free (spte);
  }
}

void
sup_page_destroy (struct hash_elem *h, void *aux)
{
  uint32_t write_bytes;
  struct thread *cur = running_thread ();
  struct sup_page *spte = hash_entry (h, struct sup_page, elem);

  if (spte->ptype == MMAP && spte->status == IN_MEMORY)
  {
    if (pagedir_is_dirty (cur->pagedir, spte->vaddr))
    {
      //lock_acquire (&file_lock);
      write_bytes = file_write_at (spte->file, spte->paddr, spte->read_bytes, spte->ofs);
      //lock_release (&file_lock);
    }

    free_frame (spte->paddr);
    pagedir_clear_page (cur->pagedir, spte->vaddr);
  }
  else if (spte->status == IN_MEMORY)
  {
    free_frame (spte->paddr);
    pagedir_clear_page (cur->pagedir, spte->vaddr);
  }
  else if (spte->status == IN_SWAP)
  {
    swap_bitmap_update (spte->swaddr, true);
  }

  hash_delete (&(cur->sup_page_table), h);
  //free (spte);
}

#endif
