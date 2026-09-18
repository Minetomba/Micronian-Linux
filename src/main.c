/* Includes */
#include <stdlib.h>
#include <limits.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/prctl.h>
#include <sys/reboot.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mount.h>
#include <dirent.h>
#include <syscall.h>
#include <unistd.h>
#include <fcntl.h>

/* Constants */
#define PROCESSES_PATH "/etc/initconf.txt"
#define PROCESS_TIMEOUT 5
#define NOT_ROOT_ERROR "Error: Not running as root.\n"
#define PRESYSINIT_MESSAGE "System launch!\n"
#define USERSPACE_INIT_ERROR "Error: Binary cannot have arguments and be ran as the init system at the same time.\n"
#define CWD_ERROR "Error: Failed to get current directory path.\n"
#define MAX_ARGS 64
#define SHELL_PROMPT "> "
#define BINARY_NOT_FOUND_ERROR "Error: Binary not found."
#define SHELL_HELP_MESSAGE "exit > Make the shell exit\ncd [path] > Make the shell change directory to that path\npwd > Print the shell's current working directory\nexec [path] [args] > Execute the binary at that absolute path with those arguments\nbg [command] [args] > Run that command with those arguments in the background\nexecbg [path] [args] > Execute the binary at that absolute path in the background with the specified arguments\nhelp > Show this message\n[command] [args] > Run that command with those arguments, and wait for it to finish\n"

/* Signal Handler */
static volatile sig_atomic_t shutdown_requested = 0;
static void on_shutdown(int sig) {
	(void)sig;
	shutdown_requested = 1;
}

/* Helper Functions */
int npids() {
	int n = 0;
	struct dirent *e;
	DIR *d = opendir("/proc");
	while ((e=readdir(d))) {
		if (isdigit((unsigned char)e->d_name[0]) != 0) {
			n++;
		}
	}
	closedir(d);
	return n;
}
int isroot() {
	return syscall(SYS_geteuid) == 0;
}
void initfs() {
	/* /dev/console routing */
	int fd = open("/dev/console", O_RDWR | O_NOCTTY);
	if (fd >= 0) {
		dup2(fd, 0);
		dup2(fd, 1);
		dup2(fd, 2);
		if (fd > 2) {
			close(fd);
		}
    }

	/* Directories */
	syscall(SYS_mkdir, "/proc", 0555);
	syscall(SYS_mount, "proc", "/proc", "proc", 0, NULL);
	syscall(SYS_mkdir, "/sys", 0555);
	syscall(SYS_mount, "sys", "/sys", "sys", 0, NULL);
	syscall(SYS_mkdir, "/dev", 0555);
	syscall(SYS_mount, "dev", "/dev", "dev", 0, NULL);
	return;
}
int initsys() {
	/* Signal Handler Initiation */
	struct sigaction sa = {0};
	sa.sa_handler = on_shutdown;
	sa.sa_flags = 0;
	sigemptyset(&sa.sa_mask);
	sigaction(SIGUSR1, &sa, NULL);
	signal(SIGTERM, SIG_IGN);

	/* Startup */
	FILE *file = fopen(PROCESSES_PATH, "r");
	char line[PATH_MAX];
	pid_t pid;
	while (fgets(line, sizeof(line), file)) {
		line[strcspn(line, "\n")] = 0;
		printf("[ ... ] \"%s\"\n", line);
		pid = fork();
		if (pid == 0) {
			printf("[ OK ] pid %d\n", pid);
			char *args[] = {line, NULL};
			execvp(line, args);
			_exit(1);
		}
	}
	fclose(file);
	
	/* Reaping */
	prctl(PR_SET_CHILD_SUBREAPER, 1);
	while (wait(NULL) > 0) {
		if (shutdown_requested) {
			break;
		}
	}

	/* Shutdown */
	int timeout = 1;
	while (npids() > 1) {
		if (timeout > PROCESS_TIMEOUT) {
			kill(-1, SIGKILL);
			break;
		}
		kill(-1, SIGTERM);
		sleep(1);
		timeout ++;
	}
	sync();
	sync();
	reboot(RB_POWER_OFF);
	return 0;
}

int main(int argc, char* argv[]) {
	if (argc == 1) {
		if (!isroot()) {
			syscall(SYS_write, 1, NOT_ROOT_ERROR, sizeof(NOT_ROOT_ERROR) - 1);
			return 1;
		}
		initfs();
		syscall(SYS_write, 1, PRESYSINIT_MESSAGE, sizeof(PRESYSINIT_MESSAGE) - 1);
		initsys();
	} else {
		if (getpid() == 1) {
			syscall(SYS_write, 1, USERSPACE_INIT_ERROR, sizeof(USERSPACE_INIT_ERROR) - 1);
			return 1;
		}
		if (argc == 2) {
			if (strcmp(argv[1], "ls") == 0) {
				struct dirent *e;
				char cwd[PATH_MAX];
				if (getcwd(cwd, sizeof(cwd)) != NULL) {
					DIR *d = opendir(cwd);
					while ((e=readdir(d))) {
						printf("%s\n", (char*)e->d_name);
					}
					closedir(d);
				} else {
					printf(CWD_ERROR);
				}
			}
			if (strcmp(argv[1], "echo") == 0) {
				printf("\n");
			}
			if (strcmp(argv[1], "ps") == 0) {
				struct dirent *e;
				DIR *d = opendir("/proc");
				while ((e=readdir(d))) {
					if (isdigit((unsigned char)e->d_name[0]) != 0) {
						printf("%s > ", e->d_name);
						fflush(stdout);
						char link[64];
						char buf[PATH_MAX];
						snprintf(link, sizeof(link), "/proc/%d/exe", atoi(e->d_name));
						ssize_t len = readlink(link, buf, PATH_MAX - 1);
						if (len == -1) {
							printf("no command\n");
							continue;
						}
						buf[len] = '\0';
						printf("%s\n", buf);
					}
				}
				closedir(d);
			}
			if (strcmp(argv[1], "sh") == 0) {
				while (1) {
					write(1, SHELL_PROMPT, sizeof SHELL_PROMPT - 1);
					char cmd[PATH_MAX];
					int n = read(STDIN_FILENO, cmd, sizeof cmd - 1);
					if (n < 1) {
						_exit(1);
					}
					cmd[n] = '\0';
					if (cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'i' && cmd[3] == 't' && (cmd[4] == ' ' || cmd[4] == 0 || cmd[4] == '\n')) {
						_exit(0);
					} else if (cmd[0] == 'c' && cmd[1] == 'd' && cmd[2] == ' ') {
						char *arg = cmd + 3;
						arg[strcspn(arg, "\n")] = '\0';
						chdir(arg);
					} else if (cmd[0] == 'p' && cmd[1] == 'w' && cmd[2] == 'd' && (cmd[3] == ' ' || cmd[3] == 0 || cmd[3] == '\n')) {
						char output[PATH_MAX];
						if (getcwd(output, sizeof(output)) != NULL) {
							write(1, output, strlen(output));
							write(1, "\n", 1);
						} else {
							write(1, CWD_ERROR, sizeof CWD_ERROR - 1);
						}
					} else if (cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'e' && cmd[3] == 'c' && cmd[4] == ' ') {
						char *argv[MAX_ARGS];
						int argc = 0;
						char *tok = strtok(cmd, " \t\n");
						while (tok && argc < MAX_ARGS - 1) {
							argv[argc++] = tok;
							tok = strtok(NULL, " \t\n");
						}
						argv[argc] = NULL;
						for (int j = 0; j < MAX_ARGS - 1; j++) {
							argv[j] = argv[j + 1];
						}
						int pid = fork();
						if (pid == 0) {
							execve(argv[0], argv, NULL);
							write(1, BINARY_NOT_FOUND_ERROR, sizeof BINARY_NOT_FOUND_ERROR - 1);
							_exit(127);
						}
						waitpid(pid, NULL, 0);
					} else if (cmd[0] == 'b' && cmd[1] == 'g' && cmd[2] == ' ') {
						char *argv[MAX_ARGS];
						int argc = 0;
						char *tok = strtok(cmd, " \t\n");
						while (tok && argc < MAX_ARGS - 1) {
							argv[argc++] = tok;
							tok = strtok(NULL, " \t\n");
						}
						argv[argc] = NULL;
						for (int j = 0; j < MAX_ARGS - 1; j++) {
							argv[j] = argv[j + 1];
						}
						int pid = fork();
						if (pid == 0) {
							execvp(argv[0], argv);
							write(1, BINARY_NOT_FOUND_ERROR, sizeof BINARY_NOT_FOUND_ERROR - 1);
							_exit(127);
						}
					} else if (cmd[0] == 'e' && cmd[1] == 'x' && cmd[2] == 'e' && cmd[3] == 'c' && cmd[4] == 'b' && cmd[5] == 'g' && cmd[6] == ' ') {
						char *argv[MAX_ARGS];
						int argc = 0;
						char *tok = strtok(cmd, " \t\n");
						while (tok && argc < MAX_ARGS - 1) {
							argv[argc++] = tok;
							tok = strtok(NULL, " \t\n");
						}
						argv[argc] = NULL;
						for (int j = 0; j < MAX_ARGS - 1; j++) {
							argv[j] = argv[j + 1];
						}
						int pid = fork();
						if (pid == 0) {
							execve(argv[0], argv, NULL);
							write(1, BINARY_NOT_FOUND_ERROR, sizeof BINARY_NOT_FOUND_ERROR - 1);
							_exit(127);
						}
					} else if (cmd[0] == 'h' && cmd[1] == 'e' && cmd[2] == 'l' && cmd[3] == 'p' && (cmd[4] == ' ' || cmd[4] == 0 || cmd[4] == '\n')) {
						write(1, SHELL_HELP_MESSAGE, sizeof SHELL_HELP_MESSAGE - 1);
					} else {
						char *argv[MAX_ARGS];
						int argc = 0;
						char *tok = strtok(cmd, " \t\n");
						while (tok && argc < MAX_ARGS - 1) {
							argv[argc++] = tok;
							tok = strtok(NULL, " \t\n");
						}
						argv[argc] = NULL;
						int pid = fork();
						if (pid == 0) {
							execvp(argv[0], argv);
							write(1, BINARY_NOT_FOUND_ERROR, sizeof BINARY_NOT_FOUND_ERROR - 1);
							_exit(127);
						}
						waitpid(pid, NULL, 0);
					}
				}
			}
		}
	}
	return 0;
}