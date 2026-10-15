#include "stratum_client.h"
#include "utils.h"
#include "sha256_engine.h"
#include <string.h>
#include <stdio.h>

StratumClient::StratumClient() {
    _state = STATE_DISCONNECTED;
    _host[0] = '\0';
    _port = 0;
    _wallet[0] = '\0';
    _worker[0] = '\0';
    _password[0] = '\0';
    _extranonce1[0] = '\0';
    _extranonce2Size = 0;
    _extranonce2 = 0;
    _difficulty = 1.0;
    memset(_target, 0, sizeof(_target));
    _bufPos = 0;
    _nextId = 1;
    _subscribeId = -1;
    _authorizeId = -1;
    _lastConnectAttempt = 0;
}

void StratumClient::begin(const char* host, uint16_t port,
                          const char* wallet, const char* worker,
                          const char* password) {
    strncpy(_host, host, sizeof(_host) - 1);
    _host[sizeof(_host) - 1] = '\0';
    _port = port;
    strncpy(_wallet, wallet, sizeof(_wallet) - 1);
    _wallet[sizeof(_wallet) - 1] = '\0';
    strncpy(_worker, worker, sizeof(_worker) - 1);
    _worker[sizeof(_worker) - 1] = '\0';
    strncpy(_password, password, sizeof(_password) - 1);
    _password[sizeof(_password) - 1] = '\0';

    _state = STATE_DISCONNECTED;
    _bufPos = 0;
}

void StratumClient::loop() {
    if (WiFi.status() != WL_CONNECTED) {
        disconnect();
        return;
    }

    if (!_tcp.connected()) {
        if (_state != STATE_DISCONNECTED) {
            disconnect();
        }
    }

    if (_state == STATE_DISCONNECTED) {
        if (millis() - _lastConnectAttempt >= STRATUM_RECONNECT_MS) {
            _lastConnectAttempt = millis();
            tryConnect();
        }
    } else {
        readSocket();
    }
}

void StratumClient::disconnect() {
    if (_tcp.connected()) {
        _tcp.stop();
    }
    _state = STATE_DISCONNECTED;
    _bufPos = 0;
    
    xSemaphoreTake(statsMutex, portMAX_DELAY);
    minerStats.poolConnected = false;
    xSemaphoreGive(statsMutex);
    
    Serial.printf("[Stratum] Disconnected from pool\n");
}

bool StratumClient::tryConnect() {
    Serial.printf("[Stratum] Connecting to %s:%d...\n", _host, _port);
    if (_tcp.connect(_host, _port)) {
        Serial.printf("[Stratum] Connected!\n");
        _state = STATE_SUBSCRIBING;
        _bufPos = 0;
        _extranonce2 = 0;
        
        xSemaphoreTake(statsMutex, portMAX_DELAY);
        minerStats.poolConnected = true;
        xSemaphoreGive(statsMutex);
        
        sendSubscribe();
        return true;
    }
    Serial.printf("[Stratum] Connection failed\n");
    return false;
}

void StratumClient::readSocket() {
    while (_tcp.available()) {
        char c = _tcp.read();
        if (c == '\n') {
            _buf[_bufPos] = '\0';
            if (_bufPos > 0 && _buf[_bufPos - 1] == '\r') {
                _buf[_bufPos - 1] = '\0';
            }
            processLine(_buf);
            _bufPos = 0;
        } else {
            if (_bufPos < STRATUM_BUF_SIZE - 1) {
                _buf[_bufPos++] = c;
            }
        }
    }
}

void StratumClient::processLine(const char* line) {
    Serial.printf("[Pool] %s\n", line);
    const char* method = jsonFindKey(line, "method");
    if (method) {
        char methodStr[64];
        if (jsonReadString(method, methodStr, sizeof(methodStr))) {
            if (strcmp(methodStr, "mining.notify") == 0) {
                handleNotify(line);
            } else if (strcmp(methodStr, "mining.set_difficulty") == 0) {
                handleSetDifficulty(line);
            }
        }
        return;
    }

    const char* result = jsonFindKey(line, "result");
    const char* idStr = jsonFindKey(line, "id");
    int32_t id = -1;
    if (idStr) {
        jsonReadInt(idStr, &id);
    }
    
    if (result && id != -1) {
        if (id == _subscribeId) {
            handleSubscribeResult(line);
        } else if (id == _authorizeId) {
            handleAuthorizeResult(line);
        } else {
            handleSubmitResult(line, id);
        }
    }
}

void StratumClient::sendLine(const char* json) {
    _tcp.print(json);
    _tcp.print('\n');
}

void StratumClient::sendSubscribe() {
    _subscribeId = _nextId++;
    char buf[256];
    snprintf(buf, sizeof(buf), "{\"id\":%d,\"method\":\"mining.subscribe\",\"params\":[\"ESP32-DGB-Miner/1.0\"]}", _subscribeId);
    sendLine(buf);
}

void StratumClient::sendAuthorize() {
    _authorizeId = _nextId++;
    char buf[256];
    char walletWorker[160];
    snprintf(walletWorker, sizeof(walletWorker), "%s.%s", _wallet, _worker);
    snprintf(buf, sizeof(buf), "{\"id\":%d,\"method\":\"mining.authorize\",\"params\":[\"%s\",\"%s\"]}", _authorizeId, walletWorker, _password);
    sendLine(buf);
}

void StratumClient::handleSubscribeResult(const char* json) {
    const char* result = jsonFindKey(json, "result");
    if (result) {
        const char* en1 = jsonArrayGet(result, 1);
        if (en1) {
            jsonReadString(en1, _extranonce1, sizeof(_extranonce1));
        }
        const char* en2sz = jsonArrayGet(result, 2);
        if (en2sz) {
            int32_t sz = 0;
            if (jsonReadInt(en2sz, &sz)) {
                _extranonce2Size = sz;
            }
        }
        Serial.printf("[Stratum] Subscribed. Extranonce1: %s, Extranonce2_size: %d\n", _extranonce1, _extranonce2Size);
        _state = STATE_AUTHORIZING;
        sendAuthorize();
    }
}

void StratumClient::handleAuthorizeResult(const char* json) {
    const char* result = jsonFindKey(json, "result");
    bool authOk = false;
    if (result) {
        jsonReadBool(result, &authOk);
    }
    if (authOk) {
        Serial.printf("[Stratum] Authorized successfully\n");
        _state = STATE_MINING;
    } else {
        Serial.printf("[Stratum] Authorization failed\n");
        disconnect();
    }
}

void StratumClient::handleSubmitResult(const char* json, int id) {
    const char* result = jsonFindKey(json, "result");
    bool accepted = false;
    if (result) {
        jsonReadBool(result, &accepted);
    }
    
    xSemaphoreTake(statsMutex, portMAX_DELAY);
    if (accepted) {
        minerStats.sharesAccepted++;
        Serial.printf("[Stratum] Share accepted (id: %d)\n", id);
    } else {
        minerStats.sharesRejected++;
        Serial.printf("[Stratum] Share rejected (id: %d)\n", id);
    }
    xSemaphoreGive(statsMutex);
}

void StratumClient::handleNotify(const char* json) {
    buildJobFromNotify(json);
}

void StratumClient::buildJobFromNotify(const char* json) {
    const char* params = jsonFindKey(json, "params");
    if (!params) return;

    MiningJob job;
    memset(&job, 0, sizeof(job));
    job.valid = true;

    // params[0]: jobId
    const char* p0 = jsonArrayGet(params, 0);
    if (p0) jsonReadString(p0, job.jobId, sizeof(job.jobId));
    
    xSemaphoreTake(statsMutex, portMAX_DELAY);
    strncpy(minerStats.currentJobId, job.jobId, sizeof(minerStats.currentJobId));
    xSemaphoreGive(statsMutex);

    // params[1]: prevhash
    const char* p1 = jsonArrayGet(params, 1);
    char prevhashHex[65] = {0};
    if (p1) jsonReadString(p1, prevhashHex, sizeof(prevhashHex));

    // params[2]: coinb1
    const char* p2 = jsonArrayGet(params, 2);
    static char coinb1[COINBASE_BUF_SIZE];
    coinb1[0] = '\0';
    if (p2) jsonReadString(p2, coinb1, sizeof(coinb1));

    // params[3]: coinb2
    const char* p3 = jsonArrayGet(params, 3);
    char coinb2[256] = {0};
    if (p3) jsonReadString(p3, coinb2, sizeof(coinb2));

    // params[4]: merkle_branch
    const char* p4 = jsonArrayGet(params, 4);
    char merkleBranch[MAX_MERKLE_BRANCHES][MERKLE_HASH_LEN];
    int branchCount = 0;
    if (p4) {
        for (int i = 0; i < MAX_MERKLE_BRANCHES; i++) {
            const char* branchElem = jsonArrayGet(p4, i);
            if (!branchElem) break;
            if (jsonReadString(branchElem, merkleBranch[i], MERKLE_HASH_LEN)) {
                branchCount++;
            }
        }
    }

    // params[5]: version
    const char* p5 = jsonArrayGet(params, 5);
    char versionHex[9] = {0};
    if (p5) jsonReadString(p5, versionHex, sizeof(versionHex));

    // params[6]: nbits
    const char* p6 = jsonArrayGet(params, 6);
    char nbitsHex[9] = {0};
    if (p6) jsonReadString(p6, nbitsHex, sizeof(nbitsHex));

    // params[7]: ntime
    const char* p7 = jsonArrayGet(params, 7);
    char ntimeHex[9] = {0};
    if (p7) jsonReadString(p7, ntimeHex, sizeof(ntimeHex));
    strncpy(job.ntimeHex, ntimeHex, sizeof(job.ntimeHex));

    // params[8]: clean_jobs
    const char* p8 = jsonArrayGet(params, 8);
    bool cleanJobs = false;
    if (p8) jsonReadBool(p8, &cleanJobs);

    // a. Increment _extranonce2
    _extranonce2++;

    // b. Build extranonce2 hex string
    char extranonce2Hex[33] = {0};
    if (_extranonce2Size > 0 && _extranonce2Size <= 16) {
        snprintf(extranonce2Hex, sizeof(extranonce2Hex), "%0*x", _extranonce2Size * 2, _extranonce2);
    }
    strncpy(job.extranonce2Hex, extranonce2Hex, sizeof(job.extranonce2Hex));

    // c. Compute merkle root
    uint8_t merkleRoot[32];
    computeMerkleRoot(coinb1, coinb2, merkleBranch, branchCount, merkleRoot);

    // d. Build 80-byte block header
    uint8_t versionBin[4];
    hex2bin(versionHex, versionBin, 4);
    reverseBytes(versionBin, 4);
    memcpy(&job.header[0], versionBin, 4);

    uint8_t prevhashBin[32];
    hex2bin(prevhashHex, prevhashBin, 32);
    for (int i = 0; i < 8; i++) {
        reverseBytes(&prevhashBin[i * 4], 4);
    }
    memcpy(&job.header[4], prevhashBin, 32);

    memcpy(&job.header[36], merkleRoot, 32);

    uint8_t ntimeBin[4];
    hex2bin(ntimeHex, ntimeBin, 4);
    reverseBytes(ntimeBin, 4);
    memcpy(&job.header[68], ntimeBin, 4);

    uint8_t nbitsBin[4];
    hex2bin(nbitsHex, nbitsBin, 4);
    reverseBytes(nbitsBin, 4);
    memcpy(&job.header[72], nbitsBin, 4);

    uint32_t nonce = 0;
    memcpy(&job.header[76], &nonce, 4);

    // e. Compute midstate
    sha256_midstate(job.header, job.midstate);

    // f. Target
    memcpy(job.target, _target, 32);

    // Push to queue
    xQueueOverwrite(jobQueue, &job);

    if (cleanJobs) {
        Serial.printf("[Stratum] New job (clean): %s\n", job.jobId);
    }
}

void StratumClient::computeMerkleRoot(const char* coinb1, const char* coinb2,
                                      const char merkleBranch[][MERKLE_HASH_LEN],
                                      int branchCount, uint8_t merkleRoot[32]) {
    static char coinbaseHex[COINBASE_BUF_SIZE * 2];
    snprintf(coinbaseHex, sizeof(coinbaseHex), "%s%s%0*x%s", coinb1, _extranonce1, _extranonce2Size * 2, _extranonce2, coinb2);
    
    size_t coinbaseLen = strlen(coinbaseHex) / 2;
    static uint8_t coinbaseBin[COINBASE_BUF_SIZE];
    hex2bin(coinbaseHex, coinbaseBin, coinbaseLen);

    sha256d(coinbaseBin, coinbaseLen, merkleRoot);

    for (int i = 0; i < branchCount; i++) {
        uint8_t branchBin[32];
        hex2bin(merkleBranch[i], branchBin, 32);

        uint8_t concat[64];
        memcpy(concat, merkleRoot, 32);
        memcpy(concat + 32, branchBin, 32);

        sha256d(concat, 64, merkleRoot);
    }
}

void StratumClient::handleSetDifficulty(const char* json) {
    const char* params = jsonFindKey(json, "params");
    if (params) {
        const char* p0 = jsonArrayGet(params, 0);
        if (p0) {
            double diff = 0;
            if (jsonReadDouble(p0, &diff)) {
                _difficulty = diff;
                difficultyToTarget(_difficulty, _target);
                xSemaphoreTake(statsMutex, portMAX_DELAY);
                minerStats.currentDifficulty = _difficulty;
                xSemaphoreGive(statsMutex);
                Serial.printf("[Stratum] Difficulty set to: %.4f\n", _difficulty);
            }
        }
    }
}

bool StratumClient::submitShare(const MiningShare& share) {
    if (!isMining()) return false;

    char nonceHex[9];
    snprintf(nonceHex, sizeof(nonceHex), "%08x", share.nonce);

    int submitId = _nextId++;
    char buf[512];
    char walletWorker[160];
    snprintf(walletWorker, sizeof(walletWorker), "%s.%s", _wallet, _worker);

    snprintf(buf, sizeof(buf), "{\"id\":%d,\"method\":\"mining.submit\",\"params\":[\"%s\",\"%s\",\"%s\",\"%s\",\"%s\"]}",
             submitId, walletWorker, share.jobId, share.extranonce2Hex, share.ntimeHex, nonceHex);

    sendLine(buf);
    return true;
}
