#include "sha256.h"
#include <arm_neon.h>
#include <string.h>

static const uint32_t K[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
    0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
    0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
    0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
    0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
    0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
    0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
    0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
    0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
};

static void sha256_transform_armv8(uint32_t* state, const uint8_t* data) {
    uint32x4_t msg[4];
    msg[0] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(data)));
    msg[1] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(data + 16)));
    msg[2] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(data + 32)));
    msg[3] = vreinterpretq_u32_u8(vrev32q_u8(vld1q_u8(data + 48)));

    uint32x4_t abcd = vld1q_u32(state);
    uint32x4_t efgh = vld1q_u32(state + 4);
    uint32x4_t abcd_save = abcd;
    uint32x4_t efgh_save = efgh;

    for (int i = 0; i < 64; i += 16) {
        uint32x4_t wk, temp_abcd, temp_efgh;

        wk = vaddq_u32(msg[0], vld1q_u32(&K[i]));
        temp_abcd = vsha256hq_u32(abcd, efgh, wk);
        temp_efgh = vsha256h2q_u32(efgh, abcd, wk);
        abcd = temp_abcd; efgh = temp_efgh;

        wk = vaddq_u32(msg[1], vld1q_u32(&K[i + 4]));
        temp_abcd = vsha256hq_u32(abcd, efgh, wk);
        temp_efgh = vsha256h2q_u32(efgh, abcd, wk);
        abcd = temp_abcd; efgh = temp_efgh;

        wk = vaddq_u32(msg[2], vld1q_u32(&K[i + 8]));
        temp_abcd = vsha256hq_u32(abcd, efgh, wk);
        temp_efgh = vsha256h2q_u32(efgh, abcd, wk);
        abcd = temp_abcd; efgh = temp_efgh;

        wk = vaddq_u32(msg[3], vld1q_u32(&K[i + 12]));
        temp_abcd = vsha256hq_u32(abcd, efgh, wk);
        temp_efgh = vsha256h2q_u32(efgh, abcd, wk);
        abcd = temp_abcd; efgh = temp_efgh;

        if (i < 48) {
            uint32x4_t temp_msg0 = vsha256su0q_u32(msg[0], msg[1]);
            temp_msg0 = vsha256su1q_u32(temp_msg0, msg[2], msg[3]);
            msg[0] = temp_msg0;

            uint32x4_t temp_msg1 = vsha256su0q_u32(msg[1], msg[2]);
            temp_msg1 = vsha256su1q_u32(temp_msg1, msg[3], msg[0]);
            msg[1] = temp_msg1;

            uint32x4_t temp_msg2 = vsha256su0q_u32(msg[2], msg[3]);
            temp_msg2 = vsha256su1q_u32(temp_msg2, msg[0], msg[1]);
            msg[2] = temp_msg2;

            uint32x4_t temp_msg3 = vsha256su0q_u32(msg[3], msg[0]);
            temp_msg3 = vsha256su1q_u32(temp_msg3, msg[1], msg[2]);
            msg[3] = temp_msg3;
        }
    }

    abcd = vaddq_u32(abcd, abcd_save);
    efgh = vaddq_u32(efgh, efgh_save);
    vst1q_u32(state, abcd);
    vst1q_u32(state + 4, efgh);
}

void sha256_init(SHA256_CTX *ctx) {
    ctx->datalen = 0;
    ctx->bitlen = 0;
    ctx->state[0] = 0x6a09e667;
    ctx->state[1] = 0xbb67ae85;
    ctx->state[2] = 0x3c6ef372;
    ctx->state[3] = 0xa54ff53a;
    ctx->state[4] = 0x510e527f;
    ctx->state[5] = 0x9b05688c;
    ctx->state[6] = 0x1f83d9ab;
    ctx->state[7] = 0x5be0cd19;
}

void sha256_update(SHA256_CTX *ctx, const uint8_t data[], size_t len) {
    for (size_t i = 0; i < len; ++i) {
        ctx->data[ctx->datalen] = data[i];
        ctx->datalen++;
        if (ctx->datalen == 64) {
            sha256_transform_armv8(ctx->state, ctx->data);
            ctx->bitlen += 512;
            ctx->datalen = 0;
        }
    }
}

void sha256_final(SHA256_CTX *ctx, uint8_t hash[]) {
    uint32_t i = ctx->datalen;
    if (ctx->datalen < 56) {
        ctx->data[i++] = 0x80;
        while (i < 56) ctx->data[i++] = 0x00;
    } else {
        ctx->data[i++] = 0x80;
        while (i < 64) ctx->data[i++] = 0x00;
        sha256_transform_armv8(ctx->state, ctx->data);
        memset(ctx->data, 0, 56);
    }
    ctx->bitlen += ctx->datalen * 8;
    ctx->data[63] = ctx->bitlen;
    ctx->data[62] = ctx->bitlen >> 8;
    ctx->data[61] = ctx->bitlen >> 16;
    ctx->data[60] = ctx->bitlen >> 24;
    ctx->data[59] = ctx->bitlen >> 32;
    ctx->data[58] = ctx->bitlen >> 40;
    ctx->data[57] = ctx->bitlen >> 48;
    ctx->data[56] = ctx->bitlen >> 56;
    
    sha256_transform_armv8(ctx->state, ctx->data);
    
    for (i = 0; i < 4; ++i) {
        hash[i]      = (ctx->state[0] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 4]  = (ctx->state[1] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 8]  = (ctx->state[2] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 12] = (ctx->state[3] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 16] = (ctx->state[4] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 20] = (ctx->state[5] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 24] = (ctx->state[6] >> (24 - i * 8)) & 0x000000ff;
        hash[i + 28] = (ctx->state[7] >> (24 - i * 8)) & 0x000000ff;
    }
}