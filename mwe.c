#define _GNU_SOURCE

// #include "lib_micro_x86/ldat.h"
// #include "lib_micro_x86/patch.h"
// #include "lib_micro_x86/ucode_macro.h"



#include <stdio.h>
#include <stdint.h>
#include "newlib/misc.h"
#include "newlib/ucode_macro.h"
#include "newlib/udbg.h"
#include "newlib/opcode.h"

// void do_rdrand_patch() {
// 	uint64_t patch_addr = 0x7da0;
// 	ucode_t ucode_patch[] = {
// 		/* Manipulate rax */
// 		{
// 			ZEROEXT_DSZ64_DI(RAX, 0x8008), /* Write zero extended */
// 			NOP,
// 			NOP,
// 			END_SEQWORD
// 		}
// 	};

// 	patch_ucode(patch_addr, ucode_patch, ARRAY_SZ(ucode_patch));
// 	hook_match_and_patch(0, RDRAND_XLAT, patch_addr);
// }

int main(int argc, char* argv[]) {
	crbus_write(PMH_CR_BRAM_BASE, 0x1);
	// do_fix_IN_patch();
	// do_rdrand_patch();

	register uint32_t eax asm("eax");

	eax = 0x11223344;
	asm volatile("rdrand eax");
	printf("eax: 0x%08x\n", eax);
	return 0;
}