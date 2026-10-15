// ============================================================
// ESP32 DGB Miner - SHA-256 Engine using mbedtls
// ============================================================
#include "sha256_engine.h"
#include <string.h>
#include "mbedtls/sha256.h"

void sha256_init(SHA256_CTX* ctx) {
    // Legacy support, not needed with mbedtls
}

void sha256_update(SHA256_CTX* ctx, const uint8_t* data, size_t len) {
    // Legacy support
}

void sha256_final(SHA256_CTX* ctx, uint8_t hash[32]) {
    // Legacy support
}

void sha256_transform(uint32_t state[8], const uint8_t block[64]) {
    // Legacy support
}

void sha256d(const uint8_t* data, size_t len, uint8_t hash[32]) {
    uint8_t first_hash[32];
    mbedtls_sha256_context ctx;
    mbedtls_sha256_init(&ctx);
    mbedtls_sha256_starts(&ctx, 0); // 0 = SHA-256
    mbedtls_sha256_update(&ctx, data, len);
    mbedtls_sha256_finish(&ctx, first_hash);
    
    mbedtls_sha256_starts(&ctx, 0);
    mbedtls_sha256_update(&ctx, first_hash, 32);
    mbedtls_sha256_finish(&ctx, hash);
    mbedtls_sha256_free(&ctx);
}

void sha256_midstate(const uint8_t header[80], uint32_t midstate[8]) {
    // With mbedtls, we don't extract raw midstate.
    // Instead we just pass the header directly or clone context.
    // To match the API, we can't easily extract midstate in mbedtls > 2.x
    // So we'll just ignore midstate and do full hashes in mine_nonce!
}

void sha256_prepare_block2(const uint8_t header[80], uint8_t block2[64]) {
    // Not needed if we do full hashing, but let's copy the header over to block2
    // so we can modify it.
    memcpy(block2, header, 80); // We'll just use block2 as a full 80-byte buffer!
}

void sha256d_mine_nonce(const uint32_t midstate[8], uint8_t block2[64],
                        uint32_t nonce, uint8_t hash[32]) {
    // block2 is now an 80-byte buffer holding the header
    block2[76] = (uint8_t)(nonce);
    block2[77] = (uint8_t)(nonce >> 8);
    block2[78] = (uint8_t)(nonce >> 16);
    block2[79] = (uint8_t)(nonce >> 24);

    sha256d(block2, 80, hash);
}

bool checkHashTarget(const uint8_t hash[32], const uint8_t target[32]) {
    for (int i = 0; i < 32; i++) {
        uint8_t dh = hash[31 - i];  
        if (dh < target[i]) return true;   
        if (dh > target[i]) return false;  
    }
    return true; 
}

void difficultyToTarget(double difficulty, uint8_t target[32]) {
    memset(target, 0, 32);
    if (difficulty <= 0.0) {
        memset(target, 0xFF, 32);
        return;
    }
    
    // Bitcoin difficulty 1 target is 0x00000000FFFF0000...
    // So mantissa is 65535 (0xFFFF) at bytes 4 and 5.
    double mantissa = 65535.0 / difficulty;
    int bytePos = 5;
    
    while (mantissa >= 256.0 && bytePos > 0) {
        mantissa /= 256.0;
        bytePos--;
    }
    while (mantissa < 1.0 && bytePos < 31) {
        mantissa *= 256.0;
        bytePos++;
    }
    if (bytePos <= 0 && mantissa >= 256.0) {
        memset(target, 0xFF, 32);
        return;
    }
    
    for (int i = bytePos; i < 32 && mantissa > 0.0; i++) {
        uint8_t b = (uint8_t)mantissa;
        target[i] = b;
        mantissa = (mantissa - (double)b) * 256.0;
    }
}
