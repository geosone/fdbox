/*
This file is part of fdbox
For license - read license.txt
*/

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lib/applet.h"
#include "lib/args.h"
#include "lib/environ.h"
#include "lib/readline.h"

#include "dos/history.h"
#include "dos/prompt.h"
#include "lib/nls.h"

#ifdef __TURBOC__
#include "dos.h"
#include "lib/tc202/stdbool.h"
#include "lib/tc202/stdextra.h"
#endif

#if defined(__WATCOMC__)
#include <conio.h>
#include <i86.h>

// this is needed for clrscr() re-implementation
#include <graph.h>

void clrscr() { _clearscreen(_GCLEARSCREEN); }
#endif

#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
#include <dirent.h>
#include <stdbool.h>
#include <strings.h>
#include <termios.h>
#include <unistd.h>
#if defined(__GONWARE__)
#include <gonware/gdos.h>
#include <gonware/syscall.h>
#else
#include <poll.h>
#endif
#endif

#if defined(__WIN32__)
#include <conio.h>
#include <io.h>
#include <stdbool.h>
#include <windows.h>

int read_char() {
        DWORD cc;
        HANDLE h = GetStdHandle(STD_INPUT_HANDLE);

        // console not found
        if (h == NULL) {
                return -1;
        }

        for (;;) {
                INPUT_RECORD irec;
                ReadConsoleInput(h, &irec, 1, &cc);

                if (irec.EventType == KEY_EVENT && irec.Event.KeyEvent.bKeyDown) {
                        if (irec.Event.KeyEvent.uChar.AsciiChar != 0) {
                                return irec.Event.KeyEvent.uChar.AsciiChar;
                        } else {
                                switch (irec.Event.KeyEvent.wVirtualScanCode) {
                                case KEY_ARROW_LEFT & 0x00ff:
                                        return KEY_ARROW_LEFT;
                                case KEY_ARROW_RIGHT & 0x00ff:
                                        return KEY_ARROW_RIGHT;
                                case KEY_ARROW_UP & 0x00ff:
                                        return KEY_ARROW_UP;
                                case KEY_ARROW_DOWN & 0x00ff:
                                        return KEY_ARROW_DOWN;
                                case KEY_HOME & 0x00ff:
                                        return KEY_HOME;
                                case KEY_END & 0x00ff:
                                        return KEY_END;
                                case KEY_PGDOWN & 0x00ff:
                                        return KEY_PGDOWN;
                                case KEY_PGUP & 0x00ff:
                                        return KEY_PGUP;
                                case VK_INSERT:
                                case 82:
                                        return KEY_INS;
                                }
                        }
                        /*
                                        //&& ! ((KEY_EVENT_RECORD&)irec.Event).wRepeatCount )
                                                printf("*** Pressed %d,%c  unicode=%x, scancode=%x,
                           keycode=%x, flags=%x\n", irec.Event.KeyEvent.uChar.AsciiChar,
                                                       irec.Event.KeyEvent.uChar.AsciiChar,
                                                       irec.Event.KeyEvent.uChar.UnicodeChar,
                                                       irec.Event.KeyEvent.wVirtualScanCode,
                                                        irec.Event.KeyEvent.wVirtualKeyCode,
                                                        irec.Event.KeyEvent.dwControlKeyState
                                                );
                                                return irec.Event.KeyEvent.uChar.AsciiChar;
                                                */
                }
        }
        return -1;
}

#elif defined(_POSIX_C_SOURCE) || defined(__APPLE__)
/*
 * A key from a terminal. A sequence is read to its end, so one with
 * modifiers (Shift-Up ESC [ 1 ; 2 A, Ctrl-Del ESC [ 3 ; 5 ~), a mouse
 * report (ESC [ < 0 ; 1 ; 1 M) or a key with Alt (ESC x) leaves nothing
 * behind; those of no use here give 0.
 */
/* a byte of the terminal, read from fd 0 itself: nothing stays behind in
 * stdin's buffer, so key_waiting can tell a lone Esc from a sequence */
static int key_getc(void) {
        unsigned char c;
        ssize_t n;

        do {
                n = read(0, &c, 1);
        } while (n < 0 && errno == EINTR);
        return n == 1 ? c : -1;
}

/* another byte within ms milliseconds: the rest of a sequence */
static bool key_waiting(int ms) {
#if defined(__GONWARE__)
        return gdos_call2(GDOS_TTY_POLL, 0, (long)ms * 1000) > 0;
#else
        struct pollfd p;
        p.fd = 0;
        p.events = POLLIN;
        p.revents = 0;
        return poll(&p, 1, ms) > 0;
#endif
}

int read_char() {
        int i, num, mod, param, final;

        i = key_getc();
        switch (i) {
        case '\033':
                if (!key_waiting(50)) {
                        return KEY_ESC; /* Esc alone */
                }
                i = key_getc();
                if (i == 'O') { /* ESC O P, ESC O H */
                        i = key_getc();
                        if (i == 'H') {
                                return KEY_HOME;
                        }
                        if (i == 'F') {
                                return KEY_END;
                        }
                        return 0;
                }
                if (i != '[') {
                        return 0; /* Alt and a key */
                }
                /* the parameters, then the final byte; num is the first,
                 * mod the second (xterm's modifiers + 1: 4 Ctrl) */
                num = 0;
                mod = 0;
                param = 0;
                final = key_getc();
                while (final >= 0x30 && final <= 0x3f) {
                        if (final >= '0' && final <= '9') {
                                if (param == 0) {
                                        num = num * 10 + final - '0';
                                } else if (param == 1) {
                                        mod = mod * 10 + final - '0';
                                }
                        } else {
                                param++;
                        }
                        final = key_getc();
                }
                if (mod > 1 && ((mod - 1) & 4) && (final == 'C' || final == 'D')) {
                        return final == 'C' ? KEY_WORD_RIGHT : KEY_WORD_LEFT;
                }
                switch (final) {
                case 'Z':
                        return KEY_BACKTAB;
                case 'A':
                        return KEY_ARROW_UP;
                case 'B':
                        return KEY_ARROW_DOWN;
                case 'C':
                        return KEY_ARROW_RIGHT;
                case 'D':
                        return KEY_ARROW_LEFT;
                case 'H':
                        return KEY_HOME;
                case 'F':
                        return KEY_END;
                case '~':
                        switch (num) {
                        case 1:
                        case 7:
                                return KEY_HOME;
                        case 4:
                        case 8:
                                return KEY_END;
                        case 2:
                                return KEY_INS;
                        case 3:
                                return KEY_DEL;
                        case 5:
                                return KEY_PGUP;
                        case 6:
                                return KEY_PGDOWN;
                        }
                }
                return 0;
        case '\177':
                return '\b';
        default:
                return i;
        }
}
#define get_char_impl get_char_posix

#elif defined(__TURBOC__) || (defined(__WATCOMC__))
int read_char() {
        int c = getch();

        /* extended ASCII FTW */
        if (c == 0) {
                c = getch();
                switch (c) {
                case KEY_ARROW_LEFT & 0x00ff:
                        return KEY_ARROW_LEFT;
                case KEY_ARROW_RIGHT & 0x00ff:
                        return KEY_ARROW_RIGHT;
                case KEY_ARROW_UP & 0x00ff:
                        return KEY_ARROW_UP;
                case KEY_ARROW_DOWN & 0x00ff:
                        return KEY_ARROW_DOWN;
                case KEY_HOME & 0x00ff:
                        return KEY_HOME;
                case KEY_END & 0x00ff:
                        return KEY_END;
                case KEY_PGDOWN & 0x00ff:
                        return KEY_PGDOWN;
                case KEY_PGUP & 0x00ff:
                        return KEY_PGUP;
                case 82:
                        return KEY_INS;
                case 83:
                        return KEY_DEL;
                case KEY_WORD_LEFT & 0x00ff:
                        return KEY_WORD_LEFT;
                case KEY_WORD_RIGHT & 0x00ff:
                        return KEY_WORD_RIGHT;
                case KEY_BACKTAB & 0x00ff:
                        return KEY_BACKTAB;
                default:
                        return 0;
                }
        }
        return c;
}
#else
#error Undefined platform - we cannot read a line on a terminal
TODO - it seems that this platform is not supported yet -
    you need to define a function "read_char()" that reads a char without enter beeing pressed.
#endif

struct str_list history;

/* once: readline_init runs again after each program the shell starts */
static void history_init(void) {
        if (history.items == NULL) {
                str_list_init(&history, 50);
        }
}

int read_string(char *line, size_t max_size) {
        int l;
        struct readline_session session;
        readline_session_init(&session);
        set_cursor_insert();
        session.line = line;
        session.max_size = max_size;
        l = readline(&session);
        readline_session_deinit(&session);
        return l;
}

#if defined(WIN32)
// https://docs.microsoft.com/en-us/windows/console/clearing-the-screen
void cls_win32(HANDLE hConsole) {
        CONSOLE_SCREEN_BUFFER_INFO csbi;
        SMALL_RECT scrollRect;
        COORD scrollTarget;
        CHAR_INFO fill;

        // Get the number of character cells in the current buffer.
        if (!GetConsoleScreenBufferInfo(hConsole, &csbi)) {
                return;
        }

        // Scroll the rectangle of the entire buffer.
        scrollRect.Left = 0;
        scrollRect.Top = 0;
        scrollRect.Right = csbi.dwSize.X;
        scrollRect.Bottom = csbi.dwSize.Y;

        // Scroll it upwards off the top of the buffer with a magnitude of the entire height.
        scrollTarget.X = 0;
        scrollTarget.Y = (SHORT)(0 - csbi.dwSize.Y);

        // Fill with empty spaces with the buffer's default text attribute.
        fill.Char.UnicodeChar = TEXT(' ');
        fill.Attributes = csbi.wAttributes;

        // Do the scroll
        ScrollConsoleScreenBuffer(hConsole, &scrollRect, NULL, scrollTarget, &fill);

        // Move the cursor to the top left corner too.
        csbi.dwCursorPosition.X = 0;
        csbi.dwCursorPosition.Y = 0;

        SetConsoleCursorPosition(hConsole, csbi.dwCursorPosition);
}
#endif

void clear_screen() {
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
        printf("\e[1;1H\e[2J");
#elif defined(WIN32)
        HANDLE hStdout;
        hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
        cls_win32(hStdout);
#elif defined(__MSDOS__) || defined(__WATCOMC__)
        clrscr();
#else
        please implement clean screen
#endif
}

void set_cursor_insert() {
#if defined(_POSIX_C_SOURCE)
        printf("\e[1 q");
#elif defined(__APPLE__)
#elif defined(WIN32)
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_CURSOR_INFO cursorInfo;

        GetConsoleCursorInfo(out, &cursorInfo);
        cursorInfo.bVisible = true;
        cursorInfo.dwSize = 100;
        SetConsoleCursorInfo(out, &cursorInfo);
#elif defined(__MSDOS__) || defined(__WATCOMC__)
        union REGS xr;
        xr.h.ah = 1;
        xr.h.ch = 6;
        xr.h.cl = 7;
        int86(0x10, &xr, &xr);
#else
        please implement set cursor_block
#endif
}

void set_cursor_override() {
#if defined(_POSIX_C_SOURCE)
        printf("\e[3 q");
#elif defined(__APPLE__)
        // unsupported on apple, ok by me
#elif defined(WIN32)
        // https://stackoverflow.com/a/30126700
        HANDLE out = GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_CURSOR_INFO cursorInfo;

        GetConsoleCursorInfo(out, &cursorInfo);
        cursorInfo.bVisible = true;
        cursorInfo.dwSize = 10;
        SetConsoleCursorInfo(out, &cursorInfo);
#elif defined(__MSDOS__) || defined(__WATCOMC__)
        /*
        https://jbwyatt.com/253/emu/8086_bios_and_dos_interrupts.html
        http://computer-programming-forum.com/29-pascal/7b38c5f9b15dfae6.htm
        http://computer-programming-forum.com/47-c-language/43c194b3ed78f2e2.htm
        */
        union REGS xr;
        xr.h.ah = 1;
        xr.h.ch = 0;
        xr.h.cl = 7;
        int86(0x10, &xr, &xr);
#else
        please implement set cursor_block
#endif
}

/* https://stackoverflow.com/a/1798833 */
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
static struct termios oldt;
#endif

void readline_init() {
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
        static struct termios newt;
        tcgetattr(STDIN_FILENO, &oldt);
        newt = oldt;
        newt.c_lflag &= ~(ICANON | ECHO);
        tcsetattr(STDIN_FILENO, TCSANOW, &newt);
#endif
        history_init();
}

void readline_deinit() {
#ifdef _POSIX_C_SOURCE
        tcsetattr(STDIN_FILENO, TCSANOW, &oldt);
#endif
}

void move_cursor_back(size_t n) {
        size_t i;
        for (i = 0; i < n; i++) {
                putchar('\b');
        }
}

/*
The line holds UTF-8 on POSIX and GonWare (a character is several bytes,
a wide one - CJK, Hangul ... - takes two cells of the screen), single
bytes of the code page elsewhere. index and current_size count bytes;
the cursor moves by cells.
*/
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
#define READLINE_UTF8 1
#else
#define READLINE_UTF8 0
#endif

/* the bytes of the character that starts with lead */
static size_t char_len(unsigned char lead) {
        if (!READLINE_UTF8 || lead < 0xc0) {
                return 1;
        }
        return lead >= 0xf0 ? 4 : lead >= 0xe0 ? 3 : 2;
}

/* the start of the character before byte i */
static size_t char_prev(const char *line, size_t i) {
        if (i == 0) {
                return 0;
        }
        i--;
        while (READLINE_UTF8 && i > 0 && ((unsigned char)line[i] & 0xc0) == 0x80) {
                i--;
        }
        return i;
}

/* the cells of a character: 2 for the East Asian wide and full-width ones
   (as wcwidth, Markus Kuhn's ranges; GDOS's console uses the same) */
static size_t char_cells(const char *p) {
        unsigned long u;
        unsigned char c = (unsigned char)p[0];
        if (!READLINE_UTF8 || c < 0xe0) {
                return 1;
        }
        if (c >= 0xf0) {
                u = ((unsigned long)(c & 7) << 18) | ((unsigned long)(p[1] & 0x3f) << 12) |
                    ((unsigned long)(p[2] & 0x3f) << 6) | (unsigned long)(p[3] & 0x3f);
                return (u >= 0x20000 && u <= 0x3fffd) ? 2 : 1;
        }
        u = ((unsigned long)(c & 0x0f) << 12) | ((unsigned long)(p[1] & 0x3f) << 6) |
            (unsigned long)(p[2] & 0x3f);
        return (u >= 0x1100 && u <= 0x115f) || u == 0x2329 || u == 0x232a ||
                       (u >= 0x2e80 && u <= 0xa4cf && u != 0x303f) ||
                       (u >= 0xac00 && u <= 0xd7a3) || (u >= 0xf900 && u <= 0xfaff) ||
                       (u >= 0xfe10 && u <= 0xfe19) || (u >= 0xfe30 && u <= 0xfe6f) ||
                       (u >= 0xff00 && u <= 0xff60) || (u >= 0xffe0 && u <= 0xffe6)
                   ? 2
                   : 1;
}

/* the cells of bytes from..to-1 of the line */
static size_t cells(const char *line, size_t from, size_t to) {
        size_t n = 0;
        while (from < to && line[from]) {
                n += char_cells(line + from);
                from += char_len((unsigned char)line[from]);
        }
        return n;
}

static void put_spaces(size_t n) {
        while (n--) {
                putchar(' ');
        }
}

/* the line from byte i is shown again, followed by blank cells over what
   was there before; the cursor goes back to i */
static void redraw_from(struct readline_session *session, size_t i, size_t blank) {
        size_t n = cells(session->line, i, session->current_size);
        printf("%s", session->line + i);
        put_spaces(blank);
        move_cursor_back(n + blank);
        fflush(stdout);
}

void readline_session_init(struct readline_session *session) {
        session->line = NULL;
        session->max_size = 0;
        session->current_size = 0;
        session->current_history = 0;
        session->index = 0;
        session->override = false;
        session->free_memory = false;
}

void readline_session_allocate(struct readline_session *session, size_t max_size) {
        readline_session_init(session);
        session->line = malloc(max_size);
        session->max_size = max_size;
        session->free_memory = true;
}

void readline_session_deinit(struct readline_session *session) {
        if (session->free_memory) {
                free(session->line);
        }
        readline_session_init(session);
}

/* a line typed is kept unless it is empty, starts with a space or is the
 * one before */
static void save_history(struct readline_session *session) {
        const char *last;
        bool should_save;
        history_init();
        last = str_list_get(&history, 0);
        should_save = session->current_size != 0 && session->line[0] != ' ';
        if (last != NULL) {
                should_save &= strcmp(last, session->line) != 0;
        }
        if (should_save) {
                str_list_push(&history, session->line);
        }
}

/* text put in at the cursor, character by character */
static void readline_insert_text(struct readline_session *session, const char *text) {
        while (*text) {
                size_t n = char_len((unsigned char)*text);
                if (session->current_size + n >= session->max_size) {
                        break;
                }
                session->current_size = readline_insert_seq(session, text, n);
                text += n;
        }
}

static bool is_space(char c) { return c == ' ' || c == '\t'; }

/* to the start of this word or the one before */
static void readline_word_left(struct readline_session *session) {
        while (session->index > 0 && is_space(session->line[session->index - 1])) {
                readline_move_left(session);
        }
        while (session->index > 0 && !is_space(session->line[session->index - 1])) {
                readline_move_left(session);
        }
}

/* to the start of the next word */
static void readline_word_right(struct readline_session *session) {
        while (session->index < session->current_size && !is_space(session->line[session->index])) {
                readline_move_right(session);
        }
        while (session->index < session->current_size && is_space(session->line[session->index])) {
                readline_move_right(session);
        }
}

/*
TAB completes the file or directory name the cursor is in, as cmd.exe
does: each TAB puts the next name that starts with what was typed there,
Shift-TAB the one before; names with a space are quoted. Any other key
keeps the name.
*/
struct completion {
        char **names;
        size_t count, current;
        size_t start;  /* the word's first byte in the line */
        size_t length; /* the bytes of the word there now */
        char dir[260]; /* the directory part as typed */
};

static void completion_free(struct completion *c) {
        size_t i;
        for (i = 0; i < c->count; i++) {
                free(c->names[i]);
        }
        free(c->names);
        c->names = NULL;
        c->count = 0;
}

#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
#if defined(__GONWARE__)
#define name_ncmp strncasecmp
#define name_cmp strcasecmp
#else
#define name_ncmp strncmp
#define name_cmp strcmp
#endif

static int completion_order(const void *a, const void *b) {
        return name_cmp(*(char *const *)a, *(char *const *)b);
}

/* the names in the word's directory that start as the word does */
static bool completion_find(struct completion *c, const struct readline_session *session) {
        char word[260], opendir_name[260], *prefix;
        size_t i, n = 0, dirlen = 0, max = 0;
        bool quote = false;
        DIR *d;
        struct dirent *e;

        c->start = 0;
        for (i = 0; i < session->index; i++) {
                char ch = session->line[i];
                if (ch == '"') {
                        quote = !quote;
                } else if (!quote && (is_space(ch) || ch == '|' || ch == '<' || ch == '>')) {
                        c->start = i + 1;
                }
        }
        c->length = session->index - c->start;
        for (i = c->start; i < session->index && n + 1 < sizeof(word); i++) {
                if (session->line[i] != '"') {
                        word[n++] = session->line[i];
                }
        }
        word[n] = 0;
        for (i = 0; i < n; i++) {
                if (word[i] == '/' || word[i] == '\\' || word[i] == ':') {
                        dirlen = i + 1;
                }
        }
        memcpy(c->dir, word, dirlen);
        c->dir[dirlen] = 0;
        prefix = word + dirlen;
        if (dirlen == 0) {
                strcpy(opendir_name, ".");
        } else {
                strcpy(opendir_name, c->dir);
                /* "A\B\" as "A\B", a root ("\", "C:\", "C:") as it is */
                if (dirlen > 1 && (c->dir[dirlen - 1] == '/' || c->dir[dirlen - 1] == '\\') &&
                    c->dir[dirlen - 2] != ':') {
                        opendir_name[dirlen - 1] = 0;
                }
        }
        d = opendir(opendir_name);
        if (d == NULL) {
                return false;
        }
        while ((e = readdir(d)) != NULL) {
                char **more;
                if (e->d_name[0] == '.' && prefix[0] != '.') {
                        continue;
                }
                if (strcmp(e->d_name, ".") == 0 || strcmp(e->d_name, "..") == 0 ||
                    name_ncmp(e->d_name, prefix, strlen(prefix)) != 0) {
                        continue;
                }
                if (c->count == max) {
                        max = max ? max * 2 : 16;
                        more = realloc(c->names, max * sizeof(char *));
                        if (more == NULL) {
                                break;
                        }
                        c->names = more;
                }
                c->names[c->count] = strdup(e->d_name);
                if (c->names[c->count] != NULL) {
                        c->count++;
                }
        }
        closedir(d);
        if (c->count == 0) {
                return false;
        }
        qsort(c->names, c->count, sizeof(char *), completion_order);
        return true;
}
#else
static bool completion_find(struct completion *c, const struct readline_session *session) {
        (void)c;
        (void)session;
        return false;
}
#endif

/* the word replaced by the current name */
static void completion_put(struct completion *c, struct readline_session *session) {
        char text[600];
        const char *name = c->names[c->current];
        bool quote = strchr(c->dir, ' ') != NULL || strchr(name, ' ') != NULL;
        size_t removed = 0;

        snprintf(text, sizeof(text), "%s%s%s%s", quote ? "\"" : "", c->dir, name,
                 quote ? "\"" : "");
        while (session->index > c->start) {
                readline_move_left(session);
        }
        while (removed < c->length && session->index < session->current_size) {
                size_t before = session->current_size;
                readline_delete_right(session);
                removed += before - session->current_size;
        }
        readline_insert_text(session, text);
        c->length = session->index - c->start;
}

static int readline_keys(struct readline_session *session) {
        char seq[4];
        size_t i, n;
        char *draft = NULL; /* the line typed before going back in the history */
        struct completion comp;
        bool completing = false;
        int result = -2;

        memset(&comp, 0, sizeof(comp));
        history_init();
        session->line[0] = 0;
        session->current_history = 0;
        while (result == -2 && session->index < session->max_size) {
                int c = read_char();
                if (completing && c != '\t' && c != KEY_BACKTAB) {
                        completion_free(&comp);
                        completing = false;
                }
                switch (c) {
                case '\r':
                case '\n':
                        save_history(session);
                        putchar('\n');
                        result = (int)session->current_size;
                        break;
                case 1: /* control + a */
                case KEY_HOME:
                        readline_move_home(session);
                        break;
                case 5: /* control + e */
                case KEY_END:
                        readline_move_end(session);
                        break;
                case KEY_ARROW_LEFT:
                        readline_move_left(session);
                        break;
                case KEY_ARROW_RIGHT:
                        readline_move_right(session);
                        break;
                case 2: /* control + b */
                case KEY_WORD_LEFT:
                        readline_word_left(session);
                        break;
                case 6: /* control + f */
                case KEY_WORD_RIGHT:
                        readline_word_right(session);
                        break;
                case KEY_ARROW_UP: {
                        /* one line further back; the one typed is kept */
                        const char *older = str_list_get(&history, session->current_history);
                        if (older == NULL) {
                                break;
                        }
                        if (session->current_history == 0) {
                                free(draft);
                                draft = strdup(session->line);
                        }
                        session->current_history++;
                        readline_set(session, older);
                        break;
                }
                case KEY_ARROW_DOWN:
                        if (session->current_history == 0) {
                                break;
                        }
                        session->current_history--;
                        if (session->current_history == 0) {
                                readline_set(session, draft ? draft : "");
                        } else {
                                const char *newer =
                                    str_list_get(&history, session->current_history - 1);
                                readline_set(session, newer ? newer : "");
                        }
                        break;
                case KEY_BACKSPACE:
                        session->index = readline_delete_left(session);
                        break;
                case KEY_DEL:
                        session->index = readline_delete_right(session);
                        break;
                case 11: /* control + k: to the end */
                        while (session->index < session->current_size) {
                                readline_delete_right(session);
                        }
                        break;
                case 21: /* control + u: to the start */
                        while (session->index > 0) {
                                readline_delete_left(session);
                        }
                        break;
                case KEY_ESC: /* the line cleared, as in DOS */
                        readline_set(session, "");
                        session->current_history = 0;
                        break;
                case KEY_INS:
                        session->override = !session->override;
                        if (session->override) {
                                set_cursor_override();
                        } else {
                                set_cursor_insert();
                        }
                        break;
                case '\t':
                case KEY_BACKTAB:
                        if (!completing) {
                                completing = completion_find(&comp, session);
                                if (!completing) {
                                        completion_free(&comp);
                                        putchar('\a');
                                        fflush(stdout);
                                        break;
                                }
                                comp.current = c == '\t' ? 0 : comp.count - 1;
                        } else if (c == '\t') {
                                comp.current = (comp.current + 1) % comp.count;
                        } else {
                                comp.current = (comp.current + comp.count - 1) % comp.count;
                        }
                        completion_put(&comp, session);
                        break;
                case 3: /* control + c: the line given up */
                        readline_move_end(session);
                        fputs("^C\n", stdout);
                        session->current_size = 0;
                        session->line[0] = 0;
                        result = 0;
                        break;
                case -1: /* the end of the input */
                case 4:  /* control + d */
                        session->current_size = 0;
                        session->line[0] = 0;
                        result = -1;
                        break;
                case 12:
                        session->current_size = 0;
                        session->line[0] = 0;
                        clear_screen();
                        result = 0;
                        break;
                default:
                        if (c <= 0 || c > 0xff || (READLINE_UTF8 && (c & 0xc0) == 0x80)) {
                                break; /* a key without a character (0: one of no use), a stray
                                          UTF-8 byte */
                        }
                        /* a whole character: its UTF-8 bytes */
                        seq[0] = (char)c;
                        n = char_len((unsigned char)c);
                        for (i = 1; i < n; i++) {
                                seq[i] = (char)read_char();
                        }
                        if (session->current_size + n >= session->max_size) {
                                break;
                        }
                        if (session->override && session->index < session->current_size) {
                                readline_delete_right(session);
                        }
                        session->current_size = readline_insert_seq(session, seq, n);
                }
        }
        completion_free(&comp);
        free(draft);
        return result == -2 ? (int)session->current_size : result;
}

/* the terminal one key at a time, not echoed, Ctrl-C a key; *saved gets
 * its mode before (false: no terminal) */
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
static bool keys_mode(struct termios *saved) {
        struct termios keys;
        if (tcgetattr(STDIN_FILENO, saved) != 0) {
                return false;
        }
        keys = *saved;
        keys.c_lflag &= ~(ICANON | ECHO | ISIG);
        keys.c_cc[VMIN] = 1;
        keys.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &keys);
        return true;
}
#endif

int readline_key(void) {
        int c;
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
        struct termios t;
        bool have = keys_mode(&t);
#endif
        fflush(stdout);
        c = read_char();
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
        if (have) {
                tcsetattr(STDIN_FILENO, TCSANOW, &t);
        }
#endif
        return c;
}

int readline(struct readline_session *session) {
        int r;
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
        /* the keys one by one, not echoed; Ctrl-C a key while a line is
         * typed, not a signal that ends the shell (while a command runs it
         * stays the signal) */
        struct termios t;
        bool have = keys_mode(&t);
#endif
        r = readline_keys(session);
#if defined(_POSIX_C_SOURCE) || defined(__APPLE__)
        if (have) {
                tcsetattr(STDIN_FILENO, TCSANOW, &t);
        }
#endif
        return r;
}

size_t readline_insert_seq(struct readline_session *session, const char *seq, size_t n) {
        char *at = session->line + session->index;
        memmove(at + n, at, session->current_size - session->index + 1);
        memcpy(at, seq, n);
        session->current_size += n;
        printf("%.*s", (int)n, at);
        session->index += n;
        redraw_from(session, session->index, 0);
        return session->current_size;
}

size_t readline_delete_left(struct readline_session *session) {
        size_t p, w;
        if (session->index == 0) {
                return 0;
        }
        p = char_prev(session->line, session->index);
        w = cells(session->line, p, session->index);
        memmove(session->line + p, session->line + session->index,
                session->current_size - session->index + 1);
        session->current_size -= session->index - p;
        session->index = p;
        move_cursor_back(w);
        redraw_from(session, p, w);
        return session->index;
}

size_t readline_delete_right(struct readline_session *session) {
        size_t n, w;
        if (session->index >= session->current_size) {
                return session->index;
        }
        n = char_len((unsigned char)session->line[session->index]);
        w = char_cells(session->line + session->index);
        memmove(session->line + session->index, session->line + session->index + n,
                session->current_size - session->index - n + 1);
        session->current_size -= n;
        redraw_from(session, session->index, w);
        return session->index;
}

size_t readline_replace(struct readline_session *session, size_t index, char c) {
        session->index = index;
        if (session->index < session->current_size) {
                readline_delete_right(session);
        }
        return readline_insert(session, session->index, c);
}

size_t readline_insert(struct readline_session *session, size_t index, char c) {
        session->index = index;
        return readline_insert_seq(session, &c, 1);
}

size_t readline_set(struct readline_session *session, const char *new_text) {
        size_t l, old = cells(session->line, 0, session->current_size);
        move_cursor_back(cells(session->line, 0, session->index));
        put_spaces(old);
        move_cursor_back(old);
        l = strlen(new_text);
        if (l >= session->max_size) {
                l = session->max_size - 1;
        }
        memcpy(session->line, new_text, l);
        session->line[l] = 0;
        session->current_size = l;
        session->index = l;
        printf("%s", session->line);
        fflush(stdout);
        return l;
}

void readline_move_home(struct readline_session *session) {
        move_cursor_back(cells(session->line, 0, session->index));
        session->index = 0;
        fflush(stdout);
}

void readline_move_end(struct readline_session *session) {
        printf("%s", session->line + session->index);
        session->index = session->current_size;
        fflush(stdout);
}

void readline_move_left(struct readline_session *session) {
        size_t p;
        if (session->index > 0) {
                p = char_prev(session->line, session->index);
                move_cursor_back(cells(session->line, p, session->index));
                session->index = p;
                fflush(stdout);
        }
}

void readline_move_right(struct readline_session *session) {
        size_t n;
        if (session->index < session->current_size) {
                n = char_len((unsigned char)session->line[session->index]);
                printf("%.*s", (int)n, session->line + session->index);
                session->index += n;
                fflush(stdout);
        }
}

const char *readline_get_history(size_t i) { return str_list_get(&history, i); }
