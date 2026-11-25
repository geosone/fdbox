# fdbox

A new command interpreter for FreeDOS. Code will contain usable utilities
borrowed from Unix and maintain (basic) compatibility with 
MSDOS 6.22 `command.com`, while (sometimes) improving. 

[![Build status](https://github.com/elcuco/fdbox/actions/workflows/build.yml/badge.svg?branch=main)](https://github.com/elcuco/fdbox/actions/workflows/build.yml) ![License](https://img.shields.io/github/license/elcuco/fdbox.svg)

<img src="https://img.shields.io/github/languages/top/elcuco/fdbox.svg" alt="Languages"/>
<img src="https://img.shields.io/github/repo-size/elcuco/fdbox.svg" alt="Repository size"/>
<img src="https://img.shields.io/github/commit-activity/y/elcuco/fdbox" alt="Activity"/>

<p align="center">
<img src="fdbox-dosbox.png" alt="DOSBox" />
</p>

The program compiles also as a native Linux executable
<p align="center">
<img src="fdbox-linux.png" alt="Linux"/>
</p>

.. and Windows as well 
<p align="center">
<img src="fdbox-win10.png" alt="Windows"/>
</p>

.. and Apple silicon as well 
<p align="center">
<img src="fdbox-osx-apple-silicon.png" alt="OSX"/>
</p>

Yes this code compiles on DOS 8086 16bit, Windows 8086 32bit and Linux amd64 and OSX64/ARM!. I see no reasons why should it fail
to compile on other platforms/OS.

## Status

 * Only DOS commands are supported. Unix commands might follow.
 * Code compiles using clang/GCC (Windows+Linux+OSX), TCC (DOS), OpenWatcom2/snapshot (DOS).   
 * Github actions compiles all the code for Linux, Windows, MSDOS and OSX, 
   the workflow also runs the test on all platforms. (tests do not run on MSDOS yet).
 * Currently, code uses LibC's functions - so it is limited to
   standard C code support for localization.
 * All commands support multiple arguments (like Unix shells), unlike DOS
   which you cannot do `del /r file1.txt *.bat /f`, and arguments can come
   even after file names (not only at the beginning of the command).
 * Some commands share the same code (`copy`+`move`, `date`+`time`)
 * Interactive shell is being worked on. I want to have 4DOS command  
   completion, or bash/zsh. 
 * Contains a bash shell script to generate the development environment
   for DOS+DosBOX.

See also: 

 * [coding.md](coding.md)
 * [building.md](building.md)
 * [readme.txt](readme.txt)

## TODO
* Command (main interactive shell) does not support batch files. See https://github.com/elcuco/fdbox/tree/batch-files-support branch.
* Command does not support redirection
* Command does not support "|"
* `TurboC` does need a proper makefile.
* Make OW build a Windows binary (as well)
* OW build is done by using gnu-make and not ow-make
* Setup CI/CD to compile a DJGPP cross compilation
* Setup CI/CD to run dosemu in console mode, and compile using TC
* Setup CI/CD to run dosemu in console mode, and run tests
* History is not saved/read.
* I had this `configure` system which would create the sources for commands
   and auto generate the `applets.c` file. Not against the idea.
* There are commands that are available on Unix and DOS, but differ 
   in syntax  unsure how to handle.
* MSDOS: Using XMS swapping would be good, using SPWNO would be epic (http://www.cs.cmu.edu/%7Eralf/files.html)
* Platform specific APIs (`&` on Windows/Linux to run a program in 
  background for example).
* On Linux we can print escape sequences to make files on `dir`
  be clickable. 
* see also implementation tables bellow

## Implementation status of commands
| command                 | status  | remark                                                                                                     |
|-------------------------|---------|------------------------------------------------------------------------------------------------------------|
| `beep`                  | *done*  | needs to be tested on DOS/Windows, on unix it just beeps                                                   |
| `call`                  | *done*  |                                                                                                            |
| `cd`/`chdir`            | *done*  |                                                                                                            |
| `cls`                   | *done*  |                                                                                                            |
| `copy`                  | *done*  | Binary copies by design. Patterns, several sources into a directory, `copy a+b c` and `copy a+b` (onto a), without a target into the current directory. No recursive copy (that is XCOPY's) |
| `command`               | *done*  | the shell: pipes, redirections, batch files, error levels                                                  |
| `date`/`time`           | *done*  | `time` reads `hh:mm[:ss]` with am/pm. Setting them is not supported on Windows                            |
| `del`/`erase`           | *done*  | a directory: its files (DOS); `/r` the tree too                                                            |
| `dir`                   | *done*  | `/o[-][nesdg]...` sort keys in order (`/o` alone: `/ogn`), `/p` pauses a screen (a key goes on), `/b` names only, the free space (not yet on GonWare and DOS) |
| `echo`                  | *done*  |                                                                                                            |
| `for`                   | *done*  |                                                                                                            |
| `if`                    | *done*  |                                                                                                            |
| `md`/`mkdir`            | *done*  | `/r` (`mkdir -p`) the parents too                                                                          |
| `move`/`rename`/`ren`   | *done*  | All these command are aliases, hardcoded                                                                   |
| `prompt`                | *done*  |                                                                                                            |
| `rd`/`rmdir`            | *done*  |                                                                                                            |
| `rem`                   | *done*  |                                                                                                            |
| `set`                   | *done*  |                                                                                                            |
| `type`                  | *done*  | As a bonus, can also print line numbers                                                                    |
| `ver`                   | *done*  |                                                                                                            |

The unix side, small as in BusyBox (the bigger tools are programs of
their own): `cat`, `clear`, `cp`, `mv`, `rm` (`-r`, `-f`) as names of the
DOS commands; `basename`, `cal`, `cmp`, `cut`, `dirname`, `env`, `false`,
`head`, `hexdump`, `ls`, `pwd`, `rev`, `seq`, `sleep`, `sort`, `tail`,
`tee`, `touch`, `tr`, `true`, `uname`, `uniq`, `wc`, `which`, `yes`. They
get their arguments as programs do (`env X=1` keeps its `=`).

## Command line editor status:
| Key                       | status  | remark                                                        |
|---------------------------|---------|---------------------------------------------------------------|
| Left/right/home/end       | *done*  |                                                               |
| Up/Down: history          | *done*  | the line being typed comes back with Down                     |
| Backspace, Del            | *done*  |                                                               |
| control+a : HOME          | *done*  |                                                               |
| control+b : word left     | *done*  |                                                               |
| control+c : cancel        | *done*  | gives the line up; while a command runs it stays the signal  |
| control+d : EOF           | *done*  |                                                               |
| control+e : END           | *done*  |                                                               |
| control+f : word right    | *done*  |                                                               |
| control+k / control+u     | *done*  | delete to the end / to the start                              |
| control+l : cls           | *done*  |                                                               |
| control+arrows            | *done*  | word left/right (terminal sequences and DOS scan codes; not yet Windows) |
| esc                       | *done*  | clears the line, as in DOS                                    |
| TAB / shift+TAB           | *done*  | file names as cmd.exe completes them: the next / the one before; POSIX and GonWare only |
| insert/override           | *done*  | Cursor shape not supported on OSX                             |

## Tests
`ctest` in the build directory runs the unit tests (`tests`), every
command and the shell on the host (`tests/commands.sh`: pipes,
redirections, batch files, error levels) and the line editor in a pseudo
terminal (`tests/readline.py`). `-DFDBOX_FORMAT=OFF` configures without
fetching Format.cmake (no network needed).

## Compiler support
 1. Linux: Clang/GCC - this is the main development environment. Supported.
 2. Windows10: MinGW/TCM (9,10) - this is the main development environment. Supported.
 3. DOS: TC 2.02 - this is the main development environment. Supported.
    > __BUG__:
    >
    > `glob()` functionality tends to allocate too much >memory, and the code is faulty, 
    > not reporting this to the app. Results in garbage in the screen when
    > `dir ` on a directory with lots of files (~80 on my tests, but it depends
    > on memory available).
 4. DOS/OpenWatcom (WIP): we have a CI building it. The generated binary crashes on startup.
 5. OSX: Works (similar code path as Linux)
 6. DOS: DJGPP - I am having problems in runtime. Seems like `tolower()` is breaking
    my code. Might be a compiler bug. Interactive shell segfaults.
 7. DOS: PacificC - It's not a high priority, but we have a working branch called `pacific-c-support`
    which contains the work needed to make this project compile with that compiler.

## Why?
Because.

I started this project at 2001, and only recently re-started it from scratch. The idea came
after I saw Minibox, and though (as every programmer does...) "this is wrong, this is not the
way to do this, I will do it better". I am unsure if I am doing it better, but this is the
best way to thank Ercan Ersoy - you give me a push to restart this project: thank you.

My old project:
http://dgi_il.tripod.com/fds/index.html

Minibox:
https://github.com/ercanersoy/Minibox

Ideas and inspiration do come from busybox (the applet concept for
example). However - this project does not share any code, and cannot
be considered a derived work (please don't copy code from Busybox here,
as I might re-license code, and want new implementations for those old ideas).

## DosBox setup

I created a Linux shell script which will populate a "C:" drive for development.
Just execute it, and it will download all the compilers, and create an AUTOEXEC.BAT.
Do this inside the `etc` directory.

Then in DosBox config, edited the start-up files (usually at the end) to this:


    mount c /home/diego/fdbox/etc/dosbox
    mount d /home/diego/fdbox/
    C:\AUTOEXEC.BAT
    d:

Then, each time I start dosbox, I have in the path all the compilers I need, and
I am redirected to the full code of the program in drive D:. The project contains
a TurboC project file - so typing `tc` will bring up the IDE with the project ready
to hack.

Similar setup can be done from the GUI of DOSBOX-X, it just needs more clicks.


Linux's requirements (on Ubuntu I found that I need `p7zip-full` instead of `7z`:
```
apt install 7z wget dosbox
```

OSX requirements:
```
brew install p7zip wget dosbox
```

Windows:
 * Download [7zip](https://www.7-zip.org/download.html), the script will only look in default location
 * You can run this script from Github's bash (use official git for windows install)

TODO: If you save a file under DOS, DosBox will rename it to capital letters. I am
unsure how to automatically do this.

## License
GPL V3. See file license.txt
