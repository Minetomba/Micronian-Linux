/* Includes */
#include <linux/limits.h>
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
						printf("%c - %s\n", e->d_type, (char*)e->d_name);
					}
				} else {
					printf(CWD_ERROR);
				}
			}
		}
	}
	return 0;
}