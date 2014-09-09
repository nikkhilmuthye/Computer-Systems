#ifndef CACHE_H
#define CACHE_H

#include "devices/block.h"
#include "threads/synch.h"
#include <list.h>

#define MAX_CACHE_SIZE 64
#define WRITE_SLEEP_INTERVAL 5


struct cached_block
  {
    block_sector_t sector;
    uint8_t data[BLOCK_SECTOR_SIZE];
    bool accessed;
    bool dirty;
    int open;
    struct list_elem elem;
  };

void cache_init (void);
struct cached_block * get_cached_block (block_sector_t, bool);
struct cached_block * lookup_cache (block_sector_t);
struct cached_block * evict_cache_block (void);
void write_behind_thread (void *);
void cache_write_behind (void);

#endif
