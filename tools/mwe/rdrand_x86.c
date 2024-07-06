#define _GNU_SOURCE

#include <stdint.h>
#include <stdio.h>

int main(int argc, char* argv[]) {
	register uint32_t eax asm("eax");

	eax = 0x11223344;
	asm volatile("rdrand eax");
	printf("eax: 0x%08x\n", eax);
	return 0;
}