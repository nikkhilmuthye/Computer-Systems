#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

#define ELFMAG0 127

#define ELFMAG1 69 
#define ELFMAG2 76
#define ELFMAG3 70
#define ET_EXEC 2 
#define ET_DYN 3
#define EM_386 3
#define EV_CURRENT 1

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

/* Start of Project 2 */
#define EXEC true

bool file_load (void *);
bool is_file_exec (char *, void (**) (void), void **);
void user_exit (int);
/* End of Project 2 */

/* Start of Project 3 */
bool install_page (void *, void *, bool);
bool load_segment (struct file *, off_t, uint8_t *,
                   uint32_t, uint32_t, bool);
/* End of Project 3 */

#endif /* userprog/process.h */
