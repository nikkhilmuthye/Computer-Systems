#ifndef FILESYS_INODE_H
#define FILESYS_INODE_H

#include <stdbool.h>
#include "filesys/off_t.h"
#include "devices/block.h"

#define MAX_FILE_SIZE (8 * 1024 * 1024)

/* Start of Project 4 */
#define MAX_BLOCK_INODE 15
#define NUMBER_OF_DIRECT_BLOCKS 12
#define NUMBER_OF_INDIRECT_BLOCKS 2
#define NUMBER_OF_DOUBLE_IBLOCKS 1
#define INDIRECT_BLOCK_PTRS 128
#define INODE_BLOCK_PTRS 14

#define DIRECT_BLOCK_INDEX 0
#define INDIRECT_BLOCK_INDEX 12
#define DOUBLE_INDIRECT_BLOCK_INDEX 14

struct indirect_block
  {
    block_sector_t ptrs[INDIRECT_BLOCK_PTRS];
  };
/* End of Project 4 */

struct bitmap;

void inode_init (void);
bool inode_create (block_sector_t, off_t, bool);		/* Project 4 */
struct inode *inode_open (block_sector_t);
struct inode *inode_reopen (struct inode *);
block_sector_t inode_get_inumber (const struct inode *);
void inode_close (struct inode *);
void inode_remove (struct inode *);
off_t inode_read_at (struct inode *, void *, off_t size, off_t offset);
off_t inode_write_at (struct inode *, const void *, off_t size, off_t offset);
void inode_deny_write (struct inode *);
void inode_allow_write (struct inode *);
off_t inode_length (const struct inode *);

/* Start of Project 4 */
void lock_acquire_inode (struct inode *);
void lock_release_inode (struct inode *);
block_sector_t inode_get_parent (struct inode *);
int inode_get_open_cnt (struct inode *);
block_sector_t inode_get_sector (struct inode *);
bool inode_is_dir (struct inode *);
void inode_set_parent (block_sector_t, struct inode *);
/* End of Project 4 */

#endif /* filesys/inode.h */
