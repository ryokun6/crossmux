#pragma once

// Host compatibility shim for WeReadClient: firmware expects the mbedtls 3.x
// int-returning sha256 API. Implement it with OpenSSL (already linked by the
// native simulator) so we do not depend on system libmbedtls ABI quirks.

#include <openssl/evp.h>

#include <cstddef>
#include <cstdint>

struct mbedtls_sha256_context {
  EVP_MD_CTX* md = nullptr;
};

inline void mbedtls_sha256_init(mbedtls_sha256_context* ctx) { ctx->md = nullptr; }

inline void mbedtls_sha256_free(mbedtls_sha256_context* ctx) {
  if (ctx->md) {
    EVP_MD_CTX_free(ctx->md);
    ctx->md = nullptr;
  }
}

inline int mbedtls_sha256_starts(mbedtls_sha256_context* ctx, int is224) {
  mbedtls_sha256_free(ctx);
  ctx->md = EVP_MD_CTX_new();
  if (!ctx->md) return -1;
  const EVP_MD* md = is224 ? EVP_sha224() : EVP_sha256();
  if (EVP_DigestInit_ex(ctx->md, md, nullptr) != 1) {
    mbedtls_sha256_free(ctx);
    return -1;
  }
  return 0;
}

inline int mbedtls_sha256_update(mbedtls_sha256_context* ctx, const uint8_t* input, size_t ilen) {
  if (!ctx->md) return -1;
  return EVP_DigestUpdate(ctx->md, input, ilen) == 1 ? 0 : -1;
}

inline int mbedtls_sha256_finish(mbedtls_sha256_context* ctx, uint8_t* output) {
  if (!ctx->md) return -1;
  unsigned int len = 0;
  const int ok = EVP_DigestFinal_ex(ctx->md, output, &len) == 1 ? 0 : -1;
  mbedtls_sha256_free(ctx);
  return ok;
}
