#ifndef LF2_RNG_H
#define LF2_RNG_H

#include <stddef.h>
#include <stdint.h>

/* LF2 2.00a deterministic network RNG.

   Reverse engineering of the original executable (function 0x00417170)
   shows that network/random gameplay does not call the CRT rand() directly.
   It advances two counters and combines them with the shared 3000-byte table
   exchanged by the stock TCP handshake:

     i = (i + 1) % 1234
     j = (j + 1) % 3000
     result = (i + table[j]) % range

   The wire block is 3001 bytes; byte 3000 is a trailing byte and is not read
   by the original random function.  When stock mode is disabled these helpers
   deliberately fall back to libc rand()%range so ordinary LF2Vita modes keep
   their existing behaviour. */

void lf2_rng_stock_enable(const uint8_t *wire_table, size_t wire_len);
void lf2_rng_stock_disable(void);
void lf2_rng_stock_reset(uint32_t i, uint32_t j);
int lf2_rng_stock_enabled(void);
uint32_t lf2_rng_stock_i(void);
uint32_t lf2_rng_stock_j(void);
uint32_t lf2_rng_calls(void);

int lf2_rng_mod(int range);
int lf2_rng_bit(void);

#endif
