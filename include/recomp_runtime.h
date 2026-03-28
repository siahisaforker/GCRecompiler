#pragma once

#include <stdint.h>
#include <string.h>

typedef uint8_t  u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int8_t   s8;
typedef int16_t  s16;
typedef int32_t  s32;
typedef int64_t  s64;
typedef float    f32;
typedef double   f64;

typedef struct {
    u32 gpr[32];
    f64 fpr[32];
    u32 lr;
    u32 ctr;
    u32 pc;
    u32 cr;
    u32 xer;
    u32 fpscr;
    // Gekko-specific
    f64 ps0[32];
    f64 ps1[32];
} CPUContext;

extern u8* ram;

// Basic memory helpers (Big Endian to Little Endian if needed, but for now we assume same or handle in macro)
static inline u32 bswap32(u32 x) {
    return ((x & 0xFF000000) >> 24) | ((x & 0x00FF0000) >> 8) | ((x & 0x0000FF00) << 8) | ((x & 0x000000FF) << 24);
}

// For now, assume simple mapping or big-endian ram
#define MEM_READ8(addr)  (ram[(addr) & 0x01FFFFFF])
#define MEM_READ16(addr) (*(u16*)&ram[(addr) & 0x01FFFFFF])
#define MEM_READ32(addr) (*(u32*)&ram[(addr) & 0x01FFFFFF])
#define MEM_READ_FLOAT(addr) (*(f32*)&ram[(addr) & 0x01FFFFFF])

#define MEM_WRITE8(addr, val)  (ram[(addr) & 0x01FFFFFF] = (val))
#define MEM_WRITE16(addr, val) (*(u16*)&ram[(addr) & 0x01FFFFFF] = (val))
#define MEM_WRITE32(addr, val) (*(u32*)&ram[(addr) & 0x01FFFFFF] = (val))
#define MEM_READ_DOUBLE(addr) (*(f64*)&ram[(addr) & 0x01FFFFFF])
#define MEM_WRITE_DOUBLE(addr, val) (*(f64*)&ram[(addr) & 0x01FFFFFF] = (val))

// PowerPC Helpers
static inline u32 rotl32(u32 val, u32 sh) {
    sh &= 31;
    return (val << sh) | (val >> (32 - sh));
}

static inline u32 get_mask(u32 mb, u32 me) {
    u32 mask = 0xFFFFFFFF;
    if (mb <= me) {
        mask = (mask >> mb) & (mask << (31 - me));
    } else {
        mask = (mask >> mb) | (mask << (31 - me));
    }
    return mask;
}

#define RLWIMI(d, s, sh, mb, me) (((d) & ~get_mask(mb, me)) | (rotl32(s, sh) & get_mask(mb, me)))

static inline u32 ADDIC(CPUContext* ctx, u32 a, u32 b) {
    u32 res = a + b;
    if (res < a) ctx->xer |= 0x20000000; // CA bit
    else ctx->xer &= ~0x20000000;
    return res;
}

static inline void LMW(CPUContext* ctx, u32 r, u32 ra, u32 simm) {
    u32 addr = ctx->gpr[ra] + simm;
    for (int i = r; i < 32; i++) {
        ctx->gpr[i] = MEM_READ32(addr);
        addr += 4;
    }
}

static inline void STMW(CPUContext* ctx, u32 r, u32 ra, u32 simm) {
    u32 addr = ctx->gpr[ra] + simm;
    for (int i = r; i < 32; i++) {
        MEM_WRITE32(addr, ctx->gpr[i]);
        addr += 4;
    }
}

static inline void set_cr_field(CPUContext* ctx, int f, u32 a, u32 b, int is_signed) {
    u32 res = 0;
    if (is_signed) {
        if ((s32)a < (s32)b) res = 8;
        else if ((s32)a > (s32)b) res = 4;
        else res = 2;
    } else {
        if (a < b) res = 8;
        else if (a > b) res = 4;
        else res = 2;
    }
    ctx->cr = (ctx->cr & ~(0xF << (28 - f * 4))) | (res << (28 - f * 4));
}

static inline int CHECK_COND(CPUContext* ctx, u32 bo, u32 bi) {
    int cond = (ctx->cr >> (31 - bi)) & 1;
    int ok = 0;
    if (!(bo & 0x10)) {
        ctx->ctr--;
        int ctr_ok = (ctx->ctr != 0);
        if (bo & 0x02) ctr_ok = !ctr_ok;
        if (!(bo & 0x08)) ok = ctr_ok && (cond == ((bo >> 3) & 1));
        else ok = ctr_ok;
    } else {
        if (!(bo & 0x08)) ok = (cond == ((bo >> 3) & 1));
        else ok = 1;
    }
    return ok;
}

static inline u32 cntlzw(u32 x) {
    if (x == 0) return 32;
    u32 n = 0;
    if (x <= 0x0000FFFF) { n += 16; x <<= 16; }
    if (x <= 0x00FFFFFF) { n += 8; x <<= 8; }
    if (x <= 0x0FFFFFFF) { n += 4; x <<= 4; }
    if (x <= 0x3FFFFFFF) { n += 2; x <<= 2; }
    if (x <= 0x7FFFFFFF) { n += 1; }
    return n;
}

static inline u32 get_spr(CPUContext* ctx, u32 spr) {
    if (spr == 8) return ctx->lr;
    if (spr == 9) return ctx->ctr;
    if (spr == 1) return ctx->xer;
    return 0;
}

static inline void set_spr(CPUContext* ctx, u32 spr, u32 val) {
    if (spr == 8) ctx->lr = val;
    else if (spr == 9) ctx->ctr = val;
    else if (spr == 1) ctx->xer = val;
}

#define EXTSB(x) ((u32)(s32)(s8)(x))
#define EXTSH(x) ((u32)(s32)(s16)(x))
#define FSEL(a, b, c) ((a) >= 0.0 ? (b) : (c))
#define FRSP(x) ((f32)(x))
