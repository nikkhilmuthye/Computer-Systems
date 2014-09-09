#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "cache.h"
#include "threads/malloc.h"
#include "threads/thread.h"

/* Partition that contains the file system. */
struct block *fs_device;
char * retrieve_file_name (char *);
struct dir * dir_from_path (char *);
struct dir * dest_dir_from_path (char *);

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  cache_init ();				/* Project 4 */
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  /* Start of Project 4 */
  cache_flush ();
  /* End of Project 4 */
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size, bool is_dir) 
{
  block_sector_t inode_sector = 0;
  /* Start of Project 4 */
  bool success = false;
  struct dir *dir = dir_from_path (name);
  char* file_name = retrieve_file_name (name);
  if (strcmp(file_name, ".") != 0 && strcmp(file_name, "..") != 0)
  {
    success = (dir != NULL
               && free_map_allocate (1, &inode_sector)
  	       && inode_create (inode_sector, initial_size, is_dir)
	       && dir_add (dir, file_name, inode_sector));
  }
  free(file_name);
  /* End of Project 4 */
  if (!success && inode_sector != 0) 
    free_map_release (inode_sector, 1);
  dir_close (dir);

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  //struct dir *dir = dir_open_root ();
  /* Start of Project 4 */
  struct dir *dir = dir_from_path (name);
  char* file_name = retrieve_file_name (name);
  struct inode *dir_node = NULL;
  bool success = false;
 
  if (dir!= NULL)
  {
    //char *file_name = retrieve_file_name(dir);
    if (strcmp (file_name, "..") == 0)
    {
      success = retrieve_dir_parent (dir, &dir_node);
      if (!success)
      {
        free (file_name);
        return NULL;
      }
    }
    else if ((check_if_root_dir(dir) && strlen (file_name) == 0)
        || strcmp (file_name, ".") == 0)
    {
      free (file_name);
      return (struct file *) dir;
    }
    else
    {
      dir_lookup (dir, file_name, &dir_node);
      //success = true;
    }
  }

  dir_close (dir);
  free(file_name);

  if (!dir_node)
  {
    return NULL;
  }

  if (inode_is_dir (dir_node))
  {
    return (struct file *) dir_open(dir_node);
  }
  return file_open (dir_node);

  /***
  if (success && dir_node == NULL)
    return (struct file *)dir;
  else if (dir_node != NULL)
  {
    dir_close (dir);

    //if (check_if_root_dir(dir))
    if (inode_is_dir (dir_node))
      return (struct file *) dir_open(dir_node);
    else
      return file_open (dir_node);
  }
  else
  {
    dir_close (dir);
    return NULL;
  }
  ***/
  /* End of Project 4 */
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  //struct dir *dir = dir_open_root ();
  /* Start of Project 4 */
  struct dir *dir = dir_from_path (name);
  char* file_name = retrieve_file_name (name);
  bool success = false;

  if (dir != NULL)
    success = dir_remove (dir, file_name);
  else
    success = false;
  //bool success = dir != NULL && dir_remove (dir, file_name);
  free(file_name);
  /* End of Project 4 */

  dir_close (dir); 
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/* Start of Project 4 */
bool
change_directory (char *dir)
{
  struct inode *dir_node = NULL;
  struct dir *directory = dir_from_path (dir);
  struct thread *cur = thread_current ();
  char *file_name = retrieve_file_name(dir);
  bool success = false;

  if (directory != NULL)
  {
    if ((check_if_root_dir(directory) && strlen (file_name) == 0)
        || strcmp (file_name, ".") == 0)
    {
      cur->dir = directory;
      free(file_name);
      return true;
    }
    else if (strcmp (file_name, "..") == 0)
      success = retrieve_dir_parent (directory, &dir_node);
    else
    {
      dir_lookup (directory, file_name, &dir_node);
      success = true;
    }
  }

  free(file_name);
  if (!success)
    return success;
  dir_close (directory);
  directory = dir_open (dir_node);

  if (directory != NULL)
  {
    dir_close(cur->dir);
    cur->dir = directory;
    success = true;
  }
  else
    success = false;

  return success;
}

char *
retrieve_file_name (char *path)
{
  char *save_ptr, *token, *last = "";
  int length = strlen(path) + 1;
  char dir_path [length];

  memcpy(dir_path, path, length);
  token = strtok_r(dir_path, "/", &save_ptr);

  while (token != NULL)
  {
    last = token;
    token = strtok_r (NULL, "/", &save_ptr);
  }

  length = strlen(last) + 1;
  char *name = (char *) malloc (length);
  memcpy(name, last, length);
  return name;
}

struct dir *
dir_from_path (char *path)
{
  char *save_ptr, *token, *next = NULL;
  int length = strlen(path) + 1;
  char dir_path [length];
  struct dir *directory;
  struct thread *cur = thread_current ();
  struct inode *dir_node;
  bool success = false;

  memcpy (dir_path, path, length);
  token = strtok_r (dir_path, "/", &save_ptr);

  if (dir_path[0] == SLASH_SYM || cur->dir == NULL)
    directory = dir_open_root ();
  else
    directory = dir_reopen (cur->dir);
  
  if (token != NULL)
    next = strtok_r (NULL, "/", &save_ptr);

  while (next != NULL)
  {
    if (strcmp (token, ".") != 0)
    {
      if (strcmp (token, "..") != 0)
        success = dir_lookup (directory, token, &dir_node);
      else
        success = dir_lookup (directory, token, &dir_node);
      
      if (success)
      {
        if (inode_is_dir(dir_node))
	{
	  dir_close (directory);
	  directory = dir_open (dir_node);
	}
      }
      else
        return NULL;
    }

    token = next;
    next = strtok_r (NULL, "/", &save_ptr);
  }
  return directory;
}

struct dir *
dest_dir_from_path (char *path)
{
  struct dir *directory = dir_from_path (path);
  struct inode *dir_inode;
  char *file_name = retrieve_file_name(path);

  if (!dir_lookup (directory, file_name, &dir_inode))
    return NULL;

  return dir_open (dir_inode);
}

/* End of Project 4 */
