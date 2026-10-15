// ============================================================
// ESP32 DGB Miner - WiFi Manager
// ============================================================
#ifndef WIFI_MANAGER_H
#define WIFI_MANAGER_H

#include "config.h"
#include <DNSServer.h>

class WiFiManager {
public:
    WiFiManager();

    void begin();                   // Load config from NVS
    bool isConfigured() const;      // Returns true if WiFi is configured
    bool connectSTA();              // Connect to saved WiFi (blocking, with timeout)
    void startAP();                 // Start AP mode with captive portal DNS
    void loop();                    // Handle DNS queries (call in main loop)

    void saveConfig(const MinerConfig& cfg);
    void loadConfig(MinerConfig& cfg);
    void clearConfig();

    bool isAPMode() const { return _apMode; }

private:
    DNSServer _dns;
    bool      _apMode;
    bool      _dnsStarted;
};

#endif // WIFI_MANAGER_H
