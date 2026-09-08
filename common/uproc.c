/*
 * uproc.c
 *
 * Code grabbed from chimera 1.65.
 */

#include "port_before.h"

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <errno.h>
#include <sys/time.h>
#include <sys/types.h>
#ifndef __QNX__
#include <sys/signal.h>
#else
#include <signal.h>
#endif
#include <sys/wait.h>
#include <sys/stat.h>

#include "port_after.h"

#include "common.h"

/*
 * ReapChild
 *
 * This code grabs the status from an exiting child process.  We really
 * don't care about the status, we just want to keep zombie's from
 * cropping up.
 */
static void
ReapChild()
{
#if defined(WNOHANG) && !defined(SYSV) && !defined(SVR4)
  int pid;
#endif
  extern int errno;
  int old_errno = errno;

 /*
  * It would probably be better to use the POSIX mechanism here,but I have not
  * checked into it.  This gets us off the ground with SYSV.  RSE@GMI
  */
#if defined(WNOHANG) && !defined(SYSV) && !defined(SVR4) && !defined(__QNX__) && !defined(__EMX__)
  union wait st;

  do 
  {
    errno = 0;
    pid = wait3(&st, WNOHANG, 0);
  }
  while (pid <= 0 && errno == EINTR);
#else
  int st;

  wait(&st);
#endif
  StartReaper();
  errno = old_errno;

  return;
}

/*
 * StartReaper
 *
 * This code inits the code which reaps child processes that where
 * fork'd off for external viewers.
 */
void
StartReaper()
{
#ifdef SIGCHLD
  signal(SIGCHLD, ReapChild);
#else
  signal(SIGCLD, ReapChild);
#endif

  return;
}

/*
 * PipeCommand
 *
 * fork and exec to get a program running and supply it with
 * a stdin, stdout, stderr that so we can talk to it.
 */
int
PipeCommand(command, fd)
char *command;
int *fd;
{
  int pout[2];
  int pin[2];
  int pid;

/*
  if (pipe(pout) == -1) return(-1);
*/
  if (pipe(pin) == -1)
  {
    close(pout[0]);
    close(pout[1]);
    return(-1);
  }

  pid = fork();
  if (pid == -1)
  {
    return(-1);
  }
  else if (pid == 0)
  {
/*
    if (pout[1] != 1)
    {
      dup2(pout[1], 1);
      close(pout[1]);
    }
*/
    if (pin[0] != 0)
    {
      dup2(pin[0], 0);
      close(pin[0]);
    }

    signal(SIGPIPE, SIG_DFL);

    execl(command, command, (char *)0);
  }
  else
  {
    close(pout[1]);
    close(pin[0]);
  }

  fd[0] = pin[1];
/*
  fd[1] = pout[0];
*/

  return(0);
}
