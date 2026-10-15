#include "wifi_manager.h"
#include <WiFi.h>
#include <Preferences.h>

#define DNS_PORT 53

WiFiManager::WiFiManager()
    : _apMode(false), _dnsStarted(false) {
}

void WiFiManager::begin() {
    loadConfig(minerConfig);

    if (minerConfig.configured) {
        Serial.printf("[WiFi] Config loaded: SSID='%s' Pool='%s:%d' Wallet='%.16s...'\n",
                      minerConfig.wifiSSID,
                      minerConfig.poolUrl,
                      minerConfig.poolPort,
                      minerConfig.walletAddr);
    } else {
        Serial.println("[WiFi] No saved configuration found");
    }
}

bool WiFiManager::isConfigured() const {
    return minerConfig.configured &&
           strlen(minerConfig.wifiSSID) > 0 &&
           strlen(minerConfig.walletAddr) > 0;
}

bool WiFiManager::connectSTA() {
    WiFi.mode(WIFI_STA);
    WiFi.setAutoReconnect(true);
    WiFi.begin(minerConfig.wifiSSID, minerConfig.wifiPass);

    Serial.printf("[WiFi] Connecting to '%s'", minerConfig.wifiSSID);

    uint32_t start = millis();
    while (WiFi.status() != WL_CONNECTED) {
        if (millis() - start > 30000) {
            Serial.println(" TIMEOUT");
            WiFi.disconnect(true);
            delay(200);
            return false;
        }
        Serial.print(".");
        delay(500);
    }

    Serial.printf(" OK! IP: %s\n", WiFi.localIP().toString().c_str());
    Serial.printf("[WiFi] RSSI: %d dBm | Channel: %d\n", WiFi.RSSI(), WiFi.channel());
    _apMode = false;
    return true;
}

void WiFiManager::startAP() {
    WiFi.disconnect(true);
    delay(200);
    WiFi.mode(WIFI_AP);

    IPAddress apIP(192, 168, 4, 1);
    IPAddress subnet(255, 255, 255, 0);
    WiFi.softAPConfig(apIP, apIP, subnet);
    WiFi.softAP(WIFI_AP_SSID, WIFI_AP_PASS);

    _dns.start(DNS_PORT, "*", apIP);
    _dnsStarted = true;
    _apMode = true;

    Serial.printf("[WiFi] AP started: SSID='%s' IP=%s\n",
                  WIFI_AP_SSID, WiFi.softAPIP().toString().c_str());
    Serial.println("[WiFi] Connect to this network to configure the miner");
}

void WiFiManager::loop() {
    if (_dnsStarted) {
        _dns.processNextRequest();
    }

    if (!_apMode && WiFi.status() != WL_CONNECTED) {
        static uint32_t lastReconnect = 0;
        if (millis() - lastReconnect > WIFI_RETRY_DELAY_MS) {
            lastReconnect = millis();
            Serial.println("[WiFi] Connection lost, reconnecting...");
            minerStats.wifiConnected = false;
            WiFi.reconnect();
        }
    } else if (!_apMode && WiFi.status() == WL_CONNECTED) {
        minerStats.wifiConnected = true;
    }
}

void WiFiManager::saveConfig(const MinerConfig& cfg) {
    Preferences prefs;
    prefs.begin("miner-cfg", false);

    prefs.putString("ssid",     cfg.wifiSSID);
    prefs.putString("pass",     cfg.wifiPass);
    prefs.putString("pool",     cfg.poolUrl);
    prefs.putInt("port",        cfg.poolPort);
    prefs.putString("poolpass", cfg.poolPass);
    prefs.putString("wallet",   cfg.walletAddr);
    prefs.putString("worker",   cfg.workerName);
    prefs.putBool("configured", true);

    prefs.end();
    Serial.println("[WiFi] Configuration saved to NVS");
}

void WiFiManager::loadConfig(MinerConfig& cfg) {
    memset(&cfg, 0, sizeof(MinerConfig));

    Preferences prefs;
    prefs.begin("miner-cfg", true);

    bool hasNVS = prefs.getBool("configured", false);

    if (hasNVS) {
        prefs.getString("ssid", cfg.wifiSSID, sizeof(cfg.wifiSSID));
        prefs.getString("pass", cfg.wifiPass, sizeof(cfg.wifiPass));
        cfg.poolPort = prefs.getInt("port", DEFAULT_POOL_PORT);
        prefs.getString("pool", cfg.poolUrl, sizeof(cfg.poolUrl));
        prefs.getString("poolpass", cfg.poolPass, sizeof(cfg.poolPass));
        prefs.getString("wallet", cfg.walletAddr, sizeof(cfg.walletAddr));
        prefs.getString("worker", cfg.workerName, sizeof(cfg.workerName));
        cfg.configured = true;
        Serial.printf("[WiFi] Config loaded from NVS! Worker: %s\n", cfg.workerName);
    } else {
        // Defaults se ainda nao configurado via Web
        strncpy(cfg.poolUrl, DEFAULT_POOL_URL, sizeof(cfg.poolUrl) - 1);
        cfg.poolPort = DEFAULT_POOL_PORT;
        strncpy(cfg.poolPass, DEFAULT_POOL_PASS, sizeof(cfg.poolPass) - 1);
        strncpy(cfg.workerName, DEFAULT_WORKER, sizeof(cfg.workerName) - 1);
        cfg.configured = false; // Inicia em modo AP / Captive Portal
        Serial.println("[WiFi] Nenhuma configuracao salva. Iniciando modo Access Point.");
    }

    prefs.end();
}

void WiFiManager::clearConfig() {
    Preferences prefs;
    prefs.begin("miner-cfg", false);
    prefs.clear();
    prefs.end();
    memset(&minerConfig, 0, sizeof(MinerConfig));
    Serial.println("[WiFi] Configuration cleared");
}
