# Controle de Caixa d'Agua via ESP-NOW + WiFi + WhatsApp (Evolution API)

Sistema de dois ESP32 para monitorar e controlar o nivel de uma caixa d'agua.
Comunicacao local via **ESP-NOW** (sem WiFi no ponto da caixa) e alertas em um
**grupo do WhatsApp** atraves da **Evolution API** rodando em VPS (Docker).

## Arquitetura

```
[Caixa d'agua - sem WiFi]        [100m - com WiFi]              [VPS Hostinger]

 ESP32 #1 (Sensor)                ESP32 #2 (Gateway)             Evolution API
 +--------------------+           +------------------+           +--------------+
 | JSN-SR04T          |           | WiFi             |   HTTP    | Docker :8080 |
 | SAIDA 1: Bomba     |  ESP-NOW  | HTTPClient       | --------> | evolution_api|---> Grupo
 | SAIDA 2: Falha     | --------> | JSON p/ Evo API  |           | postgres     |     WhatsApp
 +--------------------+           +------------------+           | redis        |
                                                                 +--------------+
```

## Saidas do ESP32 Sensor

| Saida   | GPIO | Funcao                                                        |
|---------|------|---------------------------------------------------------------|
| SAIDA 1 | 25   | Rele da bomba ("Bomba Ligada")                                |
| SAIDA 2 | 26   | Falha na bomba (ligada por 10min sem o nivel subir 3cm)       |

**Logica da SAIDA 2 (Falha na Bomba):**
Se a bomba ficar ligada por `FALHA_TEMPO_MS` (10 min) e o nivel nao subir pelo
menos `FALHA_DELTA_CM` (3cm), o sistema considera falha (bomba queimada, poco
seco, registro fechado), aciona a SAIDA 2 e desliga a bomba por seguranca.

## Alertas no Grupo do WhatsApp

| Evento                                  | Mensagem                                   |
|-----------------------------------------|--------------------------------------------|
| Nivel baixo + Saida 1 acionada          | "Nivel baixo! Bomba LIGADA" + nivel/dist  |
| Saida 2 acionada (Falha na Bomba)       | "FALHA NA BOMBA!" + causas possiveis      |
| Caixa abastecida (>= 80%)               | "Caixa abastecida! Bomba DESLIGADA"       |
| Sensor sem comunicacao (2 min)          | "Sensor sem comunicacao!"                 |
| Leitura invalida do sensor              | "Sensor com leitura invalida!"            |
| Gateway ligado                          | "Monitor iniciado!"                       |

## Hardware

| Item               | Qtd | Uso                          |
|--------------------|-----|------------------------------|
| ESP32 DevKit       | 2   | Sensor + Gateway             |
| JSN-SR04T          | 1   | Sensor de nivel              |
| Modulo rele 5V     | 1   | SAIDA 1 - bomba              |
| LED/sirene (opc.)  | 1   | SAIDA 2 - sinalizacao falha  |
| Fonte 5V 1A        | 2   | Uma para cada ESP32          |

## Configuracao da Evolution API (VPS)

### 1. Descobrir o ID do grupo do WhatsApp

No terminal do VPS:

```bash
curl -H "apikey: SUA_APIKEY" \
  "http://localhost:8080/group/fetchAllGroups/SUA_INSTANCIA?getParticipants=false"
```

Procure o grupo pelo `subject` (nome) e copie o `id` — formato `120363xxxxxxxxxxx@g.us`.

> A API key e o valor de `AUTHENTICATION_API_KEY` no docker-compose/.env da Evolution API.
> A instancia e a mesma usada no seu projeto do leitor biometrico.

### 2. Liberar a porta 8080 no firewall do VPS (se ainda nao estiver)

Painel Hostinger > Regras de firewall > permitir TCP 8080
(ou restrinja a porta apenas ao IP da sua internet local, mais seguro).

### 3. Testar envio manual

```bash
curl -X POST "http://SEU_IP_VPS:8080/message/sendText/SUA_INSTANCIA" \
  -H "Content-Type: application/json" \
  -H "apikey: SUA_APIKEY" \
  -d '{"number": "120363xxxxxxxxxxx@g.us", "text": "Teste monitor caixa d agua"}'
```

Se a mensagem chegar no grupo, a API esta pronta.

## Ordem de Configuracao dos ESP32

### Passo 1 - Obter o MAC do Gateway (ESP32 #2)

1. Grave `get_mac_address/get_mac_address.ino` no **ESP32 #2**
2. Serial Monitor (115200) exibe:
```
MAC: AA:BB:CC:DD:EE:FF
Para o config.h: {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
```

### Passo 2 - Configurar o Sensor (ESP32 #1)

`esp32_sensor/config.h`:
```cpp
uint8_t GATEWAY_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // MAC do gateway
#define DIST_CAIXA_VAZIA  90   // calibre para sua caixa
#define DIST_CAIXA_CHEIA  15
```

### Passo 3 - Configurar o Gateway (ESP32 #2)

`esp32_gateway/config.h`:
```cpp
#define WIFI_SSID      "sua_rede"
#define WIFI_PASS      "senha"
#define EVO_BASE_URL   "http://SEU_IP_VPS:8080"
#define EVO_INSTANCE   "sua_instancia"
#define EVO_APIKEY     "sua_apikey"
#define WHATS_GROUP_ID "120363xxxxxxxxxxx@g.us"
```

### Passo 4 - Gravar e verificar

1. Grave `esp32_gateway.ino` no ESP32 #2 — deve enviar "Monitor iniciado!" no grupo
2. Grave `esp32_sensor.ino` no ESP32 #1 — Serial deve mostrar "[ESP-NOW] Enviado com sucesso"

## Esquema de Ligacao (ESP32 #1 - Sensor)

```
ESP32 #1       JSN-SR04T
  GPIO32  ----> TRIG
  GPIO33  <---- ECHO
  5V      ----> VCC
  GND     ----> GND

ESP32 #1       Rele (SAIDA 1 - Bomba)
  GPIO25  ----> IN
  5V      ----> VCC
  GND     ----> GND

ESP32 #1       SAIDA 2 (Falha - LED/sirene, opcional)
  GPIO26  ----> IN do segundo rele ou LED + resistor 220R

Rele 1:
  COM  ----> Fase da bomba
  NO   ----> Fase da rede eletrica
```

## Calibracao do Sensor

```
[JSN-SR04T na tampa da caixa]
   |  <- 15cm  DIST_CAIXA_CHEIA  -> desliga bomba, avisa grupo
   |
   ~  nivel da agua
   |
   |  <- 90cm  DIST_CAIXA_VAZIA  -> liga bomba (SAIDA 1), alerta grupo
   |
[Fundo da caixa]
```

## Dicas de Alcance ESP-NOW (100m)

- Em campo aberto: 200-400m — seus 100m estao dentro do alcance
- Posicione o gateway em janela/area aberta voltada para a caixa
- Evite obstaculos metalicos na linha de visada
- Se precisar de mais alcance: ESP32 com antena externa (conector U.FL)
