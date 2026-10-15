# 🏛️ Arquitetura Técnica — ESP32 DGB Miner

Este documento descreve detalhadamente o fluxo interno, arquitetura FreeRTOS, protocolo de comunicação Stratum v1 e otimizações matemáticas do **ESP32 DGB Miner**.

---

## 🧩 1. Visão Geral da Arquitetura

O firmware divide o processamento entre os dois núcleos independentes de 240 MHz do ESP32 através de **FreeRTOS Queues e Semaphores**:

```mermaid
flowchart TD
    subgraph Pool["Pool de Mineração (Stratum v1)"]
        StratumServer["Stratum Server (ex: dgb.fusionpool.pro:3332)"]
    end

    subgraph ESP32["ESP32 Microcontroller (Dual Core 240MHz)"]
        subgraph Core0["Core 0 — Rede & Comunicação"]
            Stratum["Stratum Client\n(TCP Socket)"]
            WiFi["WiFi Manager / AP Cativo"]
            WebDash["Web Dashboard\n(Async WebServer :80)"]
            Stats["MinerStats (Mutex)"]
            
            Stratum <-->|mining.notify / submit| StratumServer
            WebDash --> Stats
        end

        subgraph Queues["FreeRTOS IPC"]
            JobQueue["Job Queue\n(xQueueOverwrite - Tam: 1)"]
            ShareQueue["Share Queue\n(xQueueSend - Tam: 5)"]
        end

        subgraph Core1["Core 1 — Motor Criptográfico"]
            MiningTask["Mining Task (Core 1 Dedicado)\nPrioridade 1, Stack 8KB"]
            Midstate["Otimização Midstate SHA-256\n(64 bytes pré-computados)"]
            Hashing["Loop de Hash Duplo\nSHA-256d(Header 80B)"]
            
            MiningTask --> Midstate
            Midstate --> Hashing
        end
    end

    Stratum -->|Novo Job| JobQueue
    JobQueue -->|Consome Job| MiningTask
    Hashing -->|Share Encontrado| ShareQueue
    ShareQueue -->|mining.submit| Stratum
    MiningTask -->|Atualiza Hashrate/Dificuldade| Stats
```

---

## ⚡ 2. Otimização Criptográfica (Midstate Caching)

O algoritmo **SHA-256d** calcula dois hashes SHA-256 consecutivos sobre o cabeçalho de bloco Bitcoin/DigiByte de 80 bytes:

$$\text{Hash} = \text{SHA256}(\text{SHA256}(\text{Header}_{80}))$$

Como o cabeçalho tem 80 bytes:
1. O primeiro bloco de SHA-256 consome os primeiros **64 bytes** (Versão + PrevHash + parte da Merkle Root).
2. O campo **Nonce** (4 bytes) varia apenas nos últimos **16 bytes** (bytes 64 a 79).

### 🚀 Por que isso acelera o ESP32?
Em vez de reprocessar os 80 bytes a cada nonce testado:
* O firmware pré-calcula o estado interno do SHA-256 nos primeiros 64 bytes uma única vez por Job recebido (**Midstate de 32 bytes**).
* Para cada Nonce testado (de `0x00000000` a `0xFFFFFFFF`), o motor injeta o Midstate e calcula o hash apenas dos 16 bytes finais.
* **Ganho de Desempenho:** Quase **dobra a taxa de hash**, atingindo ~12 a 15 kH/s estáveis por placa sem risco de watchdog reset.

---

## 🔄 3. Fluxo de Comunicação Stratum v1 (JSON-RPC)

O cliente em `stratum_client.cpp` implementa o protocolo Stratum v1 padrão:

```mermaid
sequenceDiagram
    participant ESP as ESP32 (Core 0)
    participant Pool as Stratum Pool

    ESP->>Pool: {"id":1,"method":"mining.subscribe","params":["ESP32-DGB-Miner/1.0"]}
    Pool-->>ESP: {"id":1,"result":[...,"extranonce1",extranonce2_size]}
    
    ESP->>Pool: {"id":2,"method":"mining.authorize","params":["wallet.worker","password"]}
    Pool-->>ESP: {"id":2,"result":true}

    Pool->>ESP: {"method":"mining.set_difficulty","params":[0.001]}
    Pool->>ESP: {"method":"mining.notify","params":["job_id","prevhash","coinb1","coinb2",...]}

    Note over ESP: Core 0 calcula Merkle Root e monta Header.<br/>Envia Job para Core 1 via FreeRTOS Queue.

    Note over ESP: Core 1 minera Nonces.<br/>Calcula Hash <= Target da Pool.

    ESP->>Pool: {"id":4,"method":"mining.submit","params":["wallet.worker","job_id","extranonce2","ntime","nonce"]}
    Pool-->>ESP: {"id":4,"result":true} (Share Aceito!)
```

---

## 📦 4. Tratamento de Endianness (Byte-Swapping)

O protocolo Stratum transmite campos em formatos com ordenação de bytes mista:
* **PrevHash:** Vem em 8 grupos de 4 bytes em Big-Endian. Cada grupo de 4 bytes precisa ser invertido para montagem correta do bloco Little-Endian.
* **Versão, nBits e nTime:** Recebidos em hexadecimais Big-Endian e invertidos para Little-Endian.
* **Merkle Root:** Gerada diretamente no formato correto pelo cálculo das árvores de Merkle com o coinbase modificado.
