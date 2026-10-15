// ============================================================
// ESP32 DGB Miner - Utility Functions Implementation
// ============================================================
#include "utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

// ── Hex Conversion ──────────────────────────────────────────

static uint8_t hexNibble(char c) {
    if (c >= '0' && c <= '9') return (uint8_t)(c - '0');
    if (c >= 'a' && c <= 'f') return (uint8_t)(c - 'a' + 10);
    if (c >= 'A' && c <= 'F') return (uint8_t)(c - 'A' + 10);
    return 0;
}

void hex2bin(const char* hex, uint8_t* bin, size_t binLen) {
    for (size_t i = 0; i < binLen; i++) {
        bin[i] = (hexNibble(hex[i * 2]) << 4) | hexNibble(hex[i * 2 + 1]);
    }
}

void bin2hex(const uint8_t* bin, size_t binLen, char* hex) {
    static const char tbl[] = "0123456789abcdef";
    for (size_t i = 0; i < binLen; i++) {
        hex[i * 2]     = tbl[(bin[i] >> 4) & 0x0F];
        hex[i * 2 + 1] = tbl[bin[i] & 0x0F];
    }
    hex[binLen * 2] = '\0';
}

// ── Endianness ──────────────────────────────────────────────

void reverseBytes(uint8_t* data, size_t len) {
    for (size_t i = 0; i < len / 2; i++) {
        uint8_t tmp = data[i];
        data[i] = data[len - 1 - i];
        data[len - 1 - i] = tmp;
    }
}

uint32_t swapEndian32(uint32_t v) {
    return ((v & 0xFF000000u) >> 24) |
           ((v & 0x00FF0000u) >> 8)  |
           ((v & 0x0000FF00u) << 8)  |
           ((v & 0x000000FFu) << 24);
}

uint32_t readUint32BE(const uint8_t* d) {
    return ((uint32_t)d[0] << 24) | ((uint32_t)d[1] << 16) |
           ((uint32_t)d[2] << 8)  | (uint32_t)d[3];
}

void writeUint32BE(uint8_t* d, uint32_t v) {
    d[0] = (uint8_t)(v >> 24);
    d[1] = (uint8_t)(v >> 16);
    d[2] = (uint8_t)(v >> 8);
    d[3] = (uint8_t)(v);
}

uint32_t readUint32LE(const uint8_t* d) {
    return (uint32_t)d[0] | ((uint32_t)d[1] << 8) |
           ((uint32_t)d[2] << 16) | ((uint32_t)d[3] << 24);
}

void writeUint32LE(uint8_t* d, uint32_t v) {
    d[0] = (uint8_t)(v);
    d[1] = (uint8_t)(v >> 8);
    d[2] = (uint8_t)(v >> 16);
    d[3] = (uint8_t)(v >> 24);
}

// ── Formatting ──────────────────────────────────────────────

void formatUptime(uint32_t seconds, char* out, size_t maxLen) {
    uint32_t h = seconds / 3600;
    uint32_t m = (seconds % 3600) / 60;
    uint32_t s = seconds % 60;
    snprintf(out, maxLen, "%02u:%02u:%02u", (unsigned)h, (unsigned)m, (unsigned)s);
}

void formatHashrate(float hps, char* out, size_t maxLen) {
    if (hps >= 1e9f)      snprintf(out, maxLen, "%.2f GH/s", hps / 1e9f);
    else if (hps >= 1e6f) snprintf(out, maxLen, "%.2f MH/s", hps / 1e6f);
    else if (hps >= 1e3f) snprintf(out, maxLen, "%.2f kH/s", hps / 1e3f);
    else                  snprintf(out, maxLen, "%.1f H/s",  hps);
}

// ── Minimal JSON Helpers (for Stratum protocol) ─────────────
// These handle the specific patterns used by Stratum v1.
// NOT a general-purpose JSON parser.

const char* jsonSkipWhitespace(const char* p) {
    while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n') p++;
    return p;
}

const char* jsonSkipValue(const char* p) {
    p = jsonSkipWhitespace(p);
    if (*p == '"') {
        // String – skip to closing quote (handle escapes)
        p++;
        while (*p && *p != '"') {
            if (*p == '\\') p++;    // skip escaped char
            p++;
        }
        if (*p == '"') p++;
        return p;
    }
    if (*p == '[') {
        // Array
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '"') { p++; while (*p && *p != '"') { if (*p == '\\') p++; p++; } }
            else if (*p == '[') depth++;
            else if (*p == ']') depth--;
            p++;
        }
        return p;
    }
    if (*p == '{') {
        // Object
        int depth = 1;
        p++;
        while (*p && depth > 0) {
            if (*p == '"') { p++; while (*p && *p != '"') { if (*p == '\\') p++; p++; } }
            else if (*p == '{') depth++;
            else if (*p == '}') depth--;
            p++;
        }
        return p;
    }
    // Number, bool, null – skip until delimiter
    while (*p && *p != ',' && *p != ']' && *p != '}' && *p != ' ' && *p != '\t') p++;
    return p;
}

const char* jsonFindKey(const char* json, const char* key) {
    // Search for "key" : <value> and return pointer to <value>
    size_t klen = strlen(key);
    const char* p = json;
    while ((p = strstr(p, "\"")) != NULL) {
        p++;  // skip opening quote
        if (strncmp(p, key, klen) == 0 && p[klen] == '"') {
            p += klen + 1;  // skip key + closing quote
            p = jsonSkipWhitespace(p);
            if (*p == ':') {
                p++;
                return jsonSkipWhitespace(p);
            }
        }
    }
    return NULL;
}

const char* jsonArrayGet(const char* arrayStart, int index) {
    // arrayStart should point to '['. Returns pointer to the Nth element value.
    const char* p = arrayStart;
    if (*p != '[') return NULL;
    p++;
    p = jsonSkipWhitespace(p);
    if (*p == ']') return NULL;  // empty array

    for (int i = 0; i < index; i++) {
        p = jsonSkipValue(p);
        p = jsonSkipWhitespace(p);
        if (*p == ',') p++;
        p = jsonSkipWhitespace(p);
        if (*p == ']') return NULL;  // index out of range
    }
    return p;
}

bool jsonReadString(const char* p, char* out, size_t maxLen) {
    p = jsonSkipWhitespace(p);
    if (*p != '"') return false;
    p++;
    size_t i = 0;
    while (*p && *p != '"' && i < maxLen - 1) {
        if (*p == '\\') { p++; if (!*p) break; }
        out[i++] = *p++;
    }
    out[i] = '\0';
    return true;
}

bool jsonReadInt(const char* p, int32_t* out) {
    p = jsonSkipWhitespace(p);
    if (*p == '"') return false;  // it's a string, not a number
    char* end;
    long val = strtol(p, &end, 10);
    if (end == p) return false;
    *out = (int32_t)val;
    return true;
}

bool jsonReadDouble(const char* p, double* out) {
    p = jsonSkipWhitespace(p);
    if (*p == '"') return false;
    char* end;
    double val = strtod(p, &end);
    if (end == p) return false;
    *out = val;
    return true;
}

bool jsonReadBool(const char* p, bool* out) {
    p = jsonSkipWhitespace(p);
    if (strncmp(p, "true", 4) == 0)       { *out = true;  return true; }
    if (strncmp(p, "false", 5) == 0)      { *out = false; return true; }
    return false;
}
