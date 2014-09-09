
#ifndef VM_SWAP_H
#define VM_SWAP_H

#include <debug.h>
#include <list.h>
#include <hash.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "devices/block.h"

#define BLOCKSPERPAGE (PGSIZE/BLOCK_SECTOR_SIZE)

void swap_init (void);
unsigned swp_hash_func (const struct hash_elem *, void *);
bool swp_less_func (const struct hash_elem *,
                    const struct hash_elem *,
                    void *);
void swap_in (block_sector_t, void *);
block_sector_t swap_out (struct frame *);
void swap_clear (block_sector_t);
void swap_bitmap_update (uint32_t, bool);
#endif