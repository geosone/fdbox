/* auto generated file */
/* not modify manually*/

/* general includes */
#include "fdbox.h"
#include "lib/applet.h"
#include <stddef.h>

/* applets includes */
#include "./help.h"
#include "dos/beep.h"
#include "dos/call.h"
#include "dos/cd.h"
#include "dos/chcp.h"
#include "dos/cls.h"
#include "dos/cmd_dir.h"
#include "dos/command.h"
#include "dos/copymove.h"
#include "dos/datetime.h"
#include "dos/del.h"
#include "dos/echo.h"
#include "dos/exit.h"
#include "dos/for.h"
#include "dos/goto.h"
#include "dos/history.h"
#include "dos/if.h"
#include "dos/keyb.h"
#include "dos/load.h"
#include "dos/md.h"
#include "dos/path.h"
#include "dos/prompt.h"
#include "dos/rd.h"
#include "dos/reboot.h"
#include "dos/rem.h"
#include "dos/set.h"
#include "dos/shift.h"
#include "dos/type.h"
#include "dos/ver.h"
#include "unix/basename.h"
#include "unix/cal.h"
#include "unix/cmp.h"
#include "unix/cut.h"
#include "unix/dirname.h"
#include "unix/env.h"
#include "unix/head.h"
#include "unix/hexdump.h"
#include "unix/ls.h"
#include "unix/pwd.h"
#include "unix/simple.h"
#include "unix/sort.h"
#include "unix/tail.h"
#include "unix/touch.h"
#include "unix/tr.h"
#include "unix/uniq.h"
#include "unix/wc.h"

/* clang-format off */
struct applet commands[] = {
        {help_beep, command_beep, "beep"},
        {help_call, command_call, "call"},
        {help_cd, command_cd, "cd"},
        {help_chcp, command_chcp, "chcp"},
        {help_cd, command_cd, "chdir"},
        {help_cls, command_cls, "cls"},
        {help_copy, command_copy, "copy"},
        {help_command, command_command, "command"},
        {help_date, command_date, "date"},
        {help_del, command_del, "del"},
        {help_dir, command_dir, "dir"},
        {help_echo, command_echo, "echo"},
        {help_del, command_del, "erase"},
        {help_exit, command_exit, "exit"},
        {help_for, command_for, "for"},
        {help_goto, command_goto, "goto"},
        {help_history, command_history, "history"},
        {help_if, command_if, "if"},
        {help_keyb, command_keyb, "keyb"},
        {help_load, command_load, "load"},
        {help_md, command_md, "md"},
        {help_md, command_md, "mkdir"},
        {help_move, command_move, "move"},
        {help_path, command_path, "path"},
        {help_poweroff, command_poweroff, "poweroff"},
        {help_prompt, command_prompt, "prompt"},
        {help_rd, command_rd, "rd"},
        {help_reboot, command_reboot, "reboot"},
        {help_rem, command_rem, "rem"},
        {help_del, command_del, "rm"},
        {help_rd, command_rd, "rmdir"},
        {help_set, command_set, "set"},
        {help_shift, command_shift, "shift"},
        {help_time, command_time, "time"},
        {help_type, command_type, "type"},
        {help_ver, command_ver, "ver"},
        {help_cal, command_cal, "cal", true},
        {help_head, command_head, "head", true},
        {help_hexdump, command_hexdump, "hexdump", true},
        {help_pwd, command_pwd, "pwd", true},
        {help_tail, command_tail, "tail", true},
        {help_wc, command_wc, "wc", true},
        {help_touch, command_touch, "touch", true},
        {help_basename, command_basename, "basename", true},
        {help_dirname, command_dirname, "dirname", true},
        {help_sort, command_sort, "sort", true},
        {help_uniq, command_uniq, "uniq", true},
        {help_type, command_type, "cat", true},
        {help_cls, command_cls, "clear", true},
        {help_copy, command_copy, "cp", true},
        {help_move, command_move, "mv", true},
        {help_ls, command_ls, "ls", true},
        {help_true, command_true, "true", true},
        {help_false, command_false, "false", true},
        {help_yes, command_yes, "yes", true},
        {help_sleep, command_sleep, "sleep", true},
        {help_seq, command_seq, "seq", true},
        {help_rev, command_rev, "rev", true},
        {help_tee, command_tee, "tee", true},
        {help_cmp, command_cmp, "cmp", true},
        {help_tr, command_tr, "tr", true},
        {help_cut, command_cut, "cut", true},
        {help_env, command_env, "env", true},
        {help_which, command_which, "which", true},
        {help_uname, command_uname, "uname", true},
        {help_help, command_help, "help"},
        {NULL, NULL, NULL}
};
