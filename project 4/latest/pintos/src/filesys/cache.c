#include "threads/thread.h"
#include "threads/malloc.h"
#include "filesys/filesys.h"
#include "filesys/cache.h"
#include "devices/timer.h"


struct list cache_list;
struct lock cache_lock;
int entry_count;

static struct list_elem *e = NULL;

void
cache_init (void)
{
  list_init (&cache_list);
  lock_init (&cache_lock);
  entry_count = 0;
  //thread_create ("write_behind_thread", PRI_MIN, write_behind_thread, NULL);
}


struct cached_block *
get_cached_block (block_sector_t sector, bool dirty)
{
  lock_acquire (&cache_lock);

  struct cached_block *cb = lookup_cache (sector);

  if (cb == NULL)
  {
    if (entry_count < MAX_CACHE_SIZE)
    {
      cb = (struct cached_block *) malloc (sizeof (struct cached_block));
      if (cb == NULL)
      {
        lock_release (&cache_lock);
        PANIC ("ERROR : Main and Cache memory full.");
      }
      cb->open = 0;
      entry_count++;
    }
    else
    {
      cb = evict_cache_block ();
      list_remove (&cb->elem);
    }
    cb->sector = sector;
    block_read (fs_device, sector, cb->data);
    cb->accessed = true;
    cb->open++;
    cb->dirty = dirty;
    list_push_back (&cache_list, &cb->elem);
  }
  else
  {
    cb->accessed = true;
    cb->open++;
    cb->dirty |= dirty;
  }
  lock_release (&cache_lock);
  return cb;
}



struct cached_block *
lookup_cache (block_sector_t sector)
{
  struct cached_block *cb;
  struct list_elem *e;

  for (e = list_begin(&cache_list); e != list_end(&cache_list);
       e = list_next(e))
    {
      cb = list_entry(e, struct cached_block, elem);
      if (cb->sector == sector)
	{
	  return cb;
	}
    }
  return NULL;
}

struct cached_block *
evict_cache_block (void)
{

  struct cached_block *cb;

  if (e == NULL)
    e = list_begin (&cache_list);
  
  while (true)
  {
    cb = list_entry (e, struct cached_block, elem);

    if (cb->open > 0)
      continue;
    else if (cb->accessed)
      cb->accessed = false;
    else
    {
      if (cb->dirty)
        block_write (fs_device, cb->sector, cb->data);
      break;
    }

    e = list_next (e);
    if (e == list_end (&cache_list))
      e = list_begin (&cache_list);
  }

  return cb;

}


void
write_behind_thread (void *aux UNUSED)
{
  while (true)
  {
    timer_sleep (WRITE_SLEEP_INTERVAL);
    cache_write_behind ();
  }
}


void
cache_write_behind (void)
{
  lock_acquire (&cache_lock);
  struct list_elem *e;

  e = list_begin (&cache_list);

  //for (e = list_begin (&cache_list); e != list_end (&cache_lock);)// e = list_next (e))
  while (e != list_end (&cache_list))
  {
    struct cached_block *cb = list_entry (e, struct cached_block, elem);

    if (cb->dirty)
    {
      cb->dirty = false;
      block_write (fs_device, cb->sector, &(cb->data));
      e = list_next (e);
      //list_remove (&cb->elem);
      //free (cb);
    }
    else
      e = list_next (e);
  }
  lock_release (&cache_lock);
}

void
cache_flush (void)
{
  lock_acquire (&cache_lock);
  struct list_elem *e;

  e = list_begin (&cache_list);

  //for (e = list_begin (&cache_list); e != list_end (&cache_lock);)// e = list_next (e))
  while (e != list_end (&cache_list))
  {
    struct cached_block *cb = list_entry (e, struct cached_block, elem);

    if (cb->dirty)
    {
      cb->dirty = false;
      block_write (fs_device, cb->sector, &(cb->data));
    }
    e = list_next (e);
    list_remove (&cb->elem);
    free (cb);
  }
  lock_release (&cache_lock);
}

