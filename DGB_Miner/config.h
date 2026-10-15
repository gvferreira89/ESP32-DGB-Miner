// ============================================================
// ESP32 DGB Miner - Configuration & Shared Types
// ============================================================
#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>

// ── Firmware Info ───────────────────────────────────────────
#define FIRMWARE_NAME       "ESP32-DGB-Miner"
#define FIRMWARE_VERSION    "1.0.0"

// ── Default Pool Configuration ──────────────────────────────
#define DEFAULT_POOL_URL    "dgb-sha.mining-dutch.nl"
#define DEFAULT_POOL_PORT   3337
#define DEFAULT_POOL_PASS   "d=0.0001"
#define DEFAULT_WORKER      "esp32"

// ── System Configuration ────────────────────────────────────
#define SERIAL_BAUD             115200
#define WIFI_AP_SSID            "ESP32-DGB-Miner"
#define WIFI_AP_PASS            ""
#define WIFI_CONNECT_TIMEOUT_MS 15000
#define WIFI_RETRY_DELAY_MS     5000
#define MAX_WIFI_RETRIES        3

#define STRATUM_BUF_SIZE        2048
#define STRATUM_RECONNECT_MS    5000
#define STRATUM_TIMEOUT_MS      30000

#define COINBASE_BUF_SIZE       2048
#define MAX_MERKLE_BRANCHES     16
#define MERKLE_HASH_LEN         65   // 64 hex chars + null

#define MINING_CORE             1
#define MINING_TASK_STACK       8192
#define MINING_TASK_PRIORITY    1

#define HASHRATE_REPORT_NONCES  100000
#define WDT_YIELD_NONCES        50000

#define LED_PIN                 2

// ── Miner Configuration (persisted in NVS) ──────────────────
struct MinerConfig {
    char wifiSSID[64];
    char wifiPass[64];
    char poolUrl[128];
    uint16_t poolPort;
    char walletAddr[128];
    char workerName[32];
    char poolPass[64];
    bool configured;
};

// ── Mining Job (from Stratum → Mining Task) ─────────────────
struct MiningJob {
    char     jobId[64];
    uint8_t  header[80];          // Full 80-byte block header
    uint32_t midstate[8];         // SHA-256 midstate (first 64 bytes)
    uint8_t  target[32];          // Share target in display order (MSB first)
    char     extranonce2Hex[32];  // Extranonce2 hex for share submission
    char     ntimeHex[16];        // nTime hex for share submission
    uint32_t startNonce;          // Track current nonce across task yields
    bool     valid;
};

// ── Mining Share (Mining Task → Stratum) ────────────────────
struct MiningShare {
    char     jobId[64];
    char     extranonce2Hex[32];
    char     ntimeHex[16];
    uint32_t nonce;
};

// ── Live Statistics ─────────────────────────────────────────
struct MinerStats {
    float    hashrate;            // Current hash rate in H/s
    uint32_t sharesAccepted;
    uint32_t sharesRejected;
    uint64_t totalHashes;
    uint32_t startTimeMs;         // millis() at boot
    bool     mining;
    bool     poolConnected;
    bool     wifiConnected;
    char     currentJobId[64];
    float    temperature;
    uint32_t freeHeap;
    double   bestDiff;
    double   currentDifficulty;
};

// ── Global Handles (defined in DGB_Miner.ino) ──────────────
extern QueueHandle_t     jobQueue;
extern QueueHandle_t     shareQueue;
extern MinerStats        minerStats;
extern MinerConfig       minerConfig;
extern SemaphoreHandle_t statsMutex;

#endif // CONFIG_H
