#include "lf2_rng.h"

#include <stdlib.h>
#include <string.h>

#define LF2_STOCK_RNG_TABLE_LEN 3000u

static uint8_t g_table[LF2_STOCK_RNG_TABLE_LEN];
static uint32_t g_i;
static uint32_t g_j;
static uint32_t g_calls;
static int g_enabled;

void lf2_rng_stock_enable(const uint8_t *wire_table, size_t wire_len){
    if(!wire_table || wire_len < LF2_STOCK_RNG_TABLE_LEN){
        lf2_rng_stock_disable();
        return;
    }
    memcpy(g_table, wire_table, LF2_STOCK_RNG_TABLE_LEN);
    /* The two original globals are BSS-backed in the stock executable and
       therefore begin at zero.  Keep reset separate so a future trace can
       override this without changing the call sites. */
    g_i=0;
    g_j=0;
    g_calls=0;
    g_enabled=1;
}

void lf2_rng_stock_disable(void){
    memset(g_table,0,sizeof(g_table));
    g_i=0;
    g_j=0;
    g_calls=0;
    g_enabled=0;
}

void lf2_rng_stock_reset(uint32_t i,uint32_t j){
    g_i=i%1234u;
    g_j=j%3000u;
    g_calls=0;
}

int lf2_rng_stock_enabled(void){return g_enabled;}
uint32_t lf2_rng_stock_i(void){return g_i;}
uint32_t lf2_rng_stock_j(void){return g_j;}
uint32_t lf2_rng_calls(void){return g_calls;}

int lf2_rng_mod(int range){
    if(range<=0)return 0;
    if(!g_enabled)return rand()%range;
    g_i=(g_i+1u)%1234u;
    g_j=(g_j+1u)%3000u;
    g_calls++;
    return (int)((g_i+(uint32_t)g_table[g_j])%(uint32_t)range);
}

int lf2_rng_bit(void){return lf2_rng_mod(2);}
