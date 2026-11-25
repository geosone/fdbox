#ifndef FDBOX_APPLETS
#define FDBOX_APPLETS

typedef int (*function_handler)(int, char *[]);
typedef const char *(*function_help)();

/* The only problematic platform is turbo C, all others are modern */
#if defined(__TURBOC__)
#include "lib/tc202/stdbool.h"
#else
#include <stdbool.h>
#endif

#define CASE_INSENSITVE false
#define CASE_SENSITVE true

struct applet {
        function_help help_text;
        function_handler handler;
        const char *name;
        /* a unix command: its arguments split as a program gets them (at
         * white space only, "NAME=VALUE" stays one word), not as DOS splits
         * a command's at "=" too */
        bool words;
};

struct applet *find_applet(bool sensitive, const char *applet_name, struct applet commands[]);

#endif // FDBOX_APPLETS
