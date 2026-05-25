# Diagrama – CAE-500 XMAX → ESP32 → Telegram

## ⚠️ ATENÇÃO: Use MAX3232, não MAX232

O ESP32 opera em **3.3V**. O MAX232 padrão produz saída TTL de 5V,
o que pode danificar os GPIO do ESP32. Use o **MAX3232** (mesma função,
mesmo pinout, mas compatível com 3.3V).

## Ligações

```
CAE-500 XMAX            MAX3232              ESP32
  DB9 pino 2 (TX) ────► R1IN
  DB9 pino 5 (GND) ───► GND ──────────────► GND
                         R1OUT ─────────────► GPIO16 (RX2)
                         VCC ───────────────► 3.3V   ← não use 5V!
```

## Pinos do DB9 na CAE-500 XMAX

| Pino DB9 | Sinal | Conecta em     |
|----------|-------|----------------|
| 2        | TX    | MAX3232 R1IN   |
| 5        | GND   | MAX3232 GND    |
| 3, 7, 8  | —     | Não conectar   |

## Diagrama de blocos

```
┌─────────────┐  RS232   ┌─────────┐  TTL    ┌──────────┐  WiFi  ┌──────────┐
│ CAE-500XMAX │ ±12V     │ MAX3232 │  3.3V   │  ESP32   │ ─────► │ Telegram │
│  DB9 TX     │ ────────►│ R1IN    │ ───────►│ GPIO16   │        │  Grupo   │
└─────────────┘          │ R1OUT   │         │          │        └──────────┘
                         └─────────┘         └──────────┘
```

## Configuração do Bot Telegram (passo a passo)

### 1. Criar o bot
1. Abra o Telegram e pesquise **@BotFather**
2. Envie `/newbot`
3. Escolha um nome e um username (deve terminar em `bot`)
4. Copie o **token** gerado (ex: `123456789:ABCdefGHIjklMNO...`)

### 2. Adicionar o bot ao grupo
1. Abra o grupo do Telegram
2. Vá em **Adicionar Membros** e adicione o seu bot
3. Dê permissão de **enviar mensagens** ao bot

### 3. Descobrir o Chat ID do grupo
- Adicione o bot **@userinfobot** ao grupo
- Ele responderá com o ID do grupo (número negativo, ex: `-1001234567890`)
- Depois remova o @userinfobot

### 4. Preencher o config.h
```cpp
#define TELEGRAM_TOKEN   "123456789:ABCdefGHIjklMNOpqrSTUvwxyz"
#define TELEGRAM_CHAT_ID "-1001234567890"
```

## Bibliotecas necessárias (Arduino IDE)

| Biblioteca     | Autor            | Instalação                          |
|----------------|------------------|-------------------------------------|
| ArduinoJson    | Benoit Blanchon  | Gerenciar Bibliotecas → "ArduinoJson" |
| WiFi, HTTPClient, WiFiClientSecure | — | Inclusas no core ESP32 |

### Configurar o ESP32 no Arduino IDE
1. Arquivo → Preferências → URLs adicionais:
   ```
   https://raw.githubusercontent.com/espressif/arduino-esp32/gh-pages/package_esp32_index.json
   ```
2. Ferramentas → Placa → Gerenciador → buscar **esp32** → instalar

## Exemplo de mensagem no Telegram

```
🔥 ALARME DE INCÊNDIO

📍 Endereço: 042
📋 BOTOEIRA PAV 3
🕐 14:32:05 — 25/05/2026

Central: CAE-500 XMAX
```

```
⚠️ FALHA

📍 Endereço: 007
📋 DETECTOR SALA 1
🕐 14:31:52 — 25/05/2026

Central: CAE-500 XMAX
```

```
✅ NORMALIZADO

📍 Endereço: 042
📋 BOTOEIRA PAV 3
🕐 14:33:00 — 25/05/2026

Central: CAE-500 XMAX
```

## Comportamento do cooldown

- **ALARME**: sem cooldown — sempre notifica
- **FALHA / NORMAL**: notifica no máximo 1x por endereço a cada 60 s
- Ajuste `COOLDOWN_MS` no `config.h` para mudar esse intervalo
