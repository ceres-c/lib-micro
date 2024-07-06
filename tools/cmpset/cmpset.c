#define _GNU_SOURCE

#include <argp.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

#include "misc.h"
#include "ucode_macro.h"
#include "ldat.h"
#include "patch.h"

#include "ucode_macro.h"

u8 verbose = 0;

const char *argp_program_version = "custom-cpu v0.1";
static char doc[] = "Tool for patching ucode";
static char args_doc[] = "";

// cli argument availble options.
static struct argp_option options[] = {
	{.name="verbose", .key='v', .arg=NULL, .flags=0, .doc="Produce verbose output"},
	{.name="reset", .key='r', .arg=NULL, .flags=0, .doc="reset match & patch"},
	{.name="rdrand", .key='p', .arg=NULL, .flags=0, .doc="patch rdrand"},
	{.name="core", .key='c', .arg="core", .flags=0, .doc="core to patch [0-3]"},
	{0}
};


// define a struct to hold the arguments.
struct arguments{
	u8 verbose;
	u8 reset;
	u8 rdrand;
	s8 core;
};


// define a function which will parse the args.
static error_t parse_opt(int key, char *arg, struct argp_state *state){
	char *token;
	int i;
	struct arguments *arguments = state->input;
	switch(key){

		case 'v':
			arguments->verbose = 1;
			break;
		case 'r':
			arguments->reset = 1;
			break;
		case 'p':
			arguments->rdrand = 1;
			break;
		case 'c':
			arguments->core = strtol(arg, NULL, 0);
			if (arguments->core < 0 || arguments->core > 3){
				argp_usage(state);
				exit(EXIT_FAILURE);
			}
			break;

		default:
			return ARGP_ERR_UNKNOWN;
	}

	return 0;
}

void do_rdrand_patch(void) {
	uint64_t patch_addr = 0x7da0;

	/* Add 1 to rcx if rax != rbx */
	ucode_t ucode_patch[] = {
		{
			SUB_DSZ64_DRR(TMP0, RAX, RBX),	/* tmp0 = rax - rbx. tmp0 now has per-register flags set */
			UJMPCC_DIRECT_NOTTAKEN_CONDNZ_RI(TMP0, patch_addr + 0x04),
			NOP,
			END_SEQWORD
		},
		{ // 0x04
			MOVE_DSZ64_DR(TMP0, RCX),		/* Move current value of rcx to tmp0, because ucode ADD can */
			ADD_DSZ64_DRI(RCX, TMP0, 1),	/* operate on only one architectual register at a time, it seems */
			NOP,
			END_SEQWORD
		},
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

// initialize the argp struct. Which will be used to parse and use the args.
static struct argp argp = {options, parse_opt, args_doc, doc, 0, 0, 0};

int main(int argc, char* argv[]) {
	setbuf(stdin, NULL);
	setbuf(stdout, NULL);
	setbuf(stderr, NULL);

	struct arguments arguments;
	memset(&arguments, 0, sizeof(struct arguments));
	arguments.core = -1;

	argp_parse(&argp, argc, argv, 0, 0, &arguments);
	verbose = arguments.verbose;

	u8 core = (arguments.core < 0)? 0 : arguments.core;
	if (0 <= core && core <= 3) 
		assign_to_core(core);
	else {
		printf("core out of bound");
		exit(EXIT_FAILURE);
	}

	if (arguments.reset) { // Reset match and patch
		init_match_and_patch();
		usleep(20000);
	}

	if (arguments.rdrand) {
		do_fix_IN_patch();
		do_rdrand_patch();
	}

	uint64_t operand1 = 1, operand2 = 1;
	uint64_t rcx_value = 0;

	printf("rdrand rcx [rax: 0x%lx - rbx: 0x%lx]:\n", operand1, operand2);
	// NOTE: This asm uses intel syntax
	__asm__ __volatile__ (
		"xor %%rcx, %%rcx\t\n"
		"mov %%rax, %[op1];\t\n"
		"mov %%rbx, %[op2];\t\n"
		"rdrand rcx;\t\n" // operand2 - operand1
		"rdrand rcx;\t\n" // operand2 - operand1
		"mov %[rcx_value], %%rcx;\t\n"

		: [rcx_value]	"=r" (rcx_value)				// Output operands
		: [op1]			"r" (operand1),					// Input operands
		  [op2]			"r" (operand2)
		: "%rax", // result_a							// Clobbered register
		  "%rbx", // result_b
		  "%rcx"  // scratch
	);
	printf("  rcx: 0x%lx\n", rcx_value);

	operand1 = 0b01, operand2 = 0b11;
	printf("rdrand rcx [rax: 0x%lx - rbx: 0x%lx]:\n", operand1, operand2);
	// NOTE: This asm uses intel syntax
	__asm__ __volatile__ (
		"xor %%rcx, %%rcx\t\n"
		"mov %%rax, %[op1];\t\n"
		"mov %%rbx, %[op2];\t\n"
		"rdrand rcx;\t\n" // operand2 - operand1
		"rdrand rcx;\t\n" // operand2 - operand1
		"mov %[rcx_value], %%rcx;\t\n"

		: [rcx_value]	"=r" (rcx_value)				// Output operands
		: [op1]			"r" (operand1),					// Input operands
		  [op2]			"r" (operand2)
		: "%rax", // result_a							// Clobbered register
		  "%rbx", // result_b
		  "%rcx"  // scratch
	);
	printf("  rcx: 0x%lx\n", rcx_value);

	return 0;
}
