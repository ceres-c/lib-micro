#ifndef UCODE_MACRO_H_
#define UCODE_MACRO_H_

#include "opcode.h"
#include "xlat.h"

#define MOD0 (1UL << 23)
#define MOD1 (1UL << 44)
#define MOD2 (1UL << 45)

__attribute__((always_inline))
static inline unsigned long long parity0(unsigned long long value) {
    unsigned long long parity = 0;
    while (value) {
        parity ^= (value & 1);
        value = value >> 2;
    }
    return parity;
}

__attribute__((always_inline))
static inline unsigned long long parity1(unsigned long long value) {
    unsigned long long parity = 0;
    value = value >> 1;
    while (value) {
        parity ^= (value & 1);
        value = value >> 2;
    }
    return parity;
}

// generic stuff

#define CRC_UOP_MASK 0x3FFFFFFFFFFFUL
#define CRC_SEQ_MASK 0xFFFFFFFUL

#define IMM_ENCODE_SRC0(src0_id) \
    (((src0_id) & 0xffUL) << 24) | (((src0_id) & 0x1f00)<< 10) | (((src0_id) & 0xe000) >> 13) | (1 << 3)

#define IMM_ENCODE_SRC1(src1_id) \
    (((src1_id) & 0xffUL) << 24) | (((src1_id) & 0x1f00)<< 10) | (((src1_id) & 0xe000) >> 7) | (1 << 9)

#define IMM0_ENCODE(imm) \
    (((imm)&0xffUL) << 24)

#define IMM1_ENCODE(imm) \
    (((imm)&0x1fUL) << 18) 

#define SRC0_ENCODE(val) \
    (((val) & 0x3f) << 0)

#define SRC1_ENCODE(val) \
    (((val) & 0x3f) << 6)

#define DST_ENCODE(val) \
    (((val) & 0x3f) << 12)

#define CRC_SEQ(seq) \
    (( (seq) & CRC_SEQ_MASK) | ((parity0((seq)&CRC_SEQ_MASK) << 28) | (parity1((seq)&CRC_SEQ_MASK) << 29)))

#define CRC_UOP(uop) \
    (( (uop) & CRC_UOP_MASK) | (parity0((uop)&CRC_UOP_MASK) << 46) | (parity1((uop)&CRC_UOP_MASK) << 47))

#define INSTR_I0(imm) ( IMM_ENCODE_SRC0(imm) )
#define INSTR_I1(imm) ( IMM_ENCODE_SRC1(imm) )
#define INSTR_R0(src) ( SRC0_ENCODE(src) )
#define INSTR_R1(src) ( SRC1_ENCODE(src) )
#define INSTR_M0(macro) ( IMM_ENCODE_SRC0(macro) | MOD0 )
#define INSTR_M1(macro) ( IMM_ENCODE_SRC1(macro) | MOD0 )
#define INSTR_DI0(dst, imm) ( DST_ENCODE(dst) | IMM_ENCODE_SRC0(imm) )
#define INSTR_DI1(dst, imm) ( DST_ENCODE(dst) | IMM_ENCODE_SRC1(imm) )
#define INSTR_DR0(dst, src) ( DST_ENCODE(dst) | SRC0_ENCODE(src) )
#define INSTR_DR1(dst, src) ( DST_ENCODE(dst) | SRC1_ENCODE(src) )
#define INSTR_DM0(dst, macro) ( DST_ENCODE(dst) | IMM_ENCODE_SRC0(macro) | MOD0 )
#define INSTR_DM1(dst, macro) ( DST_ENCODE(dst) | IMM_ENCODE_SRC1(macro) | MOD0 )
#define INSTR_RR(src0, src1) ( SRC0_ENCODE(src0) | SRC1_ENCODE(src1) )
#define INSTR_RI(src, imm) ( SRC0_ENCODE(src) | IMM_ENCODE_SRC1(imm) )
#define INSTR_IR(imm, src) ( IMM_ENCODE_SRC0(imm) | SRC1_ENCODE(src) )
#define INSTR_RM(src, macro) ( SRC0_ENCODE(src) | IMM_ENCODE_SRC1(macro) | MOD0 )
#define INSTR_MR(macro, src) ( IMM_ENCODE_SRC0(macro) | SRC1_ENCODE(src) | MOD0 )
#define INSTR_DRR(dst, src0, src1) ( DST_ENCODE(dst) | SRC0_ENCODE(src0) | SRC1_ENCODE(src1) )
#define INSTR_DRI(dst, src, imm) ( DST_ENCODE(dst) | SRC0_ENCODE(src) | IMM_ENCODE_SRC1(imm) )
#define INSTR_DIR(dst, imm, src) ( DST_ENCODE(dst) | IMM_ENCODE_SRC0(imm) | SRC1_ENCODE(src) )
#define INSTR_DRM(dst, src, macro) ( DST_ENCODE(dst) | SRC0_ENCODE(src) | IMM_ENCODE_SRC1(macro) | MOD0 )
#define INSTR_DMR(dst, macro, src) ( DST_ENCODE(dst) | IMM_ENCODE_SRC0(macro) | SRC1_ENCODE(src) | MOD0 )

#define MEMOP_ENCODE(dst, base_reg, index_reg, offset, seg) \
    ( DST_ENCODE(dst) | SRC0_ENCODE(base_reg) | SRC1_ENCODE(index_reg) | IMM0_ENCODE(offset) | IMM1_ENCODE(seg) )

#include "inst.h"

// segment fields
#define RDSEGFLD(dst, seg, fld) \
    ( _RDSEGFLD | DST_ENCODE(dst) | IMM0_ENCODE((fld)<<4) | IMM1_ENCODE(seg) )

#define WRSEGFLD(src, seg, fld) \
    ( _WRSEGFLD | SRC0_ENCODE(src) | IMM0_ENCODE(((fld)<<4) | ((seg)&0x1f)) )

// stagingbuf to reg

/** \defgroup LDSTGBUF
 *  @{
 */
#define LDSTGBUF_DSZ64_ASZ16_SC1_DI(dst, addr_imm) \
    ( _LDSTGBUF_DSZ64_ASZ16_SC1 | DST_ENCODE(dst) | IMM_ENCODE_SRC0(addr_imm) | MOD2 )

#define LDSTGBUF_DSZ64_ASZ16_SC1_DR(dst, addr_reg) \
    ( _LDSTGBUF_DSZ64_ASZ16_SC1 | DST_ENCODE(dst) | SRC0_ENCODE(addr_reg) | MOD2 )
/** @} */

/** \defgroup STADSTGBUF
 *  @{
 */
#define STADSTGBUF_DSZ64_ASZ16_SC1_RI(src, addr_imm) \
    ( _STADSTGBUF_DSZ64_ASZ16_SC1 | DST_ENCODE(src) | IMM_ENCODE_SRC0(addr_imm) | MOD2 )

#define STADSTGBUF_DSZ64_ASZ16_SC1_RR(src, addr_reg) \
    ( _STADSTGBUF_DSZ64_ASZ16_SC1 | DST_ENCODE(src) | SRC0_ENCODE(addr_reg) | MOD2 )
/** @} */

#define SFENCE _SFENCE

/** \defgroup READUIP_REGOVR
 *  @{
 */
#define READUIP_REGOVR0(dst) \
    ( _READUIP_REGOVR | DST_ENCODE(dst) | SRC0_ENCODE(0x10) )

#define READUIP_REGOVR1(dst) \
    ( _READUIP_REGOVR | DST_ENCODE(dst) | SRC0_ENCODE(0x10) | MOD0 )
/** @} */

/** \defgroup SAVEUIP
 *  @{
 */
#define SAVEUIP0_DI(dst, addr)                                  \
    ( _SAVEUIP | DST_ENCODE(dst) | IMM_ENCODE_SRC1( (addr) ) | IMM_ENCODE_SRC0(0) )

#define SAVEUIP1_DI(dst, addr)                                  \
    ( _SAVEUIP | DST_ENCODE(dst) | IMM_ENCODE_SRC1( (addr) ) | IMM_ENCODE_SRC0(0) | MOD0 )

#define SAVEUIP0_I(addr) SAVEUIP0_DI(0, addr)
#define SAVEUIP1_I(addr) SAVEUIP1_DI(0, addr)
/** @} */

/** \defgroup SAVEUIP_REGOVR
 *  @{
 */
#define SAVEUIP_REGOVR0(imm) \
    ( _SAVEUIP_REGOVR | IMM_ENCODE_SRC1(imm) )
#define SAVEUIP_REGOVR1(imm) \
    ( _SAVEUIP_REGOVR | IMM_ENCODE_SRC1(imm) | MOD0 )
/** @} */

/** \defgroup UPDATEUSTATE
 *  @{
 */
#define UPDATEUSTATE_I(testbits)        \
    ( _UPDATEUSTATE | IMM_ENCODE_SRC1(testbits) )

#define UPDATEUSTATE_NOT_I(testbits)        \
    ( _UPDATEUSTATE | IMM_ENCODE_SRC1(testbits) | MOD0 )

#define UPDATEUSTATE_RI(src, testbits)        \
    ( _UPDATEUSTATE | SRC0_ENCODE(src) | IMM_ENCODE_SRC1(testbits) )

#define UPDATEUSTATE_NOT_RI(src, testbits)        \
    ( _UPDATEUSTATE | SRC0_ENCODE(src) | IMM_ENCODE_SRC1(testbits) | MOD0 )
/** @} */

/** \defgroup TESTUSTATE
 *  @{
 */
#define TESTUSTATE_UCODE(testbits)        \
    ( _TESTUSTATE | IMM_ENCODE_SRC1(testbits) )

#define TESTUSTATE_SYS(testbits)        \
    ( _TESTUSTATE | IMM_ENCODE_SRC1(testbits) | MOD1 )

#define TESTUSTATE_VMX(testbits)        \
    ( _TESTUSTATE | IMM_ENCODE_SRC1(testbits) | MOD1 | MOD2 )

#define TESTUSTATE_UCODE_NOT(testbits)        \
    ( _TESTUSTATE | IMM_ENCODE_SRC1(testbits) | MOD0 )

#define TESTUSTATE_SYS_NOT(testbits)        \
    ( _TESTUSTATE | IMM_ENCODE_SRC1(testbits) | MOD1 | MOD0 )

#define TESTUSTATE_VMX_NOT(testbits)        \
    ( _TESTUSTATE | IMM_ENCODE_SRC1(testbits) | MOD1 | MOD2 | MOD0 )
/** @} */

/** \defgroup URET
 *  @{
 */
#define URET0 \
    ( _URET )

#define URET1 \
    ( _URET | MOD0 )
/** @} */

#define UNK256 ( (0x256UL << 32) | MOD1 )

/** \defgroup UFLOWCTRL
 *  @{
 */
#define FLOW_CTRL_UNK       0x01
#define FLOW_CTRL_URET0     0x0a
#define FLOW_CTRL_URET1     0x0b
#define FLOW_CTRL_LDAT_IN   0x0d
#define FLOW_CTRL_MSLOOPCTR 0x0e
#define FLOW_CTRL_USTATE    0x0f

#define UFLOWCTRL_DR(dst, reg, uop) \
    ( _UFLOWCTRL | DST_ENCODE(dst) | (((uop)&0xff)<<24) | SRC1_ENCODE(reg) )

#define UFLOWCTRL_R(reg, uop) \
    ( UFLOWCTRL_DR(0, reg, uop) )
/** @} */

/** \defgroup AETTRACE
 *  @{
 */
#define AETTRACE_DCR(dst, val, src) \
    ( _AETTRACE | DST_ENCODE(dst) | (((val)&0x1f)<<18) | SRC1_ENCODE(src) )

#define AETTRACE_DCM(dst, val, macro) \
    ( _AETTRACE | DST_ENCODE(dst) | (((val)&0x1f)<<18) | IMM_ENCODE_SRC1(macro) | MOD0 )
/** @} */

#include "seqword.h"

// helpers
#define PUSHREG(src) \
    SUB_DSZ64_DIR(RSP, 0x8, RSP), MOVE_DSZ64_DR(TMP0, src), STAD_DSZ64_ASZ32_SC1_RR(TMP0, RSP, 0x3, 0x1a)

#define PUSHCREG(creg) \
    SUB_DSZ64_DIR(RSP, 0x8, RSP), MOVEFROMCREG_DSZ64_DI(TMP0, creg), STAD_DSZ64_ASZ32_SC1_RR(TMP0, RSP, 0x3, 0x1a)

#define POPREG(dst) \
    LDZX_DSZ64_ASZ32_SC1_DR(TMP0, RSP, 0x3, 0x1a), MOVE_DSZ64_DR(dst, TMP0), ADD_DSZ64_DRI(RSP, RSP, 0x8)

#define POPCREG(creg) \
    LDZX_DSZ64_ASZ32_SC1_DR(TMP0, RSP, 0x3, 0x1a), MOVETOCREG_DSZ64_DI(TMP0, creg), ADD_DSZ64_DRI(RSP, RSP, 0x8)

//see section
#define PINTMOVQI2XMMLQ_DSZ64(xmm_dst, src) \
    ( _PINTMOVQI2XMMLQ_DSZ64 | DST_ENCODE(xmm_dst) | SRC0_ENCODE(src) | MOD2 )

#define PINTMOVQXMMLQ2I_DSZ64(dst, xmm_src) \
    ( _PINTMOVQXMMLQ2I_DSZ64 | DST_ENCODE(dst) | SRC0_ENCODE(xmm_src) | MOD2 )

#define MOVUSS(dst, src) \
    _MOVUSS | DST_ENCODE(dst) | SRC0_ENCODE(src)

#define SHUFSS(dst, src) \
    _SHUFSS | DST_ENCODE(dst) | SRC0_ENCODE(src)

#define MOVUSD(dst, src) \
    _MOVUSD | DST_ENCODE(dst) | SRC0_ENCODE(src)

#define SHUFSD(dst, src) \
    _SHUFSD | DST_ENCODE(dst) | SRC0_ENCODE(src)

#define MOVUPD(dst, src) \
    _MOVUPD | DST_ENCODE(dst) | SRC0_ENCODE(src)

#define SHUFPD(dst, src) \
    _SHUFPD | DST_ENCODE(dst) | SRC0_ENCODE(src)

#define MOVUPS(dst, src) \
    _MOVUPS | DST_ENCODE(dst) | SRC0_ENCODE(src)

#define SHUFPS(dst, src) \
    _SHUFPS | DST_ENCODE(dst) | SRC0_ENCODE(src)

#define MULSS(dst, src, src2) \
    _MULSS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define SQRTSS(dst, src, src2) \
    _SQRTSS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define DIVSS(dst, src, src2) \
    _DIVSS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define ADDSS(dst, src, src2) \
    _ADDSS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define SUBSS(dst, src, src2) \
    _SUBSS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MINSS(dst, src, src2) \
    _MINSS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define CMPSS(dst, src, src2) \
    _CMPSS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MAXSS(dst, src, src2) \
    _MAXSS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define SQRTSD(dst, src, src2) \
    _SQRTSD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define DIVSD(dst, src, src2) \
    _DIVSD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define ADDSD(dst, src, src2) \
    _ADDSD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define SUBSD(dst, src, src2) \
    _SUBSD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MINSD(dst, src, src2) \
    _MINSD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define CMPSD(dst, src, src2) \
    _CMPSD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MAXSD(dst, src, src2) \
    _MAXSD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MULPD(dst, src, src2) \
    _MULPD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define SQRTPD(dst, src, src2) \
    _SQRTPD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define DIVPD(dst, src, src2) \
    _DIVPD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define ADDPD(dst, src, src2) \
    _MADDPD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define SUBPD(dst, src, src2) \
    _SUBPD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MINPD(dst, src, src2) \
    _MINPD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define CMPPD(dst, src, src2) \
    _CMPPD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MAXPD(dst, src, src2) \
    _MAXPD | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MULPS(dst, src, src2) \
    _MULPS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define SQRTPS(dst, src, src2) \
    _SQRTPS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define DIVPS(dst, src, src2) \
    _DIVPS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define ADDPS(dst, src, src2) \
    _ADDPS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define SUBPS(dst, src, src2) \
    _SUBPS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MINPS(dst, src, src2) \
    _MINPS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define CMPPS(dst, src, src2) \
    _CMPPS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#define MAXPS(dst, src, src2) \
    _MAXPS | DST_ENCODE(dst) | SRC0_ENCODE(src) | SRC1_ENCODE(src2)

#endif // UCODE_MACRO_H_
