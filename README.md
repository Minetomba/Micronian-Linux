# Micronium-Linux
Micronium Linux is a minimalist distribution designed to be as small and efficient as possible.

## Installation
Compile with "./compile.sh" (musl-gcc must be installed), move into /sbin, and tell the kernel for it to be PID 1.
To shutdown the system, send SIGUSR1 to PID 1 via syscall(SYS_kill, 1, 10), making sure only the root user can send that signal and be succesful in doing so.
To configure the system, make a file named "/etc/initconf.txt" owned by root editable only by root, and type line by line the paths to the binaries to execute. For example, this is a very small yet functional initconf that spawns iwd, sshd, and a shell all as root:
```conf
/bin/iwd
/sbin/sshd
/bin/sh
```
By default, you should only use /bin and /sbin for binaries.
All binaries in initconf.txt get launched as root, which is good because it forces user processes to never be launched under the init process, and instead be launched under something like a login manager or shell.

## Runtime dependencies
- POSIX-compliant Linux.

## Compile-time dependencies
- musl-gcc, bash, and Linux.

## Micronium Stack Language Interpreter
- Write it however you want. The interpreter only reads numbers and operators and ignores everything else, so you can write "push 5 and 48 and add them together + and then print them out ." and "5 48 + ." and it would do the same thing.