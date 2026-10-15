// ============================================================
// ESP32 DGB Miner - Web Dashboard & Config Portal
// ============================================================
#ifndef WEB_DASHBOARD_H
#define WEB_DASHBOARD_H

#include "config.h"
#include <WebServer.h>

class WebDashboard {
public:
    WebDashboard();

    void begin(bool apMode);  // Start web server (AP mode = config page, STA = dashboard)
    void loop();              // Handle HTTP clients

private:
    WebServer _server;
    bool      _apMode;

    // Route handlers
    void handleRoot();
    void handleSave();
    void handleDashboard();
    void handleSettings();
    void handleSettingsSave();
    void handleApiStats();
    void handleNotFound();

    // HTML builders
    String buildConfigPage();
    String buildDashboardPage();
    String scanWiFiNetworks();
};

#endif // WEB_DASHBOARD_H
