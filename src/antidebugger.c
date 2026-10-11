#include <config.h>
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include <signal.h>
#include <sys/ptrace.h>
#include <sys/prctl.h>
volatile sig_atomic_t trap_detected = 0;

static void
sigtrap_handler (int signo)
{
  (void) signo;
  trap_detected = 1;
}

static bool
is_tracer_by_int3 ()
{
  struct sigaction sa, old;
  sa.sa_handler = sigtrap_handler;
  sigemptyset (&sa.sa_mask);
  sa.sa_flags = 0;
  sigaction (SIGTRAP, &sa, &old);
  trap_detected = 0;
  __asm__ volatile ("int3");
  sigaction (SIGTRAP, &old, NULL);
  return (trap_detected == 0);
}

static inline bool
is_tracer_present_procfs ()
{
  FILE *f = fopen ("/proc/self/status", "r");
  if (!f)
    return false;
  char line[256];
  int tracer = 0;
  while (fgets (line, sizeof (line), f))
    {
      if (strncmp (line, "TracerPid:", 10) == 0)
	{
	  tracer = atoi (line + 10);
	  break;
	}
    }
  fclose (f);
  return (tracer != 0);
}

static inline bool
is_ld_preload ()
{
  if (getenv ("LD_PRELOAD") != NULL)
    return true;
  return false;
}

static inline void
set_no_memdump ()
{
  prctl (PR_SET_DUMPABLE, 0);
}

void
check_debugger ()
{
  set_no_memdump ();
  if (is_tracer_present_procfs () || is_tracer_by_int3 ())
    exit (EXIT_SUCCESS);
}
