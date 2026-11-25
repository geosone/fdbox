#!/bin/sh
# This file is part of fdbox
# For license - read license.txt
#
# commands.sh <fdbox> - the commands of fdbox on the host: each applet
# called directly (fdbox NAME args) and lines run by the shell (pipes,
# redirections, batch files, error levels), in an empty directory. Prints
# every failure with what was expected and what came; the exit code is
# the number of failures (0: all passed).

set -u
F=$1
case $F in /*) ;; *) F=$(pwd)/$F ;; esac
work=$(mktemp -d)
trap 'rm -rf "$work"' EXIT
cd "$work" || exit 1
failed=0 passed=0

# expect NAME EXPECTED command... - the output (stdout and stderr) is
# EXPECTED
expect() {
	name=$1 want=$2
	shift 2
	got=$("$@" 2>&1)
	if [ "$got" = "$want" ]; then
		passed=$((passed + 1))
	else
		failed=$((failed + 1))
		printf 'FAIL: %s\n--- expected\n%s\n--- got\n%s\n' "$name" "$want" "$got"
	fi
}

# status NAME CODE command... - the exit code is CODE
status() {
	name=$1 want=$2
	shift 2
	"$@" >/dev/null 2>&1
	got=$?
	if [ "$got" = "$want" ]; then
		passed=$((passed + 1))
	else
		failed=$((failed + 1))
		printf 'FAIL: %s: exit code %s, not %s\n' "$name" "$got" "$want"
	fi
}

# truth NAME command... - the command (a shell test) succeeds
truth() {
	name=$1
	shift
	if "$@"; then
		passed=$((passed + 1))
	else
		failed=$((failed + 1))
		printf 'FAIL: %s\n' "$name"
	fi
}

# shell LINES - the shell's output for these lines (no prompt)
shell() { printf '%s\n' "$@" | PROMPT= "$F" command; }
# with INPUT run an applet with INPUT on its standard input
with() {
	input=$1
	shift
	printf '%s' "$input" | "$F" "$@"
}

printf 'alpha\nbeta\n' >a.txt
printf 'alpha\nbetA\n' >b.txt
printf 'alpha\n' >c.txt

# --- unix commands
expect "true" "" "$F" true
status "true status" 0 "$F" true
status "false status" 1 "$F" false
expect "seq last" "$(printf '1\n2\n3')" "$F" seq 3
expect "seq first step last" "10,7,4,1" "$F" seq -s , 10 -3 1
expect "seq empty" "" "$F" seq 5 1
status "seq not a number" 1 "$F" seq x
expect "rev" "$(printf 'cba\neßürG')" with "$(printf 'abc\nGrüße\n')" rev
expect "yes" "$(printf 'ja\nja\nja')" sh -c "'$F' yes ja | head -3"
expect "tee" "hallo" with "hallo
" tee t1 t2
expect "tee files" "$(printf 'hallo\nhallo')" cat t1 t2
with "mehr
" tee -a t1 >/dev/null
expect "tee -a" "$(printf 'hallo\nmehr')" cat t1
expect "cmp differ" "a.txt b.txt differ: byte 10, line 2" "$F" cmp a.txt b.txt
status "cmp differ status" 1 "$F" cmp a.txt b.txt
status "cmp same" 0 "$F" cmp a.txt a.txt
expect "cmp -l" "10 141 101" "$F" cmp -l a.txt b.txt
expect "cmp eof" "cmp: EOF on c.txt after byte 6" "$F" cmp a.txt c.txt
status "cmp missing" 2 "$F" cmp a.txt nix
expect "tr range" "HALLO WELT" with "Hallo Welt
" tr a-z A-Z
expect "tr -s" "Hallo Welt" with "Hallo   Welt
" tr -s ' '
expect "tr -d" "Hao Wet" with "Hallo Welt
" tr -d l
expect "tr class" "a#b#" with "a1b2
" tr '[:digit:]' '#'
expect "tr escape" "$(printf 'x\ny')" with "x y
" tr ' ' '\n'
expect "cut -f" "b:d" with "a:b:c:d
" cut -d : -f 2,4
expect "cut -f open range" "b:c" with "a:b:c
" cut -d: -f2-
expect "cut -c" "bcd" with "abcdef
" cut -c 2-4
expect "cut -c utf-8" "üß" with "Grüße
" cut -c 3-4
expect "cut -s" "y" with "$(printf 'x\ny:z\n')" cut -d: -f1 -s
status "cut no list" 1 "$F" cut a.txt
expect "env" "FOO=1" sh -c "FOO=1 '$F' env | grep '^FOO='"
expect "env set and unset" "FOO=2" sh -c "HOME=/x '$F' env -u HOME FOO=2 | grep -E '^(FOO|HOME)='"
expect "which built-in" "dir: shell built-in command" "$F" which dir
expect "which file" "$(command -v sh)" "$F" which sh
status "which none" 1 "$F" which nix-da-nix
expect "uname" "$(uname -s)" "$F" uname
expect "uname -a" "$(uname -s) $(uname -n) $(uname -r) $(uname -m)" "$F" uname -a
mkdir -p d/sub
touch d/x d/.h
expect "ls" "$(printf 'sub\nx')" "$F" ls d
expect "ls -a" "$(printf '.\n..\n.h\nsub\nx')" "$F" ls -a d
expect "ls -1 file and dir" "$(printf 'a.txt\n\nd:\nsub\nx')" "$F" ls -1 a.txt d
truth "ls -l" sh -c "'$F' ls -l d | grep -q '^d[rwx-]\{9\} .* sub$'"
status "ls missing" 1 "$F" ls nix
expect "head" "alpha" "$F" head -n 1 a.txt
expect "tail" "beta" "$F" tail -n 1 a.txt
expect "wc" "2 2 11 a.txt" sh -c "'$F' wc a.txt | tr -s ' ' | sed 's/^ //'"
expect "sort -r" "$(printf 'beta\nalpha')" "$F" sort -r a.txt
expect "uniq -c" "$(printf '2 a\n1 b')" sh -c "printf 'a\na\nb\n' | '$F' uniq -c | sed 's/^ *//'"
expect "basename" "c.txt" "$F" basename /a/b/c.txt
expect "dirname" "/a/b" "$F" dirname /a/b/c.txt

# --- DOS commands and their unix names
mkdir -p o/zdir o/adir
printf 12345 >o/a.txt
printf 123 >o/b.txt
printf 1 >o/c.log
touch -d 2020-01-01 o/a.txt
expect "dir -b -on" "$(printf 'a.txt\nadir/\nb.txt\nc.log\nzdir/')" "$F" dir -b -on o
expect "dir -b -o-n" "$(printf 'zdir/\nc.log\nb.txt\nadir/\na.txt')" "$F" dir -b -o-n o
expect "dir -b -o (dirs first, then names)" "$(printf 'adir/\nzdir/\na.txt\nb.txt\nc.log')" "$F" dir -b -o o
expect "dir -b -os" "$(printf 'adir/\nzdir/\nc.log\nb.txt\na.txt')" "$F" dir -b -os o
expect "dir -b -od -n" "$(printf 'a.txt\nadir/\nb.txt\nc.log\nzdir/')" "$F" dir -b -odn o
expect "dir -b -oen" "$(printf 'adir/\nzdir/\nc.log\na.txt\nb.txt')" "$F" dir -b -oen o
status "dir -ox" 1 "$F" dir -ox o
truth "dir shows the free space" sh -c "'$F' dir o | grep -q 'dirs(s).*[0-9] free'"
expect "type" "$(printf 'alpha\nbeta')" "$F" type a.txt
expect "cat" "$(printf 'alpha\nbeta')" "$F" cat a.txt
"$F" md m1
truth "md" test -d m1
"$F" mkdir -p p/q/r
truth "mkdir -p" test -d p/q/r
status "rm a directory without -r" 1 "$F" rm p
truth "rm left the directory" test -d p/q/r
status "rm -r" 0 "$F" rm -r p
truth "rm -r removed it" test ! -e p
status "rm -f missing" 0 "$F" rm -f nix
status "rm missing" 1 "$F" rm nix
cp a.txt gone.txt
status "rm a file" 0 "$F" rm gone.txt
truth "rm removed the file" test ! -e gone.txt
mkdir -p e/f
touch e/1 e/f/2
status "del a directory" 0 "$F" del e
truth "del deleted the files in it only" sh -c 'test ! -e e/1 && test -e e/f/2 && test -d e'
"$F" rd m1
truth "rd" test ! -e m1
"$F" cp a.txt k.txt
expect "cp" "$(printf 'alpha\nbeta')" cat k.txt
"$F" mv k.txt l.txt
truth "mv" sh -c 'test ! -e k.txt && test -e l.txt'
echo A >ca
echo B >cb
echo C >cc.x
mkdir cd
"$F" copy ca+cb cab
expect "copy a+b c" "$(printf 'A\nB')" cat cab
"$F" copy ca + cb cab2
expect "copy a + b c" "$(printf 'A\nB')" cat cab2
cp ca ca2
"$F" copy ca2+cb+cc.x
expect "copy a+b+c onto a" "$(printf 'A\nB\nC')" cat ca2
"$F" copy ca cb cd
truth "copy several into a directory" sh -c 'test -e cd/ca && test -e cd/cb'
status "copy several onto a file" 1 "$F" copy ca cb cc.x
expect "copy nothing found" "File not found - nix" "$F" copy nix cd
status "copy nothing found status" 1 "$F" copy nix cd
mkdir ce
(cd ce && "$F" copy ../ca && "$F" copy '../*.x')
truth "copy without a target: here" sh -c 'test -e ce/ca && test -e ce/cc.x'
"$F" move cb cd/cbb
truth "move into a directory" sh -c 'test ! -e cb && test -e cd/cbb'

# TIME reads the time (setting it needs root: "Cannot set" means it was
# understood)
expect "time 25:00" "Invalid time" "$F" time 25:00
expect "time 13:00pm" "Invalid time" "$F" time 13:00pm
expect "time 10:61" "Invalid time" "$F" time 10:61
for t in 1:30p "12:00 am" "11:59:59 PM" 23:15:01.50; do
	truth "time $t" sh -c "'$F' time '$t' 2>&1 | grep -qv 'Invalid time'"
done
truth "time 1:30p is 13:30" sh -c "'$F' time 1:30p 2>&1 | grep -q '13:30:00' || [ \"\$(id -u)\" = 0 ]"
truth "time 12:00 am is 0:00" sh -c "'$F' time '12:00 am' 2>&1 | grep -q '00:00:00' || [ \"\$(id -u)\" = 0 ]"

# --- the shell
expect "shell echo" "hallo welt" shell "echo hallo welt"
expect "shell pipe" "HALLO" shell "echo hallo | tr a-z A-Z"
expect "shell pipe chain" "OLLAH" shell "echo hallo | tr a-z A-Z | rev"
shell "echo eins > r.txt" "echo zwei >> r.txt" >/dev/null
expect "shell redirection" "$(printf 'eins\nzwei')" cat r.txt
expect "shell input redirection" "EINS" shell "head -n 1 < r.txt | tr a-z A-Z"
expect "shell set" "42" shell "set WERT=42" "echo %WERT%"
expect "shell if" "ja" shell "set WERT=42" "if %WERT% == 42 echo ja"
expect "shell errorlevel" "fehler" shell "false" "if errorlevel 1 echo fehler"
expect "shell no errorlevel" "" shell "true" "if errorlevel 1 echo fehler"
expect "shell for" "$(printf 'eins\nzwei')" shell "for %F in (eins, zwei) do echo %F"
expect "shell unix arguments keep =" "X1=ja" shell "env X1=ja | grep X1"
printf 'echo batch %%1\r\n' >t.bat
expect "shell call" "batch hallo" shell "call t.bat hallo"

echo "commands: $passed passed, $failed failed"
exit $failed
