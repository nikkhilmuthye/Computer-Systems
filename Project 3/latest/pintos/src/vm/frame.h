
#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <debug.h>
#include <list.h>
#include <hash.h>
#include "threads/thread.h"

#define MAX_STACK_SIZE (8 * 1024 * 1024)


struct frame
  {
    uint32_t *phy_addr;
    // struct list user_pages;
    struct thread *process;
    uint32_t *vaddr;
    bool write;
    bool free;
    struct list_elem elem;
    //struct hash_elem frame_elem;
  };


struct user_page
  {
    struct thread *process;
    void *upage;
    struct list_elem elem;
  };


void frame_init (void);
unsigned hash_func (const struct hash_elem *, void *);
bool less_func (const struct hash_elem *, const struct hash_elem *, void *);
void * palloc_get_frame (enum palloc_flags);
bool install_frame (uint32_t *, uint32_t *, bool , struct file *,
               	    off_t , enum page_type, uint32_t, enum swap_status);
struct frame * look_up_frame (uint32_t *);
struct frame *evict_frame (void);
void free_frame (uint32_t *);
void free_all_frames (void);
bool swap_out_frame (struct frame *);
#endif
