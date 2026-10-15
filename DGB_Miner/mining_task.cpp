// ============================================================
// ESP32 DGB Miner - Mining Task Implementation (Core 1)
// ============================================================
#include "mining_task.h"
#include "sha256_engine.h"
#include "utils.h"
#include <Arduino.h>
#include <esp_task_wdt.h>

static TaskHandle_t miningTaskHandle = NULL;

// ── Mining Task Function ────────────────────────────────────
static void miningTaskFunc(void* param) {
    Serial.println("[Mining] Task started on Core 1");

    MiningJob currentJob;
    memset(&currentJob, 0, sizeof(currentJob));

    // Pre-allocated buffers for the inner loop
    uint8_t block2[80];        // Full 80-byte header buffer
    uint8_t hash[32];          // Final double-hash output
    uint32_t hashCount = 0;
    uint32_t windowStart = millis();

    while (true) {
        // ── Wait for a new job ──────────────────────────────
        MiningJob newJob;
        TickType_t waitTime = currentJob.valid ? 0 : pdMS_TO_TICKS(100);

        if (xQueueReceive(jobQueue, &newJob, waitTime) == pdTRUE) {
            // New job received
            memcpy(&currentJob, &newJob, sizeof(MiningJob));
            hashCount = 0;
            windowStart = millis();

            Serial.printf("[Mining] New job: %s\n", currentJob.jobId);

            // Prepare the second block template (padding is constant)
            sha256_prepare_block2(currentJob.header, block2);

            // Update stats
            if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                minerStats.mining = true;
                strncpy(minerStats.currentJobId, currentJob.jobId,
                        sizeof(minerStats.currentJobId) - 1);
                xSemaphoreGive(statsMutex);
            }
        }

        if (!currentJob.valid) {
            vTaskDelay(pdMS_TO_TICKS(100));
            continue;
        }

        // ── Mine a batch of nonces ──────────────────────────
        uint32_t nonce = currentJob.startNonce;
        uint32_t batchEnd = nonce + WDT_YIELD_NONCES;
        if (batchEnd < nonce) batchEnd = 0xFFFFFFFF;  // overflow guard

        for (; nonce < batchEnd; nonce++) {
            // Compute SHA-256d with midstate optimization
            sha256d_mine_nonce(currentJob.midstate, block2, nonce, hash);
            hashCount++;

            // Check if hash meets the share target
            if (checkHashTarget(hash, currentJob.target)) {
                // ── Valid share found! ──────────────────────
                Serial.printf("[Mining] *** SHARE FOUND! *** nonce=0x%08X\n", nonce);

                // Calculate approximate share difficulty
                double shareDiff = 0.0;
                // Count leading zero bits in display hash for rough diff estimate
                int zeroBits = 0;
                for (int i = 31; i >= 0 && hash[i] == 0; i--) zeroBits += 8;
                if (zeroBits > 0) {
                    shareDiff = (double)(1ULL << zeroBits) / 65535.0;
                }

                // Build share
                MiningShare share;
                strncpy(share.jobId, currentJob.jobId, sizeof(share.jobId) - 1);
                share.jobId[sizeof(share.jobId) - 1] = '\0';
                strncpy(share.extranonce2Hex, currentJob.extranonce2Hex,
                        sizeof(share.extranonce2Hex) - 1);
                share.extranonce2Hex[sizeof(share.extranonce2Hex) - 1] = '\0';
                strncpy(share.ntimeHex, currentJob.ntimeHex,
                        sizeof(share.ntimeHex) - 1);
                share.ntimeHex[sizeof(share.ntimeHex) - 1] = '\0';
                share.nonce = nonce;

                // Send to stratum client via queue
                xQueueSend(shareQueue, &share, pdMS_TO_TICKS(100));

                // Update best difficulty
                if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                    if (shareDiff > minerStats.bestDiff) {
                        minerStats.bestDiff = shareDiff;
                    }
                    xSemaphoreGive(statsMutex);
                }
            }
        }

        // Update start nonce for next batch
        currentJob.startNonce = nonce;
        if (nonce == 0xFFFFFFFF) {
            // Full nonce range exhausted (extremely unlikely at 60 kH/s)
            Serial.println("[Mining] Nonce space exhausted, waiting for new job...");
            currentJob.valid = false;
        }

        // ── Update hashrate statistics ──────────────────────
        uint32_t now = millis();
        uint32_t elapsed = now - windowStart;
        if (elapsed >= 5000) {   // Update every 5 seconds
            float rate = (float)hashCount / ((float)elapsed / 1000.0f);

            if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
                minerStats.hashrate = rate;
                minerStats.totalHashes += hashCount;
                xSemaphoreGive(statsMutex);
            }

            char hrStr[32];
            formatHashrate(rate, hrStr, sizeof(hrStr));
            Serial.printf("[Mining] Hashrate: %s | Total: %llu hashes\n",
                          hrStr, minerStats.totalHashes);

            hashCount = 0;
            windowStart = now;
        }

        // ── Check for new job (non-blocking peek) ───────────
        MiningJob peekJob;
        if (xQueuePeek(jobQueue, &peekJob, 0) == pdTRUE) {
            // New job available — will be dequeued at top of loop
            continue;
        }

        // Yield to prevent watchdog timeout
        taskYIELD();
    }
}

// ── Public API ──────────────────────────────────────────────

void startMiningTask() {
    if (miningTaskHandle != NULL) {
        Serial.println("[Mining] Task already running");
        return;
    }

    xTaskCreatePinnedToCore(
        miningTaskFunc,           // Function
        "MiningTask",             // Name
        MINING_TASK_STACK,        // Stack size
        NULL,                     // Parameter
        MINING_TASK_PRIORITY,     // Priority
        &miningTaskHandle,        // Handle
        MINING_CORE               // Core 1
    );

    Serial.println("[Mining] Task created on Core 1");
}

void stopMiningTask() {
    if (miningTaskHandle != NULL) {
        vTaskDelete(miningTaskHandle);
        miningTaskHandle = NULL;

        if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
            minerStats.mining = false;
            minerStats.hashrate = 0;
            xSemaphoreGive(statsMutex);
        }

        Serial.println("[Mining] Task stopped");
    }
}

bool isMiningTaskRunning() {
    return miningTaskHandle != NULL;
}
