#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include "newlib/misc.h"
#include "newlib/ucode_macro.h"
#include "newlib/udbg.h"
#include "newlib/opcode.h"
#include "newlib/match_and_patch_hook.h"

uint32_t ucode_addr_to_patch_addr(uint32_t addr) {
    return addr - 0x7c00;
}
uint32_t ucode_addr_to_patch_seqword_addr(uint32_t addr) {
    uint32_t base = addr - 0x7c00;
    uint32_t seq_addr = ((base%4) * 0x80 + (base/4));
    return seq_addr % 0x80;
}
void ldat_array_write(uint32_t pdat_reg, uint32_t array_sel, uint32_t bank_sel, uint32_t dword_idx, uint32_t fast_addr, uint64_t val) {
    uint64_t prev = crbus_read(0x692);
    crbus_write(0x692, prev | 1);

    crbus_write(pdat_reg + 1, 0x30000 | ((dword_idx & 0xf) << 12) | ((array_sel & 0xf) << 8) | (bank_sel & 0xf));
    crbus_write(pdat_reg, 0x000000 | (fast_addr & 0xffff));
    crbus_write(pdat_reg + 4, val & 0xffffffff);
    crbus_write(pdat_reg + 5, (val >> 32) & 0xffff);
    crbus_write(pdat_reg + 1, 0);

    crbus_write(0x692, prev);
}
void ms_array_write(uint32_t array_sel, uint32_t bank_sel, uint32_t dword_idx, uint32_t fast_addr, uint64_t val) {
    ldat_array_write(0x6a0, array_sel, bank_sel, dword_idx, fast_addr, val);
}
/**
 * write a single microcode instruction to ms_array 4.
 * @param addr: The address to write to.
 * @param val: microcode instruction to write as a uint64_t.
 */
static void ms_array_4_write(uint32_t addr, uint64_t val) {return ms_array_write(4, 0, 0, addr, val); }
/**
 * write a single microcode instruction to ms_array 2.
 * @param addr: The address to write to.
 * @param val: microcode instruction to write as a uint64_t.
 */
static void ms_array_2_write(uint32_t addr, uint64_t val) {return ms_array_write(2, 0, 0, addr, val); }

void patch_ucode(uint32_t addr, ucode_t ucode_patch[], int n) {
    // format: uop0, uop1, uop2, seqword
    // uop3 is fixed to a nop and cannot be overridden
    for (int i = 0; i < n; i++) {
        // patch ucode
        ms_array_4_write(ucode_addr_to_patch_addr(addr + i*4)+0, CRC_UOP(ucode_patch[i].uop0));
        ms_array_4_write(ucode_addr_to_patch_addr(addr + i*4)+1, CRC_UOP(ucode_patch[i].uop1));
        ms_array_4_write(ucode_addr_to_patch_addr(addr + i*4)+2, CRC_UOP(ucode_patch[i].uop2));
        // patch seqword
        ms_array_2_write(ucode_addr_to_patch_seqword_addr(addr) + i, CRC_SEQ(ucode_patch[i].seqw));
    }
}

void hook_match_and_patch(uint32_t entry_idx, uint32_t ucode_addr, uint32_t patch_addr) {
	if (ucode_addr % 2 != 0) {
		printf("[-] uop address must be even\n");
		return;
	}
	if (patch_addr % 2 != 0) {
		printf("[-] patch uop address must be even\n");
		return;
	}

	uint32_t dst = patch_addr / 2;
	uint32_t patch_value = (dst << 16) | ucode_addr | 1;

	patch_ucode(match_and_patch_hook_addr, match_and_patch_hook_ucode_patch, ARRAY_SZ(match_and_patch_hook_ucode_patch));
	uint64_t ret = ucode_invoke_2(match_and_patch_hook_addr, patch_value, entry_idx<<1);
	/* if (verbose) */
	/*     printf("hook_match_and_patch: %lx\n", ret); */
}

void do_fix_IN_patch() {
	// Patch U58ba to U017a
	hook_match_and_patch(0x1f, 0x58ba, 0x017a);
}

void do_rdrand_patch(void) {
	uint32_t patch_addr = 0x7da0;

	/* Add 1 to rcx if rax != rbx */
	ucode_t ucode_patch[] = {
		// { /* rcx = 0x1337 */
		// 	ZEROEXT_DSZ64_DI(RCX, 0x1337), /* Write zero extended */
		// 	NOP,
		// 	NOP,
		// 	END_SEQWORD
		// },

		{
			NOP,
			NOP,
			SUBR_DSZ32_DRR(TMP0, RAX, RBX),	/* tmp0 = rax - rbx. tmp0 now has per-register flags set */
			SEQ_GOTO2(patch_addr + 0x08),
			// NOP_SEQWORD,
		},
		{ // 0x04
			MOVE_DSZ32_DI(RCX, 0xA0A1A2A3),
			// NOP,
			NOP,
			NOP,
			END_SEQWORD
		},
		{ // 0x08
			MOVE_DSZ32_DR(TMP0, RCX),		/* Move current value of rcx to tmp0, because ucode ADD can */
			ADD_DSZ32_DRI(RCX, TMP0, 1),	/* operate on only one architectual register at a time, it seems */
			MOVE_DSZ64_DI(RCX, 0xA0A1A2A3),
			// NOP,
			END_SEQWORD
		},

		// {
		// 	SUB_DSZ32_DRR(TMP0, RAX, RBX),	/* tmp0 = rax - rbx. tmp0 now has per-register flags set */
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x04),
		// 	NOP,
		// 	END_SEQWORD
		// },
		// { // 0x04
		// 	MOVE_DSZ32_DR(TMP0, RCX),		/* Move current value of rcx to tmp0, because ucode ADD can */
		// 	ADD_DSZ32_DRI(RCX, TMP0, 1),	/* operate on only one architectual register at a time, it seems */
		// 	NOP,
		// 	END_SEQWORD
		// },
		// { /* Alternative code with no branching (negative results yield huge jumps) */
		// 	SUB_DSZ64_DRR(TMP0, RAX, RBX),	/* tmp0 = rax - rbx. tmp0 now has per-register flags set */
		// 	ADD_DSZ64_DRR(RCX, TMP0, RCX),
		// 	NOP,
		// 	END_SEQWORD
		// },
		// { /* Alternative code with xor and or to highlight all flipped bits */
		// 	XOR_DSZ64_DRR(TMP0, RAX, RBX),	/* tmp0 = rax - rbx. tmp0 now has per-register flags set */
		// 	OR_DSZ64_DRR(RCX, TMP0, RCX),
		// 	NOP,
		// 	END_SEQWORD
		// },
	};

	patch_ucode(patch_addr, ucode_patch, ARRAY_SZ(ucode_patch));
	hook_match_and_patch(0, RDRAND_XLAT, patch_addr);
}

int main(int argc, char* argv[]) {
	uint32_t operand1 = 1, operand2 = 1, ecx_value = 0;

	printf("[+] Before patching\n");
	printf("[.] rdrand ecx [eax: 0x%x - ebx: 0x%x]: ", operand1, operand2);
	// NOTE: This asm uses intel syntax
	__asm__ __volatile__ (
		"xor %%ecx, %%ecx\t\n"
		"mov %%eax, %[op1];\t\n"
		"mov %%ebx, %[op2];\t\n"
		"rdrand ecx;\t\n" // operand2 - operand1
		"mov %[ecx_value], %%ecx;\t\n"

		: [ecx_value]	"=r" (ecx_value)				// Output operands
		: [op1]			"r" (operand1),					// Input operands
		  [op2]			"r" (operand2)
		: "%eax", // result_a							// Clobbered register
		  "%ebx", // result_b
		  "%ecx"  // scratch
	);
	printf("0x%x\n", ecx_value);


	do_fix_IN_patch();
	do_rdrand_patch();
	printf("[+] Patch applied\n");

	// for (int i = 0; /*i < 10*/ ; i++) {
	// 	ecx_value = 0;
	// 	if (i % 2 == 0) {
	// 		operand1++;
	// 	} else {
	// 		operand2++;
	// 	}
	// 	printf("rdrand ecx [eax: 0x%x - ebx: 0x%x]: ", operand1, operand2);
	// 	__asm__ __volatile__ (
	// 		"xor %%ecx, %%ecx\t\n"
	// 		"mov %%eax, %[op1];\t\n"
	// 		"mov %%ebx, %[op2];\t\n"
	// 		"rdrand ecx;\t\n" // operand2 - operand1
	// 		"mov %[ecx_value], %%ecx;\t\n"

	// 		: [ecx_value]	"=r" (ecx_value)				// Output operands
	// 		: [op1]			"r" (operand1),					// Input operands
	// 		  [op2]			"r" (operand2)
	// 		: "%eax", // result_a							// Clobbered register
	// 		  "%ebx", // result_b
	// 		  "%ecx"  // scratch
	// 	);
	// 	printf("0x%x\n", ecx_value);
	// }


	operand1 = 1, operand2 = 1, ecx_value = 0;
	printf("[.] rdrand ecx [eax: 0x%x - ebx: 0x%x]: ", operand1, operand2);
	// NOTE: This asm uses intel syntax
	__asm__ __volatile__ (
		"xor %%ecx, %%ecx\t\n"
		"mov %%eax, %[op1];\t\n"
		"mov %%ebx, %[op2];\t\n"
		"rdrand ecx;\t\n" // operand2 - operand1
		"mov %[ecx_value], %%ecx;\t\n"

		: [ecx_value]	"=r" (ecx_value)				// Output operands
		: [op1]			"r" (operand1),					// Input operands
		  [op2]			"r" (operand2)
		: "%eax", // operand1							// Clobbered register
		  "%ebx", // operand2
		  "%ecx"  // result
	);
	printf("0x%x\n", ecx_value);

	operand1 = 0b01, operand2 = 0b00, ecx_value = 0;
	printf("[.] rdrand ecx [eax: 0x%x - ebx: 0x%x]: ", operand1, operand2);
	// NOTE: This asm uses intel syntax
	__asm__ __volatile__ (
		"xor %%ecx, %%ecx\t\n"
		"mov %%eax, %[op1];\t\n"
		"mov %%ebx, %[op2];\t\n"
		"rdrand ecx;\t\n" // operand2 - operand1
		"mov %[ecx_value], %%ecx;\t\n"

		: [ecx_value]	"=r" (ecx_value)				// Output operands
		: [op1]			"r" (operand1),					// Input operands
		  [op2]			"r" (operand2)
		: "%eax", // result_a							// Clobbered register
		  "%ebx", // result_b
		  "%ecx"  // scratch
	);
	printf("0x%x\n", ecx_value);

	printf("[+] To reset the patch use the -r setting of any program in the tools folder\n");
}