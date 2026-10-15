// ============================================================
//  ESP32 DGB Miner — DigiByte SHA-256d Mining Firmware
//  Version 1.0.0
// ============================================================
//  Features:
//    • SHA-256d mining with midstate optimisation (~60 kH/s)
//    • Stratum v1 pool client (auto-reconnect)
//    • Captive-portal WiFi + pool configuration
//    • Real-time web dashboard (dark-themed, auto-refresh)
//    • Dual-core: Core 0 = network, Core 1 = mining
//    • Zero external library dependencies
// ============================================================

#include "config.h"
#include "utils.h"
#include "sha256_engine.h"
#include "wifi_manager.h"
#include "stratum_client.h"
#include "mining_task.h"
#include "web_dashboard.h"

// ── Global Objects ──────────────────────────────────────────
QueueHandle_t     jobQueue     = NULL;
QueueHandle_t     shareQueue   = NULL;
MinerStats        minerStats;
MinerConfig       minerConfig;
SemaphoreHandle_t statsMutex   = NULL;

WiFiManager    wifiMgr;
StratumClient  stratum;
WebDashboard   dashboard;

// ── Setup ───────────────────────────────────────────────────
void setup() {
    Serial.begin(SERIAL_BAUD);
    delay(800);

    Serial.println();
    Serial.println("╔═══════════════════════════════════════╗");
    Serial.println("║     ⛏  ESP32 DGB Miner  v1.0.0       ║");
    Serial.println("║     DigiByte SHA-256d Mining          ║");
    Serial.println("╚═══════════════════════════════════════╝");
    Serial.println();
    Serial.printf("Chip : %s  Rev %d\n", ESP.getChipModel(), ESP.getChipRevision());
    Serial.printf("Flash: %u KB  |  Heap: %u KB\n",
                  ESP.getFlashChipSize() / 1024, ESP.getFreeHeap() / 1024);
    Serial.printf("Cores: %d  |  Freq: %u MHz\n", ESP.getChipCores(), ESP.getCpuFreqMHz());
    Serial.println();

    // LED
    pinMode(LED_PIN, OUTPUT);
    digitalWrite(LED_PIN, LOW);

    // Initialise stats
    memset(&minerStats, 0, sizeof(minerStats));
    minerStats.startTimeMs = millis();

    // FreeRTOS primitives
    jobQueue   = xQueueCreate(1, sizeof(MiningJob));    // latest job only
    shareQueue = xQueueCreate(5, sizeof(MiningShare));   // buffer a few shares
    statsMutex = xSemaphoreCreateMutex();

    // Load saved configuration from NVS
    wifiMgr.begin();

    if (wifiMgr.isConfigured()) {
        // ── Station Mode ────────────────────────────────────
        Serial.println("[Boot] Configuration found — connecting to WiFi...");

        if (wifiMgr.connectSTA()) {
            minerStats.wifiConnected = true;

            // Start web dashboard in STA mode
            dashboard.begin(false);

            // Start Stratum client
            stratum.begin(minerConfig.poolUrl,
                          minerConfig.poolPort,
                          minerConfig.walletAddr,
                          minerConfig.workerName,
                          minerConfig.poolPass);

            // Start mining task on Core 1
            startMiningTask();

            Serial.println("[Boot] ✓ Mining started! Dashboard at http://" +
                           WiFi.localIP().toString());
        } else {
            // WiFi connection failed — fall back to AP mode
            Serial.println("[Boot] WiFi failed — falling back to AP setup portal");
            wifiMgr.startAP();
            dashboard.begin(true);
        }
    } else {
        // ── AP / Setup Mode ─────────────────────────────────
        Serial.println("[Boot] No config — starting setup portal...");
        wifiMgr.startAP();
        dashboard.begin(true);
    }
}

// ── Main Loop (Core 0) ─────────────────────────────────────
void loop() {
    // WiFi manager (DNS for captive portal, auto-reconnect)
    wifiMgr.loop();

    // Web server
    dashboard.loop();

    // When connected in STA mode
    if (minerStats.wifiConnected) {
        // Stratum client state machine
        stratum.loop();

        // Forward shares from mining task to stratum
        MiningShare share;
        if (xQueueReceive(shareQueue, &share, 0) == pdTRUE) {
            stratum.submitShare(share);
        }
    }

    // ── Periodic housekeeping ───────────────────────────────
    static uint32_t lastHousekeep = 0;
    uint32_t now = millis();
    if (now - lastHousekeep > 2000) {
        lastHousekeep = now;

        // Update system metrics
        if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(10)) == pdTRUE) {
            minerStats.freeHeap    = ESP.getFreeHeap();
            minerStats.temperature = temperatureRead();
            xSemaphoreGive(statsMutex);
        }

        // Blink LED when mining
        if (minerStats.mining) {
            digitalWrite(LED_PIN, !digitalRead(LED_PIN));
        } else {
            // Slow pulse in AP/idle mode
            static bool ledState = false;
            ledState = !ledState;
            digitalWrite(LED_PIN, ledState);
        }
    }

    delay(5);  // Small yield to prevent WDT on Core 0
}
