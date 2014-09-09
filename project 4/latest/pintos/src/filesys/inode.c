#include "filesys/inode.h"
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "filesys/cache.h"
#include "threads/malloc.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44


/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    /* Start of Project 4 */
    //block_sector_t start;               /* First data sector. */
    off_t length;                       /* File size in bytes. */
    bool is_dir;			/* Specify if inode is for directory. */
    block_sector_t parent;		/* Sector of parent inode. */
    unsigned magic;                     /* Magic number. */
    block_sector_t ptrs[MAX_BLOCK_INODE];	/* Block sector pointers of file. */
    uint32_t dir_index;			/* Next Direct index. */
    uint32_t indir_index;		/* Next indirect index. */
    uint32_t double_indir_index;	/* Next double indirect index. */
    uint32_t unused[106];               /* Not used. */
    /* End of Project 4 */
  };

bool inode_alloc (struct inode_disk *);	/* Project 4 */

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise. */
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    /* Start of Project 4 */
    //struct inode_disk data;             /* Inode content. */
    struct lock lock;			/* Lock for synchronization. */
    uint32_t dir_index;			/* Next Direct index. */
    uint32_t indir_index;		/* Next indirect index. */
    uint32_t double_indir_index;	/* Next double indirect index. */
    block_sector_t parent;		/* Parent inode sector. */
    off_t file_length;			/* File length/size in bytes. */
    off_t read_length;			/* Actual readable length from file. */
    bool is_dir;			/* Specify if inode is for directory. */
    block_sector_t ptrs[MAX_BLOCK_INODE];
    /* End of Project 4 */
  };

/* Start of Project 4 */
void close_double_indirect_inode_block (block_sector_t *, size_t, size_t);
void close_indirect_inode_block (block_sector_t *, size_t);
void close_inode (struct inode *);
size_t expand_indir_for_double_indir_block (struct inode *, size_t, struct indirect_block *);
size_t expand_double_indirect_block (struct inode *, size_t);
size_t expand_indirect_block (struct inode *, size_t);
off_t expand_inode (struct inode *, off_t, bool);
size_t bytes_to_double_indirect_sector (off_t);
size_t bytes_to_indirect_sector (off_t);
size_t bytes_to_direct_sector (off_t);
bool alloc_inode (struct inode_disk *);
/* End of Project 4 */

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t length, off_t pos) 
{
  ASSERT (inode != NULL);

  uint32_t index, direct_block_idx, indirect_idx, indirect_ptrs[INDIRECT_BLOCK_PTRS];

  /* Start of Project 4 */
  if (length > pos)
  {
    if (pos < BLOCK_SECTOR_SIZE * NUMBER_OF_DIRECT_BLOCKS)
    {
      /* Direct Block is used. */
      direct_block_idx = pos / BLOCK_SECTOR_SIZE;
      return inode->ptrs[direct_block_idx];
    }
    else if (pos < BLOCK_SECTOR_SIZE * ( NUMBER_OF_DIRECT_BLOCKS
    				       + NUMBER_OF_INDIRECT_BLOCKS * INDIRECT_BLOCK_PTRS))
    {
      /* Indirect Block is used. */
      pos = pos - (BLOCK_SECTOR_SIZE * NUMBER_OF_DIRECT_BLOCKS);
      indirect_idx = pos / (BLOCK_SECTOR_SIZE * INDIRECT_BLOCK_PTRS) + NUMBER_OF_DIRECT_BLOCKS;

      /* Read Indirect block from file disk. */
      block_read (fs_device, inode->ptrs[indirect_idx], &indirect_ptrs);
      pos = pos % (BLOCK_SECTOR_SIZE * INDIRECT_BLOCK_PTRS);
      index = pos / BLOCK_SECTOR_SIZE;
      return indirect_ptrs[index];
    }
    else
    {
      /* Double Indirect Block is used. */
      /* Read double indirect block from disk. */
      block_read (fs_device, inode->ptrs[DOUBLE_INDIRECT_BLOCK_INDEX], &indirect_ptrs);
      pos = pos - (BLOCK_SECTOR_SIZE *  ( NUMBER_OF_DIRECT_BLOCKS
      					+ NUMBER_OF_INDIRECT_BLOCKS * INDIRECT_BLOCK_PTRS));
      indirect_idx = pos / (BLOCK_SECTOR_SIZE * INDIRECT_BLOCK_PTRS);
      /* Read indirect block from disk. */
      block_read(fs_device, indirect_ptrs[indirect_idx], &indirect_ptrs);
      pos %= BLOCK_SECTOR_SIZE * INDIRECT_BLOCK_PTRS;
      index = pos / BLOCK_SECTOR_SIZE;
      return indirect_ptrs[index];
    }
  }
  else
    return -1;
  /* End of Project 4 */
  /***
  if (pos < inode->data.length)
    return inode->data.start + pos / BLOCK_SECTOR_SIZE;
  else
    return -1;
  ***/
}

/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length, bool is_dir)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      /* Start of Project 4 */
      if (length > MAX_FILE_SIZE)
        disk_inode->length = MAX_FILE_SIZE;
      else
        disk_inode->length = length;
      disk_inode->parent = ROOT_DIR_SECTOR;
      disk_inode->is_dir = is_dir;
      disk_inode->magic = INODE_MAGIC;

      if (alloc_inode (disk_inode))
      {
        block_write (fs_device, sector, disk_inode);
	success = true;
      }
      free (disk_inode);
      /* End of Project 4 */
      /***
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      if (free_map_allocate (sectors, &disk_inode->start)) 
        {
          block_write (fs_device, sector, disk_inode);
          if (sectors > 0) 
            {
              static char zeros[BLOCK_SECTOR_SIZE];
              size_t i;
              
              for (i = 0; i < sectors; i++) 
                block_write (fs_device, disk_inode->start + i, zeros);
            }
          success = true; 
        } 
      free (disk_inode);
      ***/
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;
  struct inode_disk disk_inode;			/* Project 4 */

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;

  /* Start of Project 4 */
  //block_read (fs_device, inode->sector, &inode->data);
  lock_init(&inode->lock);
  block_read(fs_device, inode->sector, &disk_inode);
  inode->read_length = disk_inode.length;
  inode->file_length = disk_inode.length;
  inode->is_dir = disk_inode.is_dir;
  inode->parent = disk_inode.parent;
  inode->dir_index = disk_inode.dir_index;
  inode->indir_index = disk_inode.indir_index;
  inode->double_indir_index = disk_inode.double_indir_index;
  memcpy(&inode->ptrs, &disk_inode.ptrs, MAX_BLOCK_INODE * sizeof(block_sector_t));
  /* End of Project 4 */

  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;

  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          free_map_release (inode->sector, 1);
	  /* Start of Project 4 */
          //free_map_release (inode->data.start,
          //                  bytes_to_sectors (inode->data.length)); 
	  close_inode (inode);
	  /* End of Project 4 */
        }
      else
        {
	  struct inode_disk data;
	  data.length = inode->file_length;
	  data.parent = inode->parent;
	  data.is_dir = inode->is_dir;
	  data.dir_index = inode->dir_index;
	  data.indir_index = inode->indir_index;
	  data.double_indir_index = inode->double_indir_index;
	  data.magic = INODE_MAGIC;

	  memcpy (&data.ptrs, &(inode->ptrs), MAX_BLOCK_INODE * sizeof(block_sector_t));
	  block_write (fs_device, inode->sector, &data);
	}

      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;

  /* Start of Project 4 */
  off_t read_length = inode->read_length;
  if (read_length <= offset)
    return bytes_read;
  /* End of Project 4 */

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, read_length, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      //off_t inode_left = inode_length (inode) - offset;
      off_t inode_left = read_length - offset;				/* Project 4 */
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* Start of Project 4 */
      /***
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          * Read full sector directly into caller's buffer. *
          block_read (fs_device, sector_idx, buffer + bytes_read);
        }
      else 
        {
          * Read sector into bounce buffer, then partially copy
             into caller's buffer. *
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
          block_read (fs_device, sector_idx, bounce);
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      ***/

      /***/
      struct cached_block *cb = get_cached_block (sector_idx ,false);
      cb->open--;
      cb->accessed = true;

      memcpy (buffer + bytes_read, (uint8_t *)&cb->data + sector_ofs, chunk_size);
      /***/

      /* End of Project 4 */
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  //free (bounce); 			/* Project 4 */

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;

  if (inode->deny_write_cnt)
    return 0;

  if (offset + size > inode_length (inode))
  {
    if (!inode->is_dir)
      lock_acquire (&(inode->lock));
    inode->file_length = expand_inode (inode, offset + size, false);
    if (!inode->is_dir)
      lock_release (&(inode->lock));
  }

  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, inode_length (inode), offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      /* Start of Project 4 */
      /***
      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          * Write full sector directly to disk. *
          block_write (fs_device, sector_idx, buffer + bytes_written);
        }
      else 
        {
          * We need a bounce buffer. *
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          * If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. *
          if (sector_ofs > 0 || chunk_size < sector_left) 
            block_read (fs_device, sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          block_write (fs_device, sector_idx, bounce);
        }
      ***/

      /***/
      struct cached_block *cb = get_cached_block (sector_idx, true);
      cb->open--;
      cb->accessed = true;
      cb->dirty = true;

      memcpy ((uint8_t *)&cb->data + sector_ofs, buffer + bytes_written, chunk_size);
      /***/

      /* End of Project 4 */

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  inode->read_length = inode->file_length;
  //free (bounce); 			/* Project 4 */

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  //return inode->data.length;
  return inode->file_length;			/* Project 4 */
}


/* Start of Project 4 */

bool
alloc_inode (struct inode_disk *disk_inode)
{
  struct inode inode;

  inode.file_length = 0;
  inode.dir_index = 0;
  inode.indir_index = 0;
  inode.double_indir_index = 0;

  expand_inode (&inode, disk_inode->length, true);

  disk_inode->dir_index = inode.dir_index;
  disk_inode->indir_index = inode.indir_index;
  disk_inode->double_indir_index = inode.double_indir_index;
  memcpy(&disk_inode->ptrs, &inode.ptrs, MAX_BLOCK_INODE * sizeof(block_sector_t));

  return true;
}

bool
inode_is_dir (struct inode *inode)
{
  return inode->is_dir;
}

size_t
bytes_to_direct_sector (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

size_t
bytes_to_indirect_sector (off_t size)
{
  if (size <= BLOCK_SECTOR_SIZE * NUMBER_OF_DIRECT_BLOCKS)
    return 0;
  size -= BLOCK_SECTOR_SIZE * NUMBER_OF_DIRECT_BLOCKS;
  return DIV_ROUND_UP(size, BLOCK_SECTOR_SIZE * INDIRECT_BLOCK_PTRS);
}

size_t
bytes_to_double_indirect_sector (off_t size)
{
  if (size <= BLOCK_SECTOR_SIZE * ( NUMBER_OF_DIRECT_BLOCKS
				  + NUMBER_OF_INDIRECT_BLOCKS * INDIRECT_BLOCK_PTRS))
    return 0;

  return NUMBER_OF_DOUBLE_IBLOCKS;
}

off_t
expand_inode (struct inode *inode, off_t length, bool create_inode)
{
  static char zero_sector[BLOCK_SECTOR_SIZE];
  size_t new_sectors = bytes_to_sectors (length) - bytes_to_sectors (inode->file_length);

  if (new_sectors == 0)
    return length;

  if (create_inode)
  {
    if (free_map_count () < new_sectors)
      return 0;
  }

  /* Allocate direct sectors of inode */
  while (inode->dir_index < INDIRECT_BLOCK_INDEX)
  {
    uint32_t inode_idx = inode->dir_index;

    if (!free_map_allocate (1, &inode->ptrs[inode_idx]))
      return 0;

    block_write(fs_device, inode->ptrs[inode_idx], zero_sector);
    new_sectors--;
    inode->dir_index++;

    if (new_sectors == 0)
      return length;
  }

  /* Allocate indirect sectors of inode */
  while (inode->dir_index < DOUBLE_INDIRECT_BLOCK_INDEX)
  {
    new_sectors = expand_indirect_block (inode, new_sectors);

    if (new_sectors == 0)
      return length;
  }

  /* Allocate double indirect sectors of inode */
  if (inode->dir_index == DOUBLE_INDIRECT_BLOCK_INDEX)
  {
    new_sectors = expand_double_indirect_block (inode, new_sectors);
  }

  return (length - new_sectors*BLOCK_SECTOR_SIZE);

}

size_t
expand_indirect_block (struct inode *inode, size_t new_sectors)
{
  static char zero_sector[BLOCK_SECTOR_SIZE];
  struct indirect_block new_block;

  /* Check if new sectors needs to be allocated for indirect block.
   * Else read previous indirect block from disk and continue. */
  if (inode->indir_index == 0)
    free_map_allocate (1, &inode->ptrs[inode->dir_index]);
  else
    block_read(fs_device, inode->ptrs[inode->dir_index], &new_block);

  /* Allocate direct blocks from indirect block retrieved from above if.
   * Decrement new_sectors, if all required blocks are allocated then break. */
  while (inode->indir_index < INDIRECT_BLOCK_PTRS)
  {
    free_map_allocate(1, &new_block.ptrs[inode->indir_index]);
    block_write(fs_device, new_block.ptrs[inode->indir_index], zero_sector);
    inode->indir_index++;
    new_sectors--;

    if (new_sectors == 0)
      break;
  }

  block_write (fs_device, inode->ptrs[inode->dir_index], &new_block);
  
  /* Update the direct and indirect block indices in inode. */
  if (inode->indir_index == INDIRECT_BLOCK_PTRS)
  {
    inode->indir_index = 0;
    inode->dir_index++;
  }


  return new_sectors;
}

size_t
expand_double_indirect_block (struct inode *inode, size_t new_sectors)
{
  struct indirect_block new_block;

  /* Check if new sectors needs to be allocated for double indirect block.
   * Else read previous double indirect block from disk and continue. */
  if (inode->double_indir_index == 0 && inode->indir_index == 0)
    free_map_allocate (1, &inode->ptrs[inode->dir_index]);
  else
    block_read (fs_device, inode->ptrs[inode->dir_index], &new_block);

  while (inode->indir_index < INDIRECT_BLOCK_PTRS)
  {
    /* Expand the double indirect block to add a new block. */
    new_sectors = expand_indir_for_double_indir_block (inode, new_sectors, &new_block);

    if (new_sectors == 0)
      break;
  }

  block_write (fs_device, inode->ptrs[inode->dir_index], &new_block);

  return new_sectors;
}

size_t
expand_indir_for_double_indir_block (struct inode *inode, size_t new_sectors,
				     struct indirect_block *indir_block)
{
  struct indirect_block direct_block;
  static char zero_sector[BLOCK_SECTOR_SIZE];

  /* Check if new sectors needs to be allocated for indirect block.
   * Else read previous indirect block from disk and continue. */
  if (inode->double_indir_index == 0)
    free_map_allocate (1, &indir_block->ptrs[inode->indir_index]);
  else
    block_read (fs_device, indir_block->ptrs[inode->indir_index], &direct_block);

  /* Allocate direct blocks from indirect block retrieved from above if.
   * Decrement new_sectors, if all required blocks are allocated then break. */
  while (inode->double_indir_index < INDIRECT_BLOCK_PTRS)
  {
    free_map_allocate (1, &direct_block.ptrs[inode->double_indir_index]);
    block_write(fs_device, direct_block.ptrs[inode->double_indir_index], zero_sector);

    inode->double_indir_index++;
    new_sectors--;
    if (new_sectors == 0)
      break;
  }

  block_write (fs_device, indir_block->ptrs[inode->indir_index], &direct_block);

  /* Since double indirect block is not full, increment the indirect block index
   * and reset the double indirect block index. */
  if (inode->double_indir_index < INDIRECT_BLOCK_PTRS)
  {
    inode->double_indir_index = 0;
    inode->indir_index++;
  }

  return new_sectors;
}


void
close_inode (struct inode *inode)
{
  size_t direct_sectors = bytes_to_sectors (inode->file_length);
  size_t indirect_sectors = bytes_to_indirect_sector (inode->file_length);
  size_t double_indirect_sectors = bytes_to_double_indirect_sector (inode->file_length);

  size_t data_blocks = 0;
  uint32_t index = 0;

  /* Free all the direct blocks. */
  while (direct_sectors != 0 && index < INDIRECT_BLOCK_INDEX)
  {
    free_map_release (inode->ptrs[index], 1);
    index++;
    direct_sectors--;
  }

  /* Free all the indirect blocks. Call close_indirect_inode_block() for
   * each indirect inode block which will then expand the sector and
   * free all direct sectors in input indirect block. */
  while (indirect_sectors != 0 && index < DOUBLE_INDIRECT_BLOCK_INDEX)
  {
    if (direct_sectors < INDIRECT_BLOCK_PTRS)
      data_blocks = direct_sectors;
    else
      data_blocks = INDIRECT_BLOCK_PTRS;

    close_indirect_inode_block (&inode->ptrs[index], data_blocks);

    index++;
    direct_sectors -= data_blocks;
    indirect_sectors--;
  }

  /* Free all the sectors represented using double indirect block. */
  if (double_indirect_sectors != 0)
    close_double_indirect_inode_block (&inode->ptrs[index], indirect_sectors, direct_sectors);

}

void
close_indirect_inode_block (block_sector_t *indirect_block, size_t data_blocks)
{
  struct indirect_block sector;
  int i = 0;
 
  /* Read the indirect block from disk which contains array of direct blocks. */
  block_read (fs_device, *indirect_block, &sector);

  /* Free each direct block in input indirect_block. */
  while (i < data_blocks)
    free_map_release (sector.ptrs[i++], 1);

  /* Free the indirect block. */
  free_map_release (*indirect_block, 1);
}

void
close_double_indirect_inode_block (block_sector_t *double_indirect_block,
				   size_t indirect_block,
				   size_t data_blocks)
{
  struct indirect_block double_indirect_sector;
  size_t data_per_indirect;
  int i;

  /* Read the double indirect block from disk which contains array of indirect blocks. */
  block_read (fs_device, *double_indirect_block, &double_indirect_sector);

  /* Free each indirect block in input double indirect_block. */
  while (i < indirect_block)
  {
    if (data_blocks < INDIRECT_BLOCK_PTRS)
      data_per_indirect = data_blocks;
    else
      data_per_indirect = INDIRECT_BLOCK_PTRS;

    close_indirect_inode_block (&double_indirect_sector.ptrs[i], data_per_indirect);
    data_blocks -= data_per_indirect;
    i++;
  }

  /* Free the double indirect block. */
  free_map_release (*double_indirect_block, 1);
}

void
lock_acquire_inode (struct inode *inode)
{
  lock_acquire (&inode->lock);
}

void
lock_release_inode (struct inode *inode)
{
  lock_release (&inode->lock);
}

block_sector_t
inode_get_parent (struct inode *inode)
{
  return inode->parent;
}

int
inode_get_open_cnt (struct inode *inode)
{
  return inode->open_cnt;
}

block_sector_t
inode_get_sector (struct inode *inode)
{
  return inode->sector;
}

void
inode_set_parent (block_sector_t parent, struct inode *inode)
{
  inode->parent = parent;
}

/* End of Project 4 */
