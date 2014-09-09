#ifndef USERPROG_EXCEPTION_H
#define USERPROG_EXCEPTION_H

#include <stdbool.h>
#include <stdint.h>
#include "vm/page.h"

/* Page fault error code bits that describe the cause of the exception.  */
#define PF_P 0x1    /* 0: not-present page. 1: access rights violation. */
#define PF_W 0x2    /* 0: read, 1: write. */
#define PF_U 0x4    /* 0: kernel, 1: user process. */

/* Start of Project 3 */
#define STACK_LIMIT (8 * 1024 * 1024)  /* Limit for the Stack Growth*/
/* End of Project 3 */

void exception_init (void);
void exception_print_stats (void);

/* Start of Project 3 */
bool grow_stack_by_page (uint32_t *);
bool load_from_file (struct sup_page *);
bool swap_in_from_disk (struct sup_page *);
/* End of Project 3 */
#endif /* userprog/exception.h */
