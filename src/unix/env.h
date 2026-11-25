#ifndef __env_h__
#define __env_h__

// This file is part of fdbox
// For license - read license.txt

int command_env(int argc, char *argv[]);
const char *help_env();
int command_which(int argc, char *argv[]);
const char *help_which();
int command_uname(int argc, char *argv[]);
const char *help_uname();

#endif //__env_h__
