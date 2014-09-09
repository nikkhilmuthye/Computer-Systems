
#ifndef VM_PAGE_H
#define VM_PAGE_H

#include <debug.h>
#include <list.h>
#include <hash.h>
#include "threads/thread.h"
#include "threads/palloc.h"
#include "filesys/off_t.h"
#include "filesys/file.h"
#include "devices/block.h"

enum swap_status
  {
    IN_MEMORY,
    IN_SWAP,
    IN_FILE
  };

enum page_type
  {
    MMAP,
    CODE,
    DATA
  };

struct sup_page
  {
    uint32_t *vaddr;
    uint32_t *paddr;
    enum page_type ptype;
    enum swap_status status;
    struct file *file;
    off_t ofs;
    uint32_t read_bytes;
    block_sector_t swaddr;
    bool pinned;
    struct hash_elem elem;
  };


unsigned sup_hash_func (const struct hash_elem *, void *);
bool sup_less_func (const struct hash_elem *,
                    const struct hash_elem *,
                    void *);
struct sup_page * sup_page_lookup (struct thread *, uint32_t *);
void load_sup_page_table (struct file *, off_t, uint32_t,
                     	  uint32_t, uint32_t, bool);
void sup_page_update (uint32_t *, uint32_t *, struct file *,
                      off_t, enum page_type, uint32_t, enum swap_status);
//void update_on_swap_out (struct frame *, enum swap_status, uint32_t);
int mmap_allocate_spt (struct file *, uint32_t *, uint32_t);
void sup_page_destroy (struct hash_elem *, void *);
#endif
