#define _GNU_SOURCE

#include <stdio.h>
#include <stdint.h>
#include <x86intrin.h>
#include "newlib/misc.h"
#include "newlib/ucode_macro.h"
#include "newlib/udbg.h"
#include "newlib/opcode.h"
#include "newlib/match_and_patch_hook.h"

#define ESTIMATE_ROUNDS 2
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

	ucode_t ucode_patch[] = {
		/* rdrand %source; %source := rax == rbx ? 1 : 2 */
		// { /* This works */
		// 	SUB_DSZ64_DRR(TMP0, RAX, RBX),
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x08),
		// 	NOP,
		// 	( SEQ_NOP | SEQ_NEXT | SEQ_SYNCFULL(1) )
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

		// { /* This also works, more compact */
		// 	SUB_DSZ64_DRR(TMP0, RAX, RBX),
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x04),
		// 	ZEROEXT_DSZ64_DI(R64SRC, 1),
		// 	( SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCFULL(1) )
		// }, {
		// 	ZEROEXT_DSZ64_DI(R64SRC, 2),
		// 	NOP,
		// 	NOP,
		// 	END_SEQWORD
		// }

		// /* Loopy mcloopface */
		// {
		// 	MOVEFROMCREG_DSZ64_DI(R12, 0x38c), // Pause frontend
		// 	ZEROEXT_DSZ64_DI(TMP0, 0x000A),
		// 	CONCAT_DSZ16_DRI(TMP0, TMP0, 0xFFFF),	// TMP0 := 0x000AFFFF
		// 	NOP_SEQWORD,
		// }, {
		// 	SUB_DSZ64_DIR(TMP0, 1, TMP0),	// TMP0 := TMP0 - 1
		// 	ADD_DSZ64_DRI(R64SRC, R64SRC, 1),
		// 	ADD_DSZ64_DRI(R64SRC, R64SRC, 1),
		// 	NOP_SEQWORD
		// }, {
		// 	ADD_DSZ64_DRI(R64SRC, R64SRC, 1),
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x04),
		// 	MOVETOCREG_DSZ64_RI(R12, 0x38c), // Restore frontend
		// 	( SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCFULL(1) )
		// 	// If I change to SYNCFULL(2) in order to move the jump one uinstr below,
		// 	// even without moving the jump itself one step down, the cpu just dies lol.
		// 	// Not gonna bother with that.
		// 	// Maybe UEND and SYNC can't be on the same uinstr? Kinda makes sense.
		// }

		// { /* Read URAM test */
		// 	ZEROEXT_DSZ64_DI(TMP0, 0x5555),
		// 	WRITEURAM_RI(TMP0, 0x48),		// Pray nobody uses this address
		// 	NOP,
		// 	NOP_SEQWORD, // Don't sync, maybe it's better
		// 	// ( SEQ_NOP | SEQ_NEXT | SEQ_SYNCFULL(1) )
		// }, {
		// 	ZEROEXT_DSZ64_DI(TMP0, 0x0000),
		// 	CONCAT_DSZ16_DRI(TMP0, TMP0, 0xFFFF),	// TMP0 := 0x0000FFFF
		// 	NOP,
		// 	NOP_SEQWORD,
		// }, {
		// 	READURAM_DI(TMP1, 0x48),
		// 	ADD_DSZ64_DRR(R64SRC, R64SRC, TMP1),
		// 	NOP,
		// 	NOP_SEQWORD,
		// }, {
		// 	SUB_DSZ64_DIR(TMP0, 1, TMP0),	// TMP0 := TMP0 - 1
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x08),
		// 	NOP,
		// 	( SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCFULL(1) )
		// }

		// { /* Read/write URAM test */
		// 	ZEROEXT_DSZ64_DI(TMP0, 0x0000),
		// 	CONCAT_DSZ16_DRI(TMP0, TMP0, 0xFFFF),	// TMP0 := 0x000AFFFF
		// 	NOP,
		// 	NOP_SEQWORD,
		// }, {
		// 	WRITEURAM_RI(TMP0, 0x48),		// Pray nobody uses this address
		// 	READURAM_DI(TMP1, 0x48),
		// 	ADD_DSZ64_DRR(R64SRC, R64SRC, TMP1),
		// 	NOP_SEQWORD,
		// }, {
		// 	SUB_DSZ64_DIR(TMP0, 1, TMP0),	// TMP0 := TMP0 - 1
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x04),
		// 	NOP,
		// 	( SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCFULL(1) )
		// }

		// { /* Read/write URAM test */
		// 	/*
		// 	 * rax:rbx := 0x2AAA7AAAB (0x5555 * 0x7FFFF)
		// 	 * rax = 0x2		(hi)
		// 	 * rbx = 0xAAA7AAAB	(lo)
		// 	 */
		// 	ZEROEXT_DSZ64_DI(TMP0, 0x0007),
		// 	CONCAT_DSZ16_DRI(TMP0, TMP0, 0xFFFF),	// TMP0 := 0x0007FFFF
		// 	ZEROEXT_DSZ64_DI(TMP1, 0x5555),
		// 	NOP_SEQWORD,
		// }, {
		// 	ZEROEXT_DSZ64_DI(TMP3, 0x0000),
		// 	NOP,
		// 	NOP,
		// 	NOP_SEQWORD,
		// }, {
		// 	WRITEURAM_RI(TMP1, 0x48),		// Pray nobody uses this address
		// 	READURAM_DI(TMP2, 0x48),
		// 	ADD_DSZ64_DRR(TMP3, TMP3, TMP2),
		// 	NOP_SEQWORD,
		// }, {
		// 	SUB_DSZ64_DIR(TMP0, 1, TMP0),	// TMP0 := TMP0 - 1
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x08),
		// 	NOP,
		// 	// ( SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCFULL(1) )
		// 	( SEQ_NOP | SEQ_NEXT | SEQ_SYNCFULL(1) )
		// },
		// {
		// 	MOVE_DSZ32_DR(RBX, TMP3),
		// 	SHR_DSZ64_DRI(RAX, TMP3, 32),
		// 	NOP,
		// 	END_SEQWORD
		// }

		// { /* Read/write URAM test */
		//   /* R64SRC := READURAM != 0x5555 ? 0 : 1 */
		// 	ZEROEXT_DSZ64_DI(TMP0, 0x0007),
		// 	CONCAT_DSZ16_DRI(TMP0, TMP0, 0xFFFF),	// TMP0 := 0x0007FFFF
		// 	ZEROEXT_DSZ64_DI(TMP1, 0x5555),
		// 	NOP_SEQWORD,
		// }, { // patch_addr + 0x04
		// 	ZEROEXT_DSZ64_DI(R64SRC, 0x0000),
		// 	ZEROEXT_DSZ64_DI(TMP2, 0x0000),
		// 	WRITEURAM_RI(TMP1, 0x48),		// Pray nobody uses this address
		// 	NOP_SEQWORD,
		// }, { // patch_addr + 0x08
		// 	READURAM_DI(TMP2, 0x48),
		// 	SUB_DSZ16_DRR(TMP3, TMP2, TMP1),
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP3, patch_addr + 0x10),
		// 	( SEQ_NOP | SEQ_NEXT | SEQ_SYNCFULL(2) )
		// }, { // patch_addr + 0x0c
		// 	SUB_DSZ64_DIR(TMP0, 1, TMP0),	// TMP0 := TMP0 - 1
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x08),
		// 	NOP,
		// 	( SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCFULL(1) )
		// }, { // patch_addr + 0x10
		// 	ZEROEXT_DSZ64_DI(R64SRC, 0x0001),
		// 	NOP,
		// 	NOP,
		// 	END_SEQWORD
		// }





		{ /* R64SRC := RAX == RBX ? 2 : 3 */
			/* Note that without SYNCs, the result of upcoming architectural operations can be overwritten in weird ways */
			SUB_DSZ64_DRR(TMP0, RAX, RBX),	/* tmp0 = rax - rbx. tmp0 now has per-register flags set */
			UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, (patch_addr + 0x04)),
			ZEROEXT_DSZ64_DI(R64SRC, 0b10),
			( SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCFULL(1) )	// 1. End on uop 2 (if executed)
															// 2. Otherwise continue to the next triad
															// 3. Force OOOE to wait for the UJUMP to be evaluated,
															// then pick the right branch (both are speculatively
															// executed, but only one is retired, aka committed)
			// SYNCMARK and SYNCWAIT are also an option, but they require one extra ucode triad:
			//	- one to write 0x01 to R64SRC
			//	- one to write 0x02 to R64SRC
			// They both use a seqword like ( SEQ_UEND0(0) | SEQ_NEXT | SEQ_SYNCWAIT(0) )
		}, {
			ZEROEXT_DSZ64_DI(R64SRC, 0b11),
			NOP,
			NOP,
			( SEQ_UEND0(0) | SEQ_NEXT | SEQ_NOSYNC )
		},
		// { /* This is equivalent to the code above, but uses MARK/WAIT syncs */
		// 	SUB_DSZ64_DRR(TMP0, RAX, RBX),	/* tmp0 = rax - rbx. tmp0 now has per-register flags set */
		// 	UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, (patch_addr + 0x08)),
		// 	NOP,
		// 	( SEQ_NOP | SEQ_NEXT | SEQ_SYNCMARK(0) )
		// }, {
		// 	ZEROEXT_DSZ64_DI(R64SRC, 0b10),
		// 	NOP,
		// 	NOP,
		// 	( SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCWAIT(0) )
		// }, {
		// 	ZEROEXT_DSZ64_DI(R64SRC, 0b11),
		// 	NOP,
		// 	NOP,
		// 	( SEQ_UEND0(2) | SEQ_NEXT | SEQ_SYNCWAIT(0) )
		// },
	};

	patch_ucode(patch_addr, ucode_patch, ARRAY_SZ(ucode_patch));
	hook_match_and_patch(0, RDRAND_XLAT, patch_addr);
}

#pragma GCC push_options
#pragma GCC optimize ("O0")
int main(int argc, char* argv[]) {
	uint32_t operand1 = 1, operand2 = 2, result_hi = 0, result_lo = 0;
	uint32_t measurements[ESTIMATE_ROUNDS], t1, t2;

	printf("rax: 0x%08x, rbx: 0x%08x\n", operand1, operand2);

	do_fix_IN_patch();
	do_rdrand_patch();

	for (int i = 0; i < ESTIMATE_ROUNDS; i++) {
		t1 = __rdtsc();
		__asm__ __volatile__ (
			"xor %%ecx, %%ecx;\t\n"
			// REP100("rdrand %%ecx;\t\n")
			// "mfence\t\n"
			// "lfence\t\n"
			"rdrand %%eax;\t\n"
			// "mov %%eax, 0x41;\t\n"
			// "mov %%eax, 0x41;\t\n"
			// "mov %%eax, 0x41;\t\n"
			// "mov %%eax, 0x41;\t\n"
			// "mov %%eax, 0x41;\t\n"
			// "mov %%eax, 0x41;\t\n"
			// "mov %%eax, 0x41;\t\n"
			// "mov %%eax, 0x41;\t\n"
			// "mov %%eax, 0x41;\t\n"
			// "lfence\t\n"
			// "mfence\t\n"
			// : "=a" (result_hi), "=b" (result_lo)
			: "=a" (result_hi)
			: "a" (operand1),
			"b" (operand2)
			:
		);
		t2 = __rdtsc();
		measurements[i] = t2 - t1;
	}

	sort(measurements, ESTIMATE_ROUNDS);
	uint32_t median = measurements[ESTIMATE_ROUNDS / 2];
	// printf("result_hi:result_lo 0x%08x:0x%08x\n", result_hi, result_lo);
	printf("result: 0x%08x\n", result_hi);
	// printf("Median time: %i (0x%x)\n", median, median);
	printf("t2: 0x%08x, t1: 0x%08x\n", t2, t1);

	printf("[+] To reset the patch use the -r setting of any program in the tools folder\n");
}
#pragma GCC pop_options
