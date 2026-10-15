#include "web_dashboard.h"
#include <WiFi.h>
#include <Preferences.h>
#include "utils.h"

extern MinerConfig minerConfig;
extern MinerStats minerStats;
extern SemaphoreHandle_t statsMutex;

WebDashboard::WebDashboard() : _server(80), _apMode(false) {
}

void WebDashboard::begin(bool apMode) {
    _apMode = apMode;

    if (_apMode) {
        _server.on("/", HTTP_GET, [this]() { handleRoot(); });
        _server.on("/save", HTTP_POST, [this]() { handleSave(); });
        
        // Captive portal catch-all routes
        _server.on("/generate_204", HTTP_GET, [this]() { handleRoot(); });
        _server.on("/hotspot-detect.html", HTTP_GET, [this]() { handleRoot(); });
        _server.on("/connecttest.txt", HTTP_GET, [this]() { handleRoot(); });
        _server.on("/fwlink", HTTP_GET, [this]() { handleRoot(); });
        
        _server.onNotFound([this]() { handleRoot(); });
    } else {
        _server.on("/", HTTP_GET, [this]() { handleRoot(); });
        _server.on("/dashboard", HTTP_GET, [this]() { handleDashboard(); });
        _server.on("/settings", HTTP_GET, [this]() { handleSettings(); });
        _server.on("/settings/save", HTTP_POST, [this]() { handleSettingsSave(); });
        _server.on("/api/stats", HTTP_GET, [this]() { handleApiStats(); });
        
        _server.onNotFound([this]() { handleNotFound(); });
    }

    _server.begin();
    Serial.printf("[Web] Server started in %s mode\n", _apMode ? "AP" : "STA");
}

void WebDashboard::loop() {
    _server.handleClient();
}

void WebDashboard::handleRoot() {
    if (_apMode) {
        _server.send(200, "text/html", buildConfigPage());
    } else {
        _server.sendHeader("Location", "/dashboard");
        _server.send(302, "text/plain", "");
    }
}

void WebDashboard::handleDashboard() {
    _server.send(200, "text/html", buildDashboardPage());
}

void WebDashboard::handleSettings() {
    _server.send(200, "text/html", buildConfigPage());
}

void WebDashboard::handleSave() {
    if (_server.hasArg("ssid") && _server.hasArg("pool") && _server.hasArg("wallet")) {
        Preferences prefs;
        prefs.begin("miner-cfg", false);
        
        prefs.putString("ssid", _server.arg("ssid"));
        prefs.putString("pass", _server.arg("pass"));
        prefs.putString("pool", _server.arg("pool"));
        prefs.putInt("port", _server.arg("port").toInt());
        prefs.putString("poolpass", _server.arg("poolpass"));
        prefs.putString("wallet", _server.arg("wallet"));
        prefs.putString("worker", _server.arg("worker"));
        prefs.putBool("configured", true);
        
        prefs.end();
        
        String html = "<!DOCTYPE html><html><head><meta charset=\"UTF-8\">";
        html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
        html += "<meta http-equiv=\"refresh\" content=\"3;url=/\">";
        html += "<style>body{background:#0d1117;color:#e6edf3;font-family:sans-serif;text-align:center;padding:50px;}";
        html += "h1{color:#00ff88;}</style></head><body>";
        html += "<h1>Saved!</h1><p>Restarting ESP32...</p>";
        html += "</body></html>";
        
        _server.send(200, "text/html", html);
        
        delay(1000);
        ESP.restart();
    } else {
        _server.send(400, "text/plain", "Missing required fields");
    }
}

void WebDashboard::handleSettingsSave() {
    handleSave();
}

void WebDashboard::handleApiStats() {
    char hashrateStr[32];
    char uptimeStr[32];
    
    float hashrate = 0;
    uint32_t accepted = 0;
    uint32_t rejected = 0;
    uint32_t startTime = 0;
    bool mining = false;
    float temp = 0;
    uint32_t freeHeap = 0;
    double bestDiff = 0;
    double currentDiff = 0;
    
    if (xSemaphoreTake(statsMutex, pdMS_TO_TICKS(100)) == pdTRUE) {
        hashrate = minerStats.hashrate;
        accepted = minerStats.sharesAccepted;
        rejected = minerStats.sharesRejected;
        startTime = minerStats.startTimeMs;
        mining = minerStats.mining;
        temp = minerStats.temperature;
        freeHeap = minerStats.freeHeap;
        bestDiff = minerStats.bestDiff;
        currentDiff = minerStats.currentDifficulty;
        xSemaphoreGive(statsMutex);
    }
    
    formatHashrate(hashrate, hashrateStr, sizeof(hashrateStr));
    uint32_t uptimeSec = (millis() - startTime) / 1000;
    formatUptime(uptimeSec, uptimeStr, sizeof(uptimeStr));
    
    int rssi = WiFi.RSSI();
    
    String json = "{";
    json += "\"hashrate\":\"" + String(hashrateStr) + "\",";
    json += "\"accepted\":" + String(accepted) + ",";
    json += "\"rejected\":" + String(rejected) + ",";
    json += "\"uptime\":\"" + String(uptimeStr) + "\",";
    json += "\"mining\":" + String(mining ? "true" : "false") + ",";
    json += "\"pool\":\"" + String(minerConfig.poolUrl) + ":" + String(minerConfig.poolPort) + "\",";
    json += "\"wallet\":\"" + String(minerConfig.walletAddr) + "." + String(minerConfig.workerName) + "\",";
    json += "\"temp\":" + String(temp, 1) + ",";
    json += "\"heap\":" + String(freeHeap) + ",";
    json += "\"rssi\":" + String(rssi) + ",";
    json += "\"bestDiff\":" + String(bestDiff, 6) + ",";
    json += "\"difficulty\":" + String(currentDiff, 6);
    json += "}";
    
    _server.send(200, "application/json", json);
}

void WebDashboard::handleNotFound() {
    _server.send(404, "text/plain", "404 Not Found");
}

String WebDashboard::scanWiFiNetworks() {
    int n = WiFi.scanNetworks();
    String options = "";
    if (n == 0) {
        options = "<option value=\"\">No networks found</option>";
    } else {
        for (int i = 0; i < n; ++i) {
            options += "<option value=\"" + WiFi.SSID(i) + "\">" + WiFi.SSID(i) + " (" + String(WiFi.RSSI(i)) + "dBm)</option>";
        }
    }
    return options;
}

String WebDashboard::buildConfigPage() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>⛏️ DGB Miner Setup</title>";
    html += "<style>";
    html += "body { background-color: #0d1117; color: #e6edf3; font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, Helvetica, Arial, sans-serif; margin: 0; padding: 20px; }";
    html += ".container { max-width: 480px; margin: 0 auto; }";
    html += "h2 { text-align: center; color: #e6edf3; margin-bottom: 30px; font-weight: 600; }";
    html += ".card { background-color: #161b22; border: 1px solid #30363d; border-radius: 12px; padding: 20px; margin-bottom: 20px; }";
    html += ".card h3 { margin-top: 0; color: #00ff88; font-size: 16px; border-bottom: 1px solid #30363d; padding-bottom: 10px; margin-bottom: 15px; }";
    html += "label { display: block; margin-bottom: 8px; font-size: 14px; font-weight: 500; color: #8b949e; }";
    html += "input, select { width: 100%; padding: 10px; margin-bottom: 15px; border: 1px solid #30363d; border-radius: 6px; background-color: #0d1117; color: #e6edf3; font-size: 14px; box-sizing: border-box; }";
    html += "input:focus, select:focus { outline: none; border-color: #00ff88; }";
    html += "button { width: 100%; padding: 12px; background-color: #238636; color: #ffffff; border: none; border-radius: 6px; font-size: 16px; font-weight: 600; cursor: pointer; transition: background-color 0.2s; }";
    html += "button:hover { background-color: #2ea043; }";
    html += ".footer { text-align: center; margin-top: 30px; font-size: 12px; color: #8b949e; }";
    html += "</style>";
    html += "</head><body>";
    
    html += "<div class=\"container\">";
    html += "<h2>⛏️ DGB Miner Setup</h2>";
    
    html += "<form action=\"" + String(_apMode ? "/save" : "/settings/save") + "\" method=\"POST\">";
    
    // WiFi Section
    html += "<div class=\"card\">";
    html += "<h3>WiFi Configuration</h3>";
    html += "<label>Select Network</label>";
    html += "<select name=\"ssid\" required>";
    if (!_apMode && String(minerConfig.wifiSSID).length() > 0) {
        html += "<option value=\"" + String(minerConfig.wifiSSID) + "\">" + String(minerConfig.wifiSSID) + " (Saved)</option>";
    } else {
        html += "<option value=\"\">Scanning networks...</option>";
    }
    html += scanWiFiNetworks();
    html += "</select>";
    html += "<label>Password</label>";
    html += "<input type=\"password\" name=\"pass\" value=\"" + String(minerConfig.wifiPass) + "\">";
    html += "</div>";
    
    // Pool Section
    html += "<div class=\"card\">";
    html += "<h3>Pool Configuration</h3>";
    html += "<label>Pool URL</label>";
    html += "<input type=\"text\" name=\"pool\" value=\"" + String(String(minerConfig.poolUrl).length() > 0 ? minerConfig.poolUrl : DEFAULT_POOL_URL) + "\" required>";
    html += "<label>Port</label>";
    html += "<input type=\"number\" name=\"port\" value=\"" + String(minerConfig.poolPort > 0 ? minerConfig.poolPort : DEFAULT_POOL_PORT) + "\" required>";
    html += "<label>Password (Difficulty)</label>";
    html += "<input type=\"text\" name=\"poolpass\" value=\"" + String(String(minerConfig.poolPass).length() > 0 ? minerConfig.poolPass : DEFAULT_POOL_PASS) + "\">";
    html += "</div>";
    
    // Wallet Section
    html += "<div class=\"card\">";
    html += "<h3>Wallet Configuration</h3>";
    html += "<label>DGB Address</label>";
    html += "<input type=\"text\" name=\"wallet\" placeholder=\"dgb1q... or D...\" value=\"" + String(minerConfig.walletAddr) + "\" required>";
    html += "<label>Worker Name</label>";
    html += "<input type=\"text\" name=\"worker\" value=\"" + String(String(minerConfig.workerName).length() > 0 ? minerConfig.workerName : DEFAULT_WORKER) + "\">";
    html += "</div>";
    
    html += "<button type=\"submit\">Save & Restart</button>";
    html += "</form>";
    
    html += "<div class=\"footer\">ESP32 DGB Miner v" + String(FIRMWARE_VERSION) + "</div>";
    html += "</div>";
    
    html += "</body></html>";
    return html;
}

String WebDashboard::buildDashboardPage() {
    String html = "<!DOCTYPE html><html><head>";
    html += "<meta charset=\"UTF-8\">";
    html += "<meta name=\"viewport\" content=\"width=device-width, initial-scale=1.0\">";
    html += "<title>⛏️ DGB Miner</title>";
    html += "<style>";
    html += "body { background-color: #0d1117; color: #e6edf3; font-family: -apple-system, BlinkMacSystemFont, \"Segoe UI\", Roboto, Helvetica, Arial, sans-serif; margin: 0; padding: 20px; }";
    html += ".container { max-width: 600px; margin: 0 auto; }";
    html += "h2 { text-align: center; color: #e6edf3; margin-bottom: 30px; font-weight: 600; display: flex; align-items: center; justify-content: center; gap: 10px; }";
    html += ".status-dot { width: 12px; height: 12px; border-radius: 50%; display: inline-block; }";
    html += ".status-mining { background-color: #00ff88; box-shadow: 0 0 8px #00ff88; }";
    html += ".status-idle { background-color: #ff4444; box-shadow: 0 0 8px #ff4444; }";
    html += ".grid { display: grid; grid-template-columns: 1fr 1fr; gap: 15px; margin-bottom: 20px; }";
    html += ".card { background-color: #161b22; border: 1px solid #30363d; border-radius: 12px; padding: 20px; text-align: center; }";
    html += ".card-label { color: #8b949e; font-size: 14px; font-weight: 500; margin-bottom: 10px; }";
    html += ".card-value { color: #00ff88; font-size: 24px; font-weight: 600; }";
    html += ".sys-info { background-color: #161b22; border: 1px solid #30363d; border-radius: 12px; padding: 20px; margin-bottom: 20px; }";
    html += ".sys-row { display: flex; justify-content: space-between; padding: 8px 0; border-bottom: 1px solid #30363d; font-size: 14px; }";
    html += ".sys-row:last-child { border-bottom: none; }";
    html += ".sys-label { color: #8b949e; }";
    html += ".sys-val { color: #e6edf3; font-family: monospace; }";
    html += ".btn { display: block; width: 100%; padding: 12px; background-color: #21262d; color: #c9d1d9; border: 1px solid #30363d; border-radius: 6px; font-size: 16px; font-weight: 600; text-align: center; text-decoration: none; box-sizing: border-box; transition: background-color 0.2s; }";
    html += ".btn:hover { background-color: #30363d; }";
    html += ".footer { text-align: center; margin-top: 30px; font-size: 12px; color: #8b949e; }";
    html += "</style>";
    html += "<script>";
    html += "function updateStats() {";
    html += "  fetch('/api/stats').then(r => r.json()).then(data => {";
    html += "    document.getElementById('hashrate').textContent = data.hashrate;";
    html += "    document.getElementById('shares').textContent = data.accepted + ' / ' + data.rejected;";
    html += "    document.getElementById('uptime').textContent = data.uptime;";
    html += "    const statEl = document.getElementById('status');";
    html += "    const dotEl = document.getElementById('status-dot');";
    html += "    if(data.mining) {";
    html += "      statEl.textContent = 'Mining';";
    html += "      dotEl.className = 'status-dot status-mining';";
    html += "    } else {";
    html += "      statEl.textContent = 'Idle';";
    html += "      dotEl.className = 'status-dot status-idle';";
    html += "    }";
    html += "    document.getElementById('pool').textContent = data.pool;";
    html += "    document.getElementById('wallet').textContent = data.wallet.length > 20 ? data.wallet.substring(0, 8) + '...' + data.wallet.substring(data.wallet.length - 8) : data.wallet;";
    html += "    document.getElementById('temp').textContent = data.temp + ' °C';";
    html += "    document.getElementById('heap').textContent = (data.heap / 1024).toFixed(1) + ' KB';";
    html += "    document.getElementById('rssi').textContent = data.rssi + ' dBm';";
    html += "    document.getElementById('bestDiff').textContent = data.bestDiff;";
    html += "    document.getElementById('curDiff').textContent = data.difficulty;";
    html += "  });";
    html += "}";
    html += "setInterval(updateStats, 3000);";
    html += "</script>";
    html += "</head><body onload=\"updateStats()\">";
    
    html += "<div class=\"container\">";
    html += "<h2>⛏️ DGB Miner</h2>";
    
    html += "<div class=\"grid\">";
    html += "<div class=\"card\"><div class=\"card-label\">Hashrate</div><div class=\"card-value\" id=\"hashrate\">0.00 kH/s</div></div>";
    html += "<div class=\"card\"><div class=\"card-label\">Shares (A/R)</div><div class=\"card-value\" id=\"shares\">0 / 0</div></div>";
    html += "<div class=\"card\"><div class=\"card-label\">Uptime</div><div class=\"card-value\" id=\"uptime\">00:00:00</div></div>";
    html += "<div class=\"card\"><div class=\"card-label\">Status</div><div class=\"card-value\"><span id=\"status-dot\" class=\"status-dot status-idle\"></span> <span id=\"status\">Idle</span></div></div>";
    html += "</div>";
    
    html += "<div class=\"sys-info\">";
    html += "<div class=\"sys-row\"><span class=\"sys-label\">Pool</span><span class=\"sys-val\" id=\"pool\">...</span></div>";
    html += "<div class=\"sys-row\"><span class=\"sys-label\">Wallet</span><span class=\"sys-val\" id=\"wallet\">...</span></div>";
    html += "<div class=\"sys-row\"><span class=\"sys-label\">Best Difficulty</span><span class=\"sys-val\" id=\"bestDiff\">...</span></div>";
    html += "<div class=\"sys-row\"><span class=\"sys-label\">Current Difficulty</span><span class=\"sys-val\" id=\"curDiff\">...</span></div>";
    html += "<div class=\"sys-row\"><span class=\"sys-label\">Temperature</span><span class=\"sys-val\" id=\"temp\">...</span></div>";
    html += "<div class=\"sys-row\"><span class=\"sys-label\">Free Heap</span><span class=\"sys-val\" id=\"heap\">...</span></div>";
    html += "<div class=\"sys-row\"><span class=\"sys-label\">WiFi Signal</span><span class=\"sys-val\" id=\"rssi\">...</span></div>";
    html += "</div>";
    
    html += "<a href=\"/settings\" class=\"btn\">⚙️ Settings</a>";
    
    html += "<div class=\"footer\">ESP32 DGB Miner v" + String(FIRMWARE_VERSION) + "</div>";
    html += "</div>";
    
    html += "</body></html>";
    return html;
}
