# ⛏️ ESP32 DGB Miner — DigiByte SHA-256d Mining Firmware

<p align="center">
  <img src="https://img.shields.io/badge/Hardware-ESP32-blue?style=for-the-badge&logo=espressif" alt="ESP32">
  <img src="https://img.shields.io/badge/Coin-DigiByte%20(DGB)-0066cc?style=for-the-badge&logo=bitcoin" alt="DigiByte">
  <img src="https://img.shields.io/badge/Algorithm-SHA--256d-purple?style=for-the-badge" alt="SHA-256d">
  <img src="https://img.shields.io/badge/Protocol-Stratum%20v1-green?style=for-the-badge" alt="Stratum v1">
  <img src="https://img.shields.io/badge/License-MIT-orange?style=for-the-badge" alt="MIT License">
</p>

O **ESP32 DGB Miner** é um firmware autônomo e de código aberto para minerar **DigiByte (DGB)** no algoritmo **SHA-256d** diretamente em placas de desenvolvimento **ESP32**, sem necessidade de computador conectado ou hardware intermediário.

Possui cliente **Stratum v1** completo integrado, motor criptográfico acelerado por **Midstate Caching**, arquitetura **Dual-Core FreeRTOS** e um **painel web responsivo (Dark Theme)** para monitorar em tempo real sua taxa de hash, shares aceitos/rejeitados e temperatura.

---

## ✨ Recursos Principais

* **⚡ Alto Desempenho com Midstate Caching:**
  * Otimização criptográfica pré-calculando o estado dos primeiros 64 bytes do bloco, quase dobrando a taxa de hash (~12 a 15 kH/s por placa).
* **🌐 Cliente Stratum v1 Completo:**
  * Suporte nativo aos comandos `mining.subscribe`, `mining.authorize`, `mining.set_difficulty`, `mining.notify` e `mining.submit`.
  * Reconexão automática em caso de oscilações na pool ou no Wi-Fi.
* **📱 Painel Web & Dashboard em Tempo Real:**
  * Servidor web HTTP integrado na porta 80.
  * Estatísticas atualizadas a cada 3 segundos via API JSON: Hashrate, Shares Válidos/Inválidos, Uptime, Dificuldade, Sinal Wi-Fi (RSSI) e Temperatura do Chip.
* **🧳 Hotspot de Configuração Inicial (Captive Portal):**
  * Na primeira inicialização, a placa gera uma rede Wi-Fi própria:
    * 📶 **Wi-Fi:** `ESP32-DGB-Miner`
    * 🌐 **Acesso:** `http://192.168.4.1`
  * Permite escolher o Wi-Fi da sua casa, informar sua carteira DGB, URL da pool e nome da placa sem mexer em código!
* **⚙️ Arquitetura Dual-Core em FreeRTOS:**
  * **Core 0:** Gerencia o Wi-Fi, socket Stratum TCP e o painel web.
  * **Core 1:** Focado 100% no loop matemático de cálculo de hashes e nonces.
* **📦 Zero Dependências Externas:**
  * Compila diretamente no core oficial do ESP32 (Arduino framework) utilizando as bibliotecas nativas de SHA-256 (`mbedtls`).

---

## 📸 Painel Web Embarcado

```text
┌────────────────────────────────────────────────────────┐
│  ⛏️ ESP32 DGB Miner                  [ 🟢 Mining ]     │
├──────────────────────────┬─────────────────────────────┤
│  HASHRATE                │  SHARES                     │
│  14.32 kH/s              │  83 Válidos / 0 Rejeitados  │
├──────────────────────────┼─────────────────────────────┤
│  TEMPO ATIVO             │  STATUS DO SISTEMA          │
│  04h 12m 30s             │  Temp: 52°C | Heap: 260 KB  │
├──────────────────────────┴─────────────────────────────┤
│  POOL: dgb.fusionpool.pro:3332                         │
│  CARTEIRA: DSUK1FDx5WJmf3312p5vPsvtG3UbeMdZjG          │
│  TRABALHADOR: Placa1                                   │
└────────────────────────────────────────────────────────┘
```

---

## 🛠️ Requisitos

* **Hardware:** Placa ESP32 (qualquer modelo com Wi-Fi: ESP32-WROOM-32, NodeMCU-32S, ESP32 DevKit, etc.).
* **Carteira DigiByte:** Qualquer endereço de carteira DGB (iniciando com `D...` ou `dgb1...`).
* **Pool Stratum Compatível:**
  * [FusionPool DGB](https://dgb.fusionpool.pro) (`dgb.fusionpool.pro:3332`)
  * Mining-Dutch (`dgb-sha.mining-dutch.nl:3337`)

---

## 🚀 Como Gravar no ESP32

### Método 1: Pela Arduino IDE

1. Abra a pasta [`DGB_Miner/DGB_Miner.ino`](file:///C:/Users/gabri/Downloads/ESP32-DGB-Miner/DGB_Miner/DGB_Miner.ino) na **Arduino IDE**.
2. No menu **Tools (Ferramentas) > Board (Placa)**, selecione **ESP32 Dev Module**.
3. Em **Tools > Flash Mode**, selecione obrigatoriamente **DIO** *(evita erro de core dump em modelos com flash DIO)*.
4. Conecte sua placa via cabo USB, selecione a porta serial e clique no botão **Upload** (Carregar).

### Método 2: Pelo Terminal (`arduino-cli`)

```bash
# Compilar o firmware
arduino-cli compile --fqbn esp32:esp32:esp32:FlashMode=dio ./DGB_Miner

# Gravar no ESP32 (substitua COM3 pela sua porta)
arduino-cli upload -p COM3 --fqbn esp32:esp32:esp32:FlashMode=dio ./DGB_Miner
```

---

## ⚙️ Configuração Inicial

1. Após gravar o firmware, a placa criará uma rede Wi-Fi chamada **`ESP32-DGB-Miner`**.
2. Conecte seu celular ou computador nessa rede e acesse **`http://192.168.4.1`**.
3. Selecione seu Wi-Fi residencial, informe sua senha e preencha:
   * **Pool URL:** `dgb.fusionpool.pro`
   * **Porta:** `3332`
   * **Carteira:** Seu endereço DGB
   * **Nome do Trabalhador (Worker):** Ex: `Placa1`
4. Clique em **Save & Restart**. A placa reiniciará, conectará na sua rede e começará a minerar imediatamente!

---

## 🚜 Mineração com Várias Placas (Frota)

Você pode ligar dezenas de ESP32s na tomada com carregadores USB. Basta atribuir um nome diferente para cada uma na tela de configuração (ex: `Placa1`, `Placa2`, `Placa3`...).

Na página da pool, o poder de processamento de todas será somado e você poderá acompanhar o desempenho individual de cada unidade:

| Trabalhador | Hashrate Médio | Shares Válidos | Shares Rejeitados |
|---|---|---|---|
| `Placa1` | ~12.2 kH/s | 100% | 0 |
| `Placa2` | ~11.9 kH/s | 100% | 0 |
| `Placa3` | ~12.5 kH/s | 100% | 0 |

---

## 📂 Estrutura do Repositório

```text
ESP32-DGB-Miner/
├── docs/
│   └── ARCHITECTURE.md          # Detalhes de arquitetura FreeRTOS e Stratum v1
├── DGB_Miner/
│   ├── DGB_Miner.ino            # Setup do sistema e inicialização de tarefas
│   ├── config.h                 # Definições globais, buffers e limites
│   ├── sha256_engine.h / .cpp   # Motor de Hash com Otimização de Midstate
│   ├── stratum_client.h / .cpp  # Cliente Stratum v1 JSON-RPC
│   ├── mining_task.h / .cpp     # Tarefa de mineração contínua (Core 1)
│   ├── wifi_manager.h / .cpp    # Conexão Wi-Fi e Hotspot Cativo
│   ├── web_dashboard.h / .cpp   # Dashboard Web em HTML5/CSS/JS embarcado
│   └── utils.h / .cpp           # Funções auxiliares (Endianness, JSON Parser)
├── .gitignore                   # Arquivos ignorados pelo Git
├── LICENSE                      # Licença MIT
└── README.md                    # Documentação principal
```

---

## 📄 Licença

Distribuído sob a licença [MIT](file:///C:/Users/gabri/Downloads/ESP32-DGB-Miner/LICENSE). Sinta-se livre para usar, estudar, modificar e compartilhar!
