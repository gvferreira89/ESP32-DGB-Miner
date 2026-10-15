// ============================================================
// ESP32 DGB Miner - SHA-256 Engine (Optimized for Mining)
// ============================================================
#ifndef SHA256_ENGINE_H
#define SHA256_ENGINE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

// ── SHA-256 Context ─────────────────────────────────────────
typedef struct {
    uint32_t state[8];
    uint64_t count;       // total bytes processed
    uint8_t  buffer[64];
} SHA256_CTX;

// ── Core SHA-256 API ────────────────────────────────────────
void sha256_init(SHA256_CTX* ctx);
void sha256_transform(uint32_t state[8], const uint8_t block[64]);
void sha256_update(SHA256_CTX* ctx, const uint8_t* data, size_t len);
void sha256_final(SHA256_CTX* ctx, uint8_t hash[32]);

// ── Double SHA-256 (SHA-256d) ───────────────────────────────
void sha256d(const uint8_t* data, size_t len, uint8_t hash[32]);

// ── Mining Helpers ──────────────────────────────────────────

// Compute SHA-256 midstate from first 64 bytes of 80-byte block header.
// The midstate allows skipping the first SHA-256 compression per nonce.
void sha256_midstate(const uint8_t header[80], uint32_t midstate[8]);

// Prepare the second 64-byte block template (header[64..79] + SHA-256 padding).
// Call once per job. The nonce field (bytes 12-15) will be overwritten per hash.
void sha256_prepare_block2(const uint8_t header[80], uint8_t block2[80]);

// Mine a single nonce: compute SHA-256d using midstate + prepared block2.
// Writes the final 32-byte hash (big-endian SHA order) into `hash`.
void sha256d_mine_nonce(const uint32_t midstate[8], uint8_t block2[80],
                        uint32_t nonce, uint8_t hash[32]);

// Check if hash (big-endian SHA order) meets target (display order, MSB first).
// The hash bytes are reversed internally for comparison.
// Returns true if the share is valid (hash < target in display order).
bool checkHashTarget(const uint8_t hash[32], const uint8_t target[32]);

// Convert Stratum pool difficulty to a 32-byte target in display order.
void difficultyToTarget(double difficulty, uint8_t target[32]);

#endif // SHA256_ENGINE_H
