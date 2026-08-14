#include "megaaes.h"

#include <stdlib.h>
#include <string.h>

// aes.h / aes.c are compiled into this same target (tiny-aes-mega) at tiny-AES-c's
// default key size (AES-128), with their exported symbols prefixed (AES_init_ctx ->
// mega_AES_init_ctx, AES_ECB_encrypt -> mega_AES_ECB_encrypt) via compile definitions
// so they don't clash with the app's AES-256 tiny-aes instance. We drive CTR from the
// ECB primitive below and keep our own counter, so the mode macros are left at their
// upstream defaults.
#include "aes.h"

struct mega_aes_ctr {
    struct AES_ctx aes;    // AES-128 key schedule (its own Iv field is unused here)
    uint8_t counter[16];   // 128-bit big-endian counter block
    uint8_t keystream[16]; // current keystream block
    size_t ks_offset;      // bytes of keystream already consumed (16 => exhausted)
};

// Big-endian increment of the 16-byte counter, matching OpenSSL EVP_aes_128_ctr().
static void counter_increment(uint8_t counter[16])
{
    for (int i = 15; i >= 0; --i) {
        if (++counter[i] != 0)
            break;
    }
}

struct mega_aes_ctr *mega_aes_ctr_new(const uint8_t key[16], const uint8_t iv[16])
{
    struct mega_aes_ctr *ctx = (struct mega_aes_ctr *)malloc(sizeof(*ctx));
    if (!ctx)
        return NULL;

    AES_init_ctx(&ctx->aes, key);
    memcpy(ctx->counter, iv, 16);
    ctx->ks_offset = 16; // force a fresh keystream block on the first byte
    return ctx;
}

void mega_aes_ctr_xcrypt(struct mega_aes_ctr *ctx, uint8_t *buf, size_t len)
{
    if (!ctx)
        return;

    for (size_t i = 0; i < len; ++i) {
        if (ctx->ks_offset == 16) {
            memcpy(ctx->keystream, ctx->counter, 16);
            AES_ECB_encrypt(&ctx->aes, ctx->keystream);
            counter_increment(ctx->counter);
            ctx->ks_offset = 0;
        }
        buf[i] ^= ctx->keystream[ctx->ks_offset++];
    }
}

void mega_aes_ctr_free(struct mega_aes_ctr *ctx)
{
    free(ctx);
}
