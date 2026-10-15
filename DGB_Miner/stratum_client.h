// ============================================================
// ESP32 DGB Miner - Stratum v1 Client
// ============================================================
#ifndef STRATUM_CLIENT_H
#define STRATUM_CLIENT_H

#include "config.h"
#include <WiFi.h>

class StratumClient {
public:
    enum State {
        STATE_DISCONNECTED,
        STATE_SUBSCRIBING,
        STATE_AUTHORIZING,
        STATE_MINING
    };

    StratumClient();

    void begin(const char* host, uint16_t port,
               const char* wallet, const char* worker,
               const char* password);
    void loop();                                  // Call each iteration
    bool submitShare(const MiningShare& share);   // Send share to pool
    State getState() const { return _state; }
    bool  isMining() const { return _state == STATE_MINING; }

private:
    WiFiClient _tcp;
    State      _state;

    // Connection params
    char     _host[128];
    uint16_t _port;
    char     _wallet[128];
    char     _worker[32];
    char     _password[64];

    // Stratum session
    char     _extranonce1[32];
    int      _extranonce2Size;
    uint32_t _extranonce2;        // Counter, incremented per job
    double   _difficulty;
    uint8_t  _target[32];

    // Receive buffer (line-delimited JSON)
    char _buf[STRATUM_BUF_SIZE];
    int  _bufPos;

    // Message ID tracking
    int _nextId;
    int _subscribeId;
    int _authorizeId;

    // Reconnection
    uint32_t _lastConnectAttempt;

    // Internal methods
    bool tryConnect();
    void disconnect();
    void readSocket();
    void processLine(const char* line);
    void sendLine(const char* json);

    // Protocol handlers
    void sendSubscribe();
    void sendAuthorize();
    void handleSubscribeResult(const char* json);
    void handleAuthorizeResult(const char* json);
    void handleSubmitResult(const char* json, int id);
    void handleNotify(const char* json);
    void handleSetDifficulty(const char* json);

    // Block construction
    void buildJobFromNotify(const char* json);
    void computeMerkleRoot(const char* coinb1, const char* coinb2,
                           const char merkleBranch[][MERKLE_HASH_LEN],
                           int branchCount, uint8_t merkleRoot[32]);
};

#endif // STRATUM_CLIENT_H
