#include <sys/syscall.h>
#include <unistd.h>

int main() {
	if (syscall(SYS_geteuid) != 0) {
		write(1, "Error: Not running as root.\n", 27);
		return 1;
	}
	syscall(SYS_mkdir, "/proc", 0555);
	syscall(SYS_mount, "proc", "/proc", "proc", 0, NULL);
	syscall(SYS_mkdir, "/sys", 0555);
	syscall(SYS_mount, "sys", "/sys", "sys", 0, NULL);
	
	syscall(SYS_sync);
	syscall(SYS_sync);
	syscall(SYS_reboot, 0x4321fedc);
	return 0;
}