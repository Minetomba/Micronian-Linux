#include "unistd.h"
#include "syscall.h"
#include "fcntl.h"
#include "stdint.h"

int mlc(const char *src) {
	intptr_t stack[16];
	intptr_t stack_pointer = 0;

	char c;
	intptr_t n;

	intptr_t in = syscall(SYS_open, src, O_RDONLY);
	if (in < 0) {
		return 1;
	}

	while ((n = read(in, &c, 1)) == 1) {
		if (c == '1') {
			stack[stack_pointer] = 0;
			stack_pointer -= 1;
		} else if (c == '2') {
			stack[stack_pointer] = *(intptr_t*)stack[stack_pointer];
		} else if (c == '3') {
			*(intptr_t*)stack[stack_pointer] = stack[stack_pointer - 1];
		} else if (c == '4') {
			stack[stack_pointer] += stack[stack_pointer - 1];
		} else if (c == '5') {
			int temp_a = stack[stack_pointer];
			int temp_b = stack[stack_pointer - 1];
			stack[stack_pointer] = temp_b;
			stack[stack_pointer - 1] = temp_a;
		} else if (c == '6') {
			int temp_a = stack[stack_pointer];
			int temp_b = stack[stack_pointer - 2];
			stack[stack_pointer] = temp_b;
			stack[stack_pointer - 2] = temp_a;
		} else if (c == '7') {
			stack_pointer += 1;
			stack[stack_pointer] = stack[stack_pointer - 1];
		} else if (c == '8') {
			stack_pointer += 1;
			stack[stack_pointer] = 1;
		} else {
			syscall(SYS_write, 1, "Syntax error!\n", (intptr_t)(sizeof("Syntax error!\n") - 1));
		}
	}

	syscall(SYS_close, in);
	return 0;
}