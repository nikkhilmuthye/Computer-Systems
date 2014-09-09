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

// Used to access the swap disk. For synchronization,
struct lock swap_lock;

// Used to handle the free sectors on the swap disk.
struct bitmap *swap_bitmap;
 
// Used to send to the block functions so that it gets the required 
// device.
struct block *swap_device;

static bool 
update_on_swap_out (struct frame *, enum swap_status, uint32_t);

static bool
update_on_swap_in (struct sup_page *, uint32_t *);

/* Initialize the swap bitmap and the locks. */
void
swap_init (void)
{
  swap_device = block_get_role (BLOCK_SWAP);
  if (swap_device != NULL)
    swap_bitmap = bitmap_create ((size_t) swap_device->size);
  lock_init(&swap_lock); 
}

/* Used to swap out the pages.  */
block_sector_t
swap_out (struct frame *frame)
{
  bool lock_held = false;
  if (swap_lock.holder != running_thread ())
  {
    lock_acquire (&swap_lock);
    lock_held = true;
  }

  block_sector_t swaddr;

  swaddr = bitmap_scan_and_flip (swap_bitmap, 0, 8, false);

  /* Write to disk or swap device */
  if (swaddr != BITMAP_ERROR)
  {
    block_sector_t i;
    for (i = 0; i < 8; i++)
      block_write (swap_device, swaddr + i, frame->phy_addr + 512 * i);
  }
  else
  {
    if (lock_held)
      lock_release (&swap_lock);
    PANIC ("Swap Disk is full.");
  }

  if (lock_held)
    lock_release (&swap_lock);

  return swaddr;
}

/*   */
static bool
update_on_swap_out (struct frame *frame, enum swap_status status, uint32_t swaddr)
{
  struct sup_page *spte = sup_page_lookup (frame->process, frame->vaddr);

  if (spte == NULL)
    return false;

  spte->paddr = NULL;
  spte->status = status;
  spte->swaddr = swaddr;

  pagedir_clear_page (frame->process->pagedir, frame->vaddr);

  return true;
}

void
swap_in (block_sector_t swaddr, void *kpage)
{
  bool lock_held = false;
  int i;
  if (swap_lock.holder != running_thread ())
  {
    lock_acquire (&swap_lock);
    lock_held = true;
  }

  for (i = 0; i < 8; i++)
    block_read (swap_device, swaddr, kpage + 512 * i);

  bitmap_set_multiple (swap_bitmap, swaddr, 8, false);

  if (lock_held)
    lock_release (&swap_lock);
}

void
swap_clear (block_sector_t swaddr)
{
  bool lock_held = false;
  if (swap_lock.holder != running_thread ())
  {
    lock_acquire (&swap_lock);
    lock_held = true;
  }

  bitmap_set_multiple (swap_bitmap, swaddr, 8, false);

  if (lock_held)
    lock_release (&swap_lock);
}

void
swap_bitmap_update (uint32_t swaddr, bool flag)
{
  swaddr = bitmap_scan_and_flip (swap_bitmap, swaddr, BLOCKSPERPAGE, flag);
}
#endif
