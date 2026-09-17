# OSF-Linux
**O**ne-**S**-tatic-**F**ile Linux is a minimalist distribution designed to be as small and efficient as possible.

## Installation
Compile with "./compile.sh", move into /sbin, and tell the kernel for it to be PID 1.
To shutdown the system, send SIGUSR1 to PID 1 via syscall(SYS_kill, 1, 10), making sure only the root user can send that signal and be succesful.
To configure the system, make a file named "/etc/initconf.txt" owned by root editable only by root, and type line by line the paths to the binaries to execute. For example, this is a very small yet functional initconf that spawns iwd, sshd, and a shell all as root:
```conf
/bin/iwd
/sbin/sshd
/bin/sh
```
By default, you should only use /bin and /sbin for binaries.
All binaries in initconf.txt get launched as root, which means you might need to make a wrapper to switch users and launch a process based on file input (since arguments are not specified via initconf, initconf is just a list of paths to launch).

## Dependencies
- POSIX-compliant Linux