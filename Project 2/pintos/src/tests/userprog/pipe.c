/* -*- c -*- */

#include <syscall.h>
#include "tests/lib.h"

int
main (void)
{
  char * message = "Hello on the other side of the pipe!";
  char buf[1024];

  int ret, fd[2], pid;

  msg("begin");
  
  ret = pipe(fd);
  if (ret != 0) {
    fail("Error code returned by pipe");
  }
  
  pid = fork();
  if (pid > 0) { // the parent
    close(fd[1]);
    write(fd[0], message, strlen(msg) + 1);
  }
  else { // the child
    close(fd[0]);
    ret = read(fd[1], buf, sizeof(buf));
    if (ret != strlen(message) + 1) {
      fail("Too few bytes read from pipe");
    }
    msg("%s", buf);
  }
  return 0;
}

