/*
 *  run.c -- Run external programs
 *
 *  run.c is a part of binkd project
 *
 *  Copyright (C) 1996-1997  Dima Maloff, 5047/13
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version. See COPYING.
 */

#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#ifdef AMIGA
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#endif

#include "sys.h"
#include "run.h"
#include "tools.h"
#include "sem.h"

#ifdef UNIX
#define SHELL "/bin/sh"
#define SHELL_META "\"\'\\$`[]*?(){};&|<>~"
#define SHELLOPT "-c"
#elif defined(WIN32)
#define SHELL (getenv("COMSPEC") ? getenv("COMSPEC") : "cmd.exe")
#define SHELL_META "\"\'\\%<>|&^@"
#define SHELLOPT "/c"
#elif defined(OS2)
#define SHELL "cmd.exe"
#define SHELL_META "\"\'\\%<>|" /* not sure */
#define SHELLOPT "/c"
#elif defined(DOS)
#define SHELL (getenv("COMSPEC") ? getenv("COMSPEC") : "command.com")
#define SHELL_META "\"\'\\%<>|&^@"
#define SHELLOPT "/c"
#elif defined(AMIGA)
/* AmigaOS shell */
#define SHELL "c:execute"
#define SHELL_META "\"\'\\*?(){};&|<>"
#else
#error "Unknown platform"
#endif

#ifdef AMIGA
/* run(): execute an AmigaDOS command via SystemTagList() with NIL: I/O
 * Runs synchronously so binkd waits for completion (needed for srifreq
 * to create .rsp before parse_response). NIL: I/O prevents CLI freezing
 * Error output goes to a temporary file for logging on failure */
int run(char *cmd)
{
    /* All declarations at the top for C89/ADE GCC 2.95 compatibility */
    BPTR nil_in;
    char errfile[MAXPATHLEN];
    BPTR err_out = 0;
    int rc = 0;
    char cmd_copy[MAXPATHLEN];
    char *cmd_start;
    char *cmd_end;
    BPTR lock;
    struct TagItem exec_tags[5];
    BPTR errfile_ptr;
    char buf[512];
    int len;

    /* Open NIL: for input/output */
    nil_in = Open("NIL:", MODE_OLDFILE);

    /* Create temporary error file in current directory */
    snprintf(errfile, sizeof(errfile), "binkd_err_%ld.txt", (long)time(NULL));
    err_out = Open(errfile, MODE_NEWFILE);
    if (err_out == 0)
    {
        Log(2, "cannot create error file %s, using NIL: for error output", errfile);
    }

    /* Extract the command (first word) to check if it exists */
    strncpy(cmd_copy, cmd, sizeof(cmd_copy) - 1);
    cmd_copy[sizeof(cmd_copy) - 1] = '\0';
    cmd_start = cmd_copy;  /* Work on copy, never modify original cmd */

    /* Skip leading whitespace */
    while (*cmd_start && (*cmd_start == ' ' || *cmd_start == '\t'))
        cmd_start++;

    /* Find end of command (first space or end) */
    cmd_end = cmd_start;
    while (*cmd_end && *cmd_end != ' ' && *cmd_end != '\t')
        cmd_end++;
    *cmd_end = '\0';

    /* Check if command exists */
    lock = Lock((STRPTR)cmd_start, SHARED_LOCK);
    if (lock == 0)
    {
        Log(2, "command not found, skipping: '%s'", cmd_start);
        Close(nil_in);
        if (err_out)
            Close(err_out);
        DeleteFile((STRPTR)errfile);
        return 0;
    }
    UnLock(lock);

    Log(3, "executing '%s'", cmd);

    /* Set up tags with NIL: input and error file output.
     * Use NP_* (New Process) tags instead of SYS_* to avoid sharing
     * file handles with parent process - prevents stderr from being
     * closed when child exits. */
    exec_tags[0].ti_Tag = NP_Input;
    exec_tags[0].ti_Data = (ULONG)nil_in;
    exec_tags[1].ti_Tag = NP_Output;
    exec_tags[1].ti_Data = (ULONG)nil_in;
    exec_tags[2].ti_Tag = NP_Error;
    exec_tags[2].ti_Data = (ULONG)err_out;
    exec_tags[3].ti_Tag = NP_Synchronous;
    exec_tags[3].ti_Data = TRUE;
    exec_tags[4].ti_Tag = TAG_DONE;
    exec_tags[4].ti_Data = 0;

    rc = SystemTagList((STRPTR)cmd, exec_tags);

    /* Close handles */
    Close(nil_in);
    if (err_out)
        Close(err_out);

    /* Log error output if command failed */
    if (rc != 0)
    {
        errfile_ptr = Open(errfile, MODE_OLDFILE);
        if (errfile_ptr)
        {
            Log(2, "command failed with rc=%d, output:", rc);
            while ((len = Read(errfile_ptr, buf, sizeof(buf) - 1)) > 0)
            {
                buf[len] = '\0';
                Log(2, "%s", buf);
            }
            Close(errfile_ptr);
        }
    }

    DeleteFile((STRPTR)errfile);
    return rc;
}

/* run3(): pipe/tunnel not supported on AmigaOS without ixemul. */
int run3(const char *cmd, int *in, int *out, int *err)
{
    (void)cmd; (void)in; (void)out; (void)err;
    Log(1, "run3: pipe connections not supported on Amiga");
    return -1;
}
#endif /* AMIGA */

#ifndef AMIGA
int run (char *cmd)
{
  int rc=-1;
#if defined(EMX)
  sigset_t s;
    
  sigemptyset(&s);
  sigaddset(&s, SIGCHLD);
  sigprocmask(SIG_BLOCK, &s, NULL);
  Log (3, "executing `%s'", cmd);
  Log (3, "rc=%i", (rc=system (cmd)));
  sigprocmask(SIG_UNBLOCK, &s, NULL);
#elif defined(WIN32)
  STARTUPINFO si;
  PROCESS_INFORMATION pi;
  DWORD dw;
  char *cs, *sp=getenv("COMSPEC");

  Log (3, "executing `%s'", cmd);
  memset(&si, 0, sizeof(si));
  si.cb=sizeof(si);
  if (!sp)
  {
    if (Is9x())
      sp="command";
    else
      sp="cmd";
  }
  cs=(char*)malloc(strlen(sp)+strlen(cmd)+6);
  dw=CREATE_DEFAULT_ERROR_MODE;
  strcpy(cs, sp);
  strcat(cs, " /C ");
  sp=cmd;
  if (sp[0]=='@')
  {
    dw|=CREATE_NEW_CONSOLE|CREATE_NEW_PROCESS_GROUP;
    sp++;
    if (sp[0]=='@')
    {
      si.dwFlags=STARTF_USESHOWWINDOW;
      si.wShowWindow=SW_HIDE;
      sp++;
    }
    else 
      si.lpTitle=sp;
  }
  strcat(cs, sp);
  if (!CreateProcess(NULL, cs, NULL, NULL, 0, dw, NULL, NULL, &si, &pi))
    Log (1, "Error in CreateProcess()=%ld", (long)GetLastError());
  else if (sp==cmd)
  {
    if (WaitForSingleObject(pi.hProcess, INFINITE) != WAIT_OBJECT_0)
      Log (1, "Error in WaitForSingleObject()=%ld", (long)GetLastError());
    else if (!GetExitCodeProcess(pi.hProcess, &dw))
      Log (1, "Error in GetExitCodeProcess()=%ld", (long)GetLastError());
    else
      Log (3, "rc=%i", rc = (int)dw);
  }
  free(cs);
  CloseHandle(pi.hProcess);
  CloseHandle(pi.hThread);
#else
  Log (3, "executing `%s'", cmd);
  Log (3, "rc=%i", (rc=system (cmd)));
#endif
  return rc;
}
#endif /* !AMIGA */

#ifdef __MINGW32__
static int set_cloexec(int fd)
{
  HANDLE h, parent;
  int newfd;

  // return fd;
  parent = GetCurrentProcess();
  if (!DuplicateHandle(parent, (HANDLE)_get_osfhandle(fd), parent, &h, 0, FALSE, DUPLICATE_SAME_ACCESS)) {
    Log(1, "Error DuplicateHandle");
    return fd;
  }
  newfd = _open_osfhandle((int)h, O_NOINHERIT);
  if (newfd < 0) {
    Log(1, "Error open_odfhandle");
    CloseHandle(h);
    return fd;
  }
  close(fd);
  // Log(1, "NoInherit set for %i, new handle %i", fd, newfd);
  return newfd;
}
#endif

#ifndef AMIGA
int run3 (const char *cmd, int *in, int *out, int *err)
{
  int pid;
  int pin[2], pout[2], perr[2];
  const char *shell;

  if (in && pipe(pin) == -1)
  {
    Log (1, "Cannot create input pipe (stdin): %s", strerror(errno));
    return -1;
  }
  if (out && pipe(pout) == -1)
  {
    Log (1, "Cannot create output pipe (stdout): %s", strerror(errno));
    if (in)  close(pin[1]),  close(pin[0]);
    return -1;
  }
  if (err && pipe(perr) == -1)
  {
    Log (1, "Cannot create error pipe (stderr): %s", strerror(errno));
    if (in)  close(pin[1]),  close(pin[0]);
    if (out) close(pout[1]), close(pout[0]);
    return -1;
  }

#ifdef HAVE_FORK
#ifdef AMIGA
  /* Pipe tunneling not supported on AmigaOS without fork() */
  Log(1, "run3: pipe/tunnel not supported on Amiga: %s", cmd);
  if (in) close(pin[1]), close(pin[0]);
  if (out) close(pout[1]), close(pout[0]);
  if (err) close(perr[1]), close(perr[0]);
  return -1;
#else
  pid = fork();
  if (pid == -1)
  {
    Log (1, "Cannot fork: %s", strerror(errno));
    if (in)  close(pin[1]),  close(pin[0]);
    if (out) close(pout[1]), close(pout[0]);
    if (err) close(perr[1]), close(perr[0]);
    return -1;
  }
  if (pid == 0)
  { /* child */
    if (in)
    {
      dup2(pin[0], fileno(stdin));
      close(pin[0]);
      close(pin[1]);
    }
    if (out)
    {
      dup2(pout[1], fileno(stdout));
      close(pout[0]);
      close(pout[1]);
    }
    if (err)
    {
      dup2(perr[1], fileno(stderr));
      close(perr[0]);
      close(perr[1]);
    }
    if (strpbrk(cmd, SHELL_META))
    {
      shell = SHELL;
#ifdef AMIGA
	  execl(shell, shell, cmd, (char *)NULL);
#else
      execl(shell, shell, SHELLOPT, cmd, (char *)NULL);
#endif
    }
    else
    {
      /* execute command directly */
      /* in case of shell builtin like "read line" you should specify 
       * shell exclicitly, such as "/bin/sh -c read line" */
      char **args, *word;
      int i;

      args = xalloc(sizeof(args[0]));
      for (i=1; (word = getword(cmd, i)) != NULL; i++)
      {
        args = xrealloc(args, (i+1) * sizeof(*args));
        args[i-1] = word;
      }
      args[i-1] = NULL;
      execvp(args[0], args);
      xfree(args);
    }
    Log (1, "Execution '%s' failed: %s", cmd, strerror(errno));
    return -1;
  }
  if (in)
  {
    *in = pin[1];
    close(pin[0]);
  }
  if (out)
  {
    *out = pout[0];
    close(pout[1]);
  }
  if (err)
  {
    *err = perr[0];
    close(perr[1]);
  }
#endif /* !AMIGA */
#else

  /* redirect stdin/stdout/stderr takes effect for all threads */
  /* use lsem to avoid console output during this */
  {
    int save_errno = 0, savein = -1, saveout = -1, saveerr = -1;

    LockSem(&lsem);
    fflush(stdout);
    fflush(stderr);

    if (in)
    {
      savein = dup(fileno(stdin));
      dup2(pin[0], fileno(stdin));
      *in = pin[1];
      close(pin[0]);
#if defined(OS2)
      DosSetFHState(*in, OPEN_FLAGS_NOINHERIT);
#elif defined(EMX)
      fcntl(*in, F_SETFD, FD_CLOEXEC);
#elif defined __MINGW32__
      *in = set_cloexec(*in);
#endif
    }
    if (out)
    {
      saveout = dup(fileno(stdout));
      dup2(pout[1], fileno(stdout));
      *out = pout[0];
      close(pout[1]);
#if defined(OS2)
      DosSetFHState(*out, OPEN_FLAGS_NOINHERIT);
#elif defined(EMX)
      fcntl(*out, F_SETFD, FD_CLOEXEC);
#elif defined __MINGW32__
      *out = set_cloexec(*out);
#endif
    }
    if (err)
    {
      saveerr = dup(fileno(stderr));
      dup2(perr[1], fileno(stderr));
      *err = perr[0];
      close(perr[1]);
#if defined(OS2)
      DosSetFHState(*err, OPEN_FLAGS_NOINHERIT);
#elif defined(EMX)
      fcntl(*err, F_SETFD, FD_CLOEXEC);
#elif defined __MINGW32__
      *err = set_cloexec(*err);
#endif
    }
    if (strpbrk(cmd, SHELL_META) == NULL)
    {
      /* execute command directly */
      char **args, *word;
      int i;

      args = xalloc(sizeof(args[0]));
      for (i=1; (word = getword(cmd, i)) != NULL; i++)
      {
        args = xrealloc(args, (i+1) * sizeof(*args));
        args[i-1] = word;
      }
      args[i-1] = NULL;
      pid = spawnvp(P_NOWAIT, args[0], args);
      xfree(args);
    }
    else
    {
      shell = SHELL;
      pid = spawnl(P_NOWAIT, shell, shell, SHELLOPT, cmd, NULL);
    }

    if (pid == -1)
      save_errno = errno;
    if (savein != -1)
    {
      dup2(savein, fileno(stdin));
      close(savein);
    }
    if (saveout != -1)
    {
      dup2(saveout, fileno(stdout));
      close(saveout);
    }
    if (saveerr != -1)
    {
      dup2(saveerr, fileno(stderr));
      close(saveerr);
    }
    ReleaseSem(&lsem);
    if (pid == -1)
    {
      Log (1, "Cannot execute '%s': %s", cmd, strerror(save_errno));
      return -1;
    }
  }
#endif
  Log (2, "External command '%s' started, pid %i", cmd, pid);
  return pid;
}

#endif /* !AMIGA */
