#!/usr/bin/env python3
# This file is part of fdbox
# For license - read license.txt
#
# readline.py <fdbox> - the line editor of the interactive shell, in a
# pseudo terminal: keys are typed into "fdbox command" and what the commands
# print shows what the line was when Enter came (history, Esc, Ctrl-C,
# word moves, Ctrl-K/U, Del, TAB completion). Exit code 0: all passed.

import fcntl
import os
import pty
import struct
import termios
import select
import sys
import tempfile
import time

UP, DOWN, LEFT, RIGHT = "\033[A", "\033[B", "\033[D", "\033[C"
HOME, DEL = "\033[H", "\033[3~"
CTRL_LEFT, CTRL_RIGHT, BACKTAB = "\033[1;5D", "\033[1;5C", "\033[Z"
ESC = "\033"

# each case: the keys (a list: Esc alone needs a pause after it) and the
# line the command prints
CASES = [
    ("echo", ["echo eins\r"], "eins"),
    ("history up", ["echo zwei\r", UP + UP + "\r"], "zwei|eins"),
    ("history keeps the typed line", ["echo drei", UP, DOWN, "\r"], "drei"),
    ("history down to newer", ["echo vier\r", UP + UP + UP + DOWN + "\r"], "vier|drei"),
    ("esc clears the line", ["echo falsch", ESC, "echo richtig\r"], "richtig"),
    ("ctrl-c gives the line up", ["echo abbruch\x03", "echo weiter\r"], "weiter"),
    ("ctrl-left", ["echo b" + CTRL_LEFT + "a \r"], "a b"),
    ("ctrl-b", ["echo d\x02c \r"], "c d"),
    ("ctrl-right", ["echo x" + HOME + CTRL_RIGHT + "w\r"], "wx"),
    ("ctrl-f", ["echo y" + HOME + "\x06v\r"], "vy"),
    ("ctrl-k", ["echo abcdef" + HOME + CTRL_RIGHT + RIGHT * 3 + "\x0b\r"], "abc"),
    ("ctrl-u", ["echo xyz\x15echo neu\r"], "neu"),
    ("del", ["echo abX" + LEFT + DEL + "\r"], "ab"),
    ("tab", ["type alp\t\r"], "inhalt alpha"),
    ("tab again", ["type al\t\t\r"], "inhalt alpha"),
    ("shift-tab", ["type al" + BACKTAB + "\r"], "inhalt alpha"),
    ("tab first", ["type al\t\r"], "inhalt albert"),
    ("tab quotes a space", ["type bet\t\r"], "inhalt beta"),
    ("tab in a directory", ["type sub/ga\t\r"], "inhalt gamma"),
    ("dir /p waits for a key", ["dir -b -p -on many\r", " "],
     "|".join(["f%02d" % i for i in range(1, 9)] + ["Press any key to continue . . ."] +
              ["f%02d" % i for i in range(9, 13)])),
    ("tab mid-line", ["type xx" + LEFT + LEFT + "alp\t" + DEL + DEL + "\r"], "inhalt alpha"),
]


def run(fdbox, work):
    pid, fd = pty.fork()
    if pid == 0:
        os.chdir(work)
        env = dict(os.environ, PROMPT="$G", LANG="C", LANGUAGE="")
        os.execve(fdbox, [fdbox, "command"], env)
    # a terminal of 10 rows: DIR /P waits after 8
    fcntl.ioctl(fd, termios.TIOCSWINSZ, struct.pack("HHHH", 10, 80, 0, 0))
    out = bytearray()

    def drain(quiet, limit):
        """read until nothing came for quiet seconds (the keys were handled:
        the shell echoes them), at most limit seconds; False at the end"""
        end = time.time() + limit
        last = time.time()
        while time.time() < end and time.time() - last < quiet:
            r, _, _ = select.select([fd], [], [], 0.02)
            if r:
                try:
                    data = os.read(fd, 4096)
                except OSError:
                    return False
                if not data:
                    return False
                out.extend(data)
                last = time.time()
        return True

    drain(0.5, 30)
    for _, keys, _ in CASES:
        for k in keys:
            os.write(fd, k.encode())
            # an Esc alone is told from a sequence by a pause: wait until it
            # was handled before the next keys
            drain(0.3, 30)
    os.write(fd, b"exit\r")
    drain(30, 30)  # to the end: EXIT ends the shell
    try:
        os.kill(pid, 9)
    except ProcessLookupError:
        pass
    os.waitpid(pid, 0)
    return out.decode("utf-8", "replace")


def main():
    fdbox = os.path.abspath(sys.argv[1])
    with tempfile.TemporaryDirectory() as work:
        for name, text in (("alpha.txt", "inhalt alpha"), ("albert.txt", "inhalt albert"),
                           ("beta file.txt", "inhalt beta")):
            with open(os.path.join(work, name), "w") as f:
                f.write(text + "\n")
        os.mkdir(os.path.join(work, "many"))
        for i in range(1, 13):
            open(os.path.join(work, "many", "f%02d" % i), "w").close()
        os.mkdir(os.path.join(work, "sub"))
        with open(os.path.join(work, "sub", "gamma.txt"), "w") as f:
            f.write("inhalt gamma\n")
        text = run(fdbox, work)
    # what the commands printed: the lines that are not a prompt with keys
    lines = [l.rstrip("\r") for l in text.split("\n")]
    printed = [l for l in lines if l and not l.startswith(">")]
    want = []
    for _, _, w in CASES:
        want.extend(w.split("|"))
    failed = 0
    for i, w in enumerate(want):
        got = printed[i] if i < len(printed) else "(nothing)"
        if got != w:
            failed += 1
    if failed or len(printed) != len(want):
        print("FAIL: the lines printed differ")
        print("--- expected")
        print("\n".join(want))
        print("--- got")
        print("\n".join(printed))
        print("--- the terminal")
        print(repr(text))
        return 1
    print("readline: %d cases passed" % len(CASES))
    return 0


sys.exit(main())
