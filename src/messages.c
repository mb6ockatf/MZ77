#include<config.h>
#include<stdlib.h>
#include<stdnoreturn.h>
#include<stdio.h>
#include "dt.h"
#include "messages.h"

noreturn void
show_help (STR callable_name)
{
  printf ("Usage: %s [OPTION]...\n"
	  "Compress and decompress files with LZ77 in a graphical window.\n"
	  "\n"
	  "  -h, --help       display this help and exit\n"
	  "  -V, --version    output version information and exit\n"
	  "\n"
	  "Report bugs to: %s\n", callable_name, PACKAGE_BUGREPORT);
  exit (EXIT_SUCCESS);
}

noreturn void
show_version (void)
{
  printf ("mz77 (%s) %s\n"
	  "Copyright (C) 2026 mb6ockatf & m1st4r1a\n"
	  "License GPLv3+: GNU GPL version 3 or later "
	  "<https://gnu.org/licenses/gpl.html>\n"
	  "This is free software: you are free to change and redistribute it.\n"
	  "There is NO WARRANTY, to the extent permitted by law.\n",
	  PACKAGE_NAME, PACKAGE_VERSION);
  exit (EXIT_SUCCESS);
}
