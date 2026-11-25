#ifndef __command_h__
#define __command_h__

/*
This file is part of fdbox
For license - read license.txt
*/

int command_command(int arc, char *argv[]);
const char *help_command();

/* EXIT_FAILURE means exit was triggered */
int command_execute_line(const char *line);

#if defined(__GONWARE__)
#include <stdbool.h>
#include <stddef.h>
/* the file a command name runs, found as the shell finds it (the current
 * directory, PATH, PATHEXT) */
bool command_which_file(const char *name, char *path, size_t size);
#endif

#endif //__command_h__
