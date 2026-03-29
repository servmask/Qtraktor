# TODOS

## Replace OpenSSL with tiny-AES-c for AES-256-CBC decryption

**What:** Vendor [kokke/tiny-AES-c](https://github.com/kokke/tiny-AES-c) into `vendor/tiny-aes-c/` and replace OpenSSL's EVP API in `cryptoutils.cpp` with it. Remove the OpenSSL dependency entirely.

**Why:** OpenSSL adds ~5MB to the macOS bundle (`libcrypto.3.dylib` + `libssl.3.dylib`) and is an external dependency on all platforms. Traktor uses OpenSSL for exactly one thing: AES-256-CBC decryption of encrypted .wpress archives (~40 lines in `cryptoutils.cpp:241-315`). tiny-AES-c is a pure C, zero-dependency, well-tested AES implementation that handles this with ~60 lines of wrapper code and no external dependencies.

**How:**
1. Vendor `aes.h` + `aes.c` from tiny-AES-c into `vendor/tiny-aes-c/`
2. Add `#define CBC 1` and `#define AES256 1` build flags
3. Replace `decryptString()` in `cryptoutils.cpp` with tiny-AES-c calls (`AES_init_ctx_iv` + `AES_CBC_decrypt_buffer`)
4. Implement PKCS7 padding removal (~15 lines, tiny-AES-c doesn't handle padding)
5. Remove `find_package(OpenSSL)` from CMakeLists.txt and tests/CMakeLists.txt
6. Remove `libcrypto`/`libssl` from macOS bundle stripping in ci.yml and release.yml
7. Remove vcpkg OpenSSL install from Windows CI (only zlib needed)
8. Update CLAUDE.md build instructions (no more OpenSSL dependency)

**Depends on:** Qt 6 + CMake migration PR merged first.
