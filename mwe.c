#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include "newlib/misc.h"
#include "newlib/ucode_macro.h"
#include "newlib/udbg.h"
#include "newlib/opcode.h"
#include "newlib/match_and_patch_hook.h"

#define ESTIMATE_ROUNDS 1000
#define REP10(BODY) BODY BODY BODY BODY BODY BODY BODY BODY BODY BODY
#define REP100(BODY) REP10(REP10(BODY))

static void sort(uint32_t *arr, int n) {
	/*
	 * Simple bubble sort implementation to sort an array of uint32_t values.
	 * Good enough to calculate the median of a small array.
	 */
	for (int i = 0; i < n; i++) {
		for (int j = i + 1; j < n; j++) {
			if (arr[i] > arr[j]) {
				uint32_t temp = arr[i];
				arr[i] = arr[j];
				arr[j] = temp;
			}
		}
	}
}

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
}

void do_fix_IN_patch() {
	// Patch U58ba to U017a
	hook_match_and_patch(0x1f, 0x58ba, 0x017a);
}

void do_rdrand_patch() {
	uint32_t patch_addr = 0x7da0;

	ucode_t ucode_patch[] = { /* rdrand %source; %source := rax == rbx ? 1 : 2 */

		// { /* This works */
		// 	SUB_DSZ64_DRR(TMP0, RAX, RBX),
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x08),
		// 	NOP,
		// 	(SEQ_NOP | SEQ_NEXT | SEQ_SYNCFULL(1) )
		// }, {
		// 	NOP,
		// 	NOP,
		// 	ZEROEXT_DSZ64_DI(R64SRC, 1),
		// 	END_SEQWORD
		// }, {
		// 	ZEROEXT_DSZ64_DI(R64SRC, 2),
		// 	NOP,
		// 	NOP,
		// 	END_SEQWORD
		// }

		{ /* This also works, more compact */
			SUB_DSZ64_DRR(TMP0, RAX, RBX),
			UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x04),
			ZEROEXT_DSZ64_DI(R64SRC, 1),
			(SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCFULL(1) )
		}, {
			ZEROEXT_DSZ64_DI(R64SRC, 2),
			NOP,
			NOP,
			END_SEQWORD
		}
	};

	patch_ucode(patch_addr, ucode_patch, ARRAY_SZ(ucode_patch));
	hook_match_and_patch(0, RDRAND_XLAT, patch_addr);
}

int main(int argc, char* argv[]) {
	uint32_t operand1 = 1, operand2 = 1, result = 0;
	uint32_t measurements[ESTIMATE_ROUNDS], t1, t2;

	do_fix_IN_patch();
	do_rdrand_patch();

	for (int i = 0; i < ESTIMATE_ROUNDS; i++) {
		t1 = __rdtsc();
		__asm__ __volatile__ (
			"xor %%ecx, %%ecx;\t\n"
			REP100("rdrand %%ecx;\t\n")
			: "=c" (result)
			: "a" (operand1),
			"b" (operand2)
			:
		);
		t2 = __rdtsc();
		measurements[i] = t2 - t1;
	}
	sort(measurements, ESTIMATE_ROUNDS);
	uint32_t median = measurements[ESTIMATE_ROUNDS / 2];
	printf("Result: 0x%x\n", result);
	printf("Median time: %d\n", median);

	printf("[+] To reset the patch use the -r setting of any program in the tools folder\n");
}
