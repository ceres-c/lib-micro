#ifndef UCODE_MACRO_H_
#define UCODE_MACRO_H_

#include "xlat.h"
#include "seqword.h"

#define CRC_UOP_MASK 0x3FFFFFFFFFFFULL
#define CRC_SEQ_MASK 0xFFFFFFFULL

__attribute__((always_inline))
static inline uint64_t parity0(uint64_t value) {
    uint64_t parity = 0;
    while (value) {
        parity ^= (value & 1);
        value = value >> 2;
    }
    return parity;
}

__attribute__((always_inline))
static inline uint64_t parity1(uint64_t value) {
    uint64_t parity = 0;
    value = value >> 1;
    while (value) {
        parity ^= (value & 1);
        value = value >> 2;
    }
    return parity;
}

#define CRC_UOP(uop) \
    (( (uop) & CRC_UOP_MASK) | (parity0((uop)&CRC_UOP_MASK) << 46) | (parity1((uop)&CRC_UOP_MASK) << 47))

#define CRC_SEQ(seq) \
    (( (seq) & CRC_SEQ_MASK) | ((parity0((seq)&CRC_SEQ_MASK) << 28) | (parity1((seq)&CRC_SEQ_MASK) << 29)))

#endif // UCODE_MACRO_H_
