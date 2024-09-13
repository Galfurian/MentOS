/// @file sha256.c
/// @author Enrico Fraccaroli (enry.frak@gmail.com)
/// @brief Implementation of the SHA-256 hashing algorithm.
/// @details The original code was written by Brad Conte, and is available at:
///     https://github.com/B-Con/crypto-algorithms
///
/// SHA-256 is one of the three algorithms in the SHA2
/// specification. The others, SHA-384 and SHA-512, are not
/// offered in this implementation.
/// Algorithm specification can be found here:
///     http://csrc.nist.gov/publications/fips/fips180-2/fips180-2withchangenotice.pdf
/// This implementation uses little endian byte order.

#pragma once

#include <stdint.h>
#include <stddef.h>

/// @brief SHA256 outputs a 32 byte digest.
#define SHA256_BLOCK_SIZE 32

/// @brief Keeps track of the context required to run the hashing algorithm.
typedef struct {
    uint8_t data[64];          ///< The data buffer.
    uint32_t datalen;          ///< The actual amount of data inside the buffer.
    unsigned long long bitlen; ///< Multiples of 512.
    uint32_t state[8];         ///< Contains the state for generating the output hash.
} SHA256_ctx_t;

/// @brief Initializes the context.
/// @param ctx pointer to the context to initialize.
void sha256_init(SHA256_ctx_t *ctx);

/// @brief Updates the context based on the data.
/// @param ctx the context to update.
/// @param data the data used for generate the hash.
/// @param len length of the data.
void sha256_update(SHA256_ctx_t *ctx, const uint8_t data[], size_t len);

/// @brief Given an initialized and updated context, it produces the final hash.
/// @param ctx the context.
/// @param hash where the final has is stored.
void sha256_final(SHA256_ctx_t *ctx, uint8_t hash[]);

/// @brief Support function that transforms an array of bytes into hex string.
/// @param src the stream of bytes.
/// @param src_length the length of the stream.
/// @param out the string where the hex is stored.
/// @param out_length the length of the string buffer.
void sha256_bytes_to_hex(uint8_t *src, size_t src_length, char *out, size_t out_length);
