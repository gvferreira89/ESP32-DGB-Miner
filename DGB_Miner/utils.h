// ============================================================
// ESP32 DGB Miner - Utility Functions
// ============================================================
#ifndef UTILS_H
#define UTILS_H

#include <stdint.h>
#include <stddef.h>

// ── Hex Conversion ──────────────────────────────────────────
void hex2bin(const char* hex, uint8_t* bin, size_t binLen);
void bin2hex(const uint8_t* bin, size_t binLen, char* hex);

// ── Endianness ──────────────────────────────────────────────
void     reverseBytes(uint8_t* data, size_t len);
uint32_t swapEndian32(uint32_t val);
uint32_t readUint32BE(const uint8_t* data);
void     writeUint32BE(uint8_t* data, uint32_t val);
uint32_t readUint32LE(const uint8_t* data);
void     writeUint32LE(uint8_t* data, uint32_t val);

// ── Formatting ──────────────────────────────────────────────
void formatUptime(uint32_t seconds, char* out, size_t maxLen);
void formatHashrate(float hps, char* out, size_t maxLen);

// ── JSON Helpers (minimal, for Stratum protocol) ────────────
const char* jsonSkipWhitespace(const char* p);
const char* jsonSkipValue(const char* p);
const char* jsonFindKey(const char* json, const char* key);
const char* jsonArrayGet(const char* arrayStart, int index);
bool        jsonReadString(const char* p, char* out, size_t maxLen);
bool        jsonReadInt(const char* p, int32_t* out);
bool        jsonReadDouble(const char* p, double* out);
bool        jsonReadBool(const char* p, bool* out);

#endif // UTILS_H
