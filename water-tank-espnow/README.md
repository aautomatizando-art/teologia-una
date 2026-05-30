# Controle de Caixa d'Agua via ESP-NOW + WiFi + Telegram

Sistema de dois ESP32 para monitorar e controlar o nivel de uma caixa d'agua
sem necessidade de chip SIM ou GPRS. Comunicacao local via **ESP-NOW**.

## Arquitetura

```
[Caixa d'agua - sem WiFi]          [100m de distancia - com WiFi]

 ESP32 #1 (Sensor)                  ESP32 #2 (Gateway)
 +-------------------+              +-------------------+
 | JSN-SR04T         |              | WiFi              |-----> Internet
 | Rele / Bomba      |  ESP-NOW     | Telegram Bot      |
 | Sem WiFi          | -----------> | Alerta no celular |
 +-------------------+              +-------------------+
  Alimentado por fonte               Alimentado por fonte
  ou bateria local                   ou USB
```

**Por que ESP-NOW?**
- Alcance: ate 200-400m em campo aberto
- Sem mensalidade (sem SIM card)
- Consumo baixissimo
- Latencia ~1ms
- Nao precisa de WiFi no ponto do sensor

## Hardware Necessario

| Item               | Quantidade | Uso               |
|--------------------|------------|-------------------|
| ESP32 DevKit       | 2          | Sensor + Gateway  |
| JSN-SR04T          | 1          | Sensor de nivel   |
| Modulo rele 5V     | 1          | Controle da bomba |
| Fonte 5V 1A        | 2          | Um para cada ESP32|

## Ordem de Configuracao

### Passo 1 - Obter o MAC do Gateway (ESP32 #2)

1. Grave `get_mac_address/get_mac_address.ino` no **ESP32 #2**
2. Abra o Serial Monitor (115200 baud)
3. Anote o MAC exibido:
```
MAC: AA:BB:CC:DD:EE:FF
Para o config.h: {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
```

### Passo 2 - Configurar o Sensor (ESP32 #1)

Edite `esp32_sensor/config.h`:

```cpp
// Cole o MAC do Gateway aqui:
uint8_t GATEWAY_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF};

// Calibre para sua caixa (distancia em cm sensor -> agua):
#define DIST_CAIXA_VAZIA  90   // nivel baixo -> liga bomba
#define DIST_CAIXA_CHEIA  15   // nivel ok    -> desliga bomba
```

### Passo 3 - Configurar o Gateway (ESP32 #2)

Edite `esp32_gateway/config.h`:

```cpp
#define WIFI_SSID  "nome_da_sua_rede"
#define WIFI_PASS  "senha_da_rede"
#define BOT_TOKEN  "token_do_bot_telegram"
#define CHAT_ID    "seu_chat_id"
```

### Passo 4 - Gravar os sketches

1. Grave `esp32_gateway.ino` no ESP32 #2 (proximo ao WiFi)
2. Grave `esp32_sensor.ino` no ESP32 #1 (na caixa d'agua)

### Passo 5 - Verificar no Serial Monitor

**ESP32 #2 (Gateway):**
```
[WiFi] Conectado! IP: 192.168.1.x
[WiFi] Canal: 6
[GATEWAY] MAC: AA:BB:CC:DD:EE:FF
[SETUP] Gateway pronto!
```

**ESP32 #1 (Sensor):**
```
[SENSOR] MAC: 11:22:33:44:55:66
[SETUP] Sensor pronto!
[NIVEL] 35.2cm -> 61%
[ESP-NOW] Enviado com sucesso
```

## Esquema de Ligacao (ESP32 #1 - Sensor)

```
ESP32 #1       JSN-SR04T
  GPIO32  ----> TRIG
  GPIO33  <---- ECHO
  5V      ----> VCC
  GND     ----> GND

ESP32 #1       Rele
  GPIO25  ----> IN
  5V      ----> VCC
  GND     ----> GND

Rele:
  COM  ----> Fase da bomba
  NO   ----> Fase da rede eletrica
```

## Mensagens Telegram

| Evento                     | Mensagem                                 |
|----------------------------|------------------------------------------|
| Gateway ligado             | "Gateway iniciado! MAC: ... Canal: ..." |
| Nivel baixo (<= 20%)       | "ALERTA: Caixa baixa! Bomba LIGADA."    |
| Caixa cheia (>= 80%)       | "Caixa abastecida! Bomba DESLIGADA."    |
| Status (a cada 5 min)      | Nivel, bomba, uptime, sinal WiFi        |
| Sensor sem sinal (2 min)   | "Sensor sem comunicacao!"               |
| Leitura invalida           | "Sensor com leitura invalida!"          |

## Calibracao do Sensor

```
[JSN-SR04T na tampa da caixa]
   |  <- 15cm  DIST_CAIXA_CHEIA  -> desliga bomba, avisa Telegram
   |
   ~  nivel da agua
   |
   |  <- 90cm  DIST_CAIXA_VAZIA  -> liga bomba, alerta Telegram
   |
[Fundo da caixa]
```

## Dicas de Alcance ESP-NOW

- Em campo aberto: 200-400m sem obstrucoes
- Com paredes/arvores: 50-150m
- Para aumentar o alcance: use antena externa no ESP32 com conector U.FL
- Posicione o ESP32 #2 (gateway) na janela ou area aberta voltada para a caixa
