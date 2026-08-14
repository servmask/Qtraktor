#ifndef MEGAAES_H
#define MEGAAES_H

// Streaming AES-128-CTR for Mega downloads, backed by vendored tiny-AES-c.
//
// Mega file keys are 128-bit and the payload is encrypted in CTR mode. tiny-AES-c
// bakes its key size in at compile time, and the rest of the app compiles it as
// AES-256 (for .wpress AES-256-CBC), so this AES-128 instance lives in a separate
// translation unit (the `tiny-aes-mega` CMake target, with prefixed symbols) and
// is reached only through this opaque C API. The key never leaves the client.
//
// CTR is implemented here on top of tiny-AES-c's single-block ECB primitive so the
// keystream stays correctly aligned across arbitrary-sized network chunks (tiny-AES-c's
// own AES_CTR_xcrypt_buffer restarts the keystream block on every call). The 128-bit
// counter is incremented big-endian to match the previous OpenSSL EVP_aes_128_ctr().

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct mega_aes_ctr; // opaque

// Create a CTR context. key is 16 bytes; iv is the 16-byte initial counter block
// (Mega uses the 8-byte nonce followed by 8 zero bytes). Returns NULL on allocation
// failure. Free with mega_aes_ctr_free().
struct mega_aes_ctr *mega_aes_ctr_new(const uint8_t key[16], const uint8_t iv[16]);

// Encrypt/decrypt len bytes of buf in place, continuing the keystream from the
// previous call. Safe to call with any len, including non-multiples of 16.
void mega_aes_ctr_xcrypt(struct mega_aes_ctr *ctx, uint8_t *buf, size_t len);

// Free a context created by mega_aes_ctr_new(). NULL is a no-op.
void mega_aes_ctr_free(struct mega_aes_ctr *ctx);

#ifdef __cplusplus
}
#endif

#endif // MEGAAES_H
