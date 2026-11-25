#ifndef __simple_h__
#define __simple_h__

// This file is part of fdbox
// For license - read license.txt

int command_true(int argc, char *argv[]);
const char *help_true();
int command_false(int argc, char *argv[]);
const char *help_false();
int command_yes(int argc, char *argv[]);
const char *help_yes();
int command_sleep(int argc, char *argv[]);
const char *help_sleep();
int command_seq(int argc, char *argv[]);
const char *help_seq();
int command_rev(int argc, char *argv[]);
const char *help_rev();
int command_tee(int argc, char *argv[]);
const char *help_tee();

#endif //__simple_h__
