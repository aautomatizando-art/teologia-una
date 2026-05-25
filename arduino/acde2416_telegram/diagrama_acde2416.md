# Diagrama – ASCAEL ACDE 24/16 → ESP32 → Telegram

## Ligação física

```
┌──────────────────┐  RS232 DB9   ┌──────────┐  GPIO16  ┌──────────┐
│  ACDE 24/16      │─────────────►│ MAX3232  │─────────►│  ESP32   │──► Telegram
│  DB9 pino 2 (TX) │              │ (3.3V)   │          │          │
│  DB9 pino 5 (GND)│──────────────│ GND      │          │          │
└──────────────────┘              └──────────┘          └──────────┘
```

> **ATENÇÃO:** Use MAX3232 (3.3V), NÃO MAX232 (5V).
> O ESP32 opera em 3.3V — tensão de 5V destrói o pino GPIO.

---

## Tabela de conexões

| ACDE 24/16 DB9 | MAX3232 | ESP32 |
|----------------|---------|-------|
| Pino 2 – TX    | R1IN    | —     |
| Pino 5 – GND   | GND     | GND   |
| —              | R1OUT   | GPIO16 (RX2) |
| —              | VCC     | 3.3V  |
| —              | GND     | GND   |

*Pino 3 (RX da central) não é necessário se só quisermos receber eventos.*

---

## Passo 1 — Descobrir o formato da central (RAW_MODE)

A ASCAEL ACDE 24/16 envia eventos em ASCII via RS232, mas o formato
exato (ordem dos campos, separadores) varia conforme a versão do firmware.
Use o RAW_MODE para capturar a saída real:

### 1. Configure `config.h`
```cpp
#define BAUD_CENTRAL  9600   // tente também: 2400, 4800, 19200
#define RAW_MODE      true
```

### 2. Grave no ESP32 e abra o Monitor Serial (115200 baud)

### 3. Acione uma botoeira ou gere um evento na central

### 4. Observe a saída no Monitor Serial — você verá algo como:
```
[RAW] 01 ALARME INCENDIO 14:32:10
      HEX: 30 31 20 41 4C 41 52 4D 45 20 49 4E 43 45 4E 44 49 4F 20 31 34 3A 33 32 3A 31 30
```

### 5. Copie a saída e informe para ajustar o parser em `acde2416_telegram.ino`

### 6. Após ajustar, mude `RAW_MODE false` e regrave

---

## Passo 2 — Configurar zonas (opcional)

Em `config.h`, preencha o array `ZONAS[]` com os nomes das zonas:

```cpp
const ZonaInfo ZONAS[] = {
    {  1, "ZONA 1 – PAV TÉRREO"    },
    {  2, "ZONA 2 – PAV SUPERIOR"  },
    {  3, "ZONA 3 – SUBSOLO"       },
    {  0, nullptr }   // sentinel
};
```

---

## Mensagens no Telegram

```
🔥 ALARME DE INCÊNDIO

📍 Zona: 01 — ZONA 1 – PAV TÉRREO
📋 01 ALARME INCENDIO 14:32:10
🕐 14:32:10

Central: ASCAEL ACDE 24/16
```

```
✅ NORMALIZADO

📍 Zona: 01 — ZONA 1 – PAV TÉRREO
📋 01 NORMAL 14:35:00
🕐 14:35:00

Central: ASCAEL ACDE 24/16
```

```
⚠️ FALHA

📍 Zona: 02 — ZONA 2 – PAV SUPERIOR
📋 02 FALHA SUPERVISAO 14:40:00

Central: ASCAEL ACDE 24/16
```

---

## Baud rates para testar

Se RAW_MODE não exibir nada, tente cada baud rate em `config.h`:

| Baud | Comando |
|------|---------|
| 9600 | `#define BAUD_CENTRAL 9600` |
| 4800 | `#define BAUD_CENTRAL 4800` |
| 2400 | `#define BAUD_CENTRAL 2400` |
| 19200 | `#define BAUD_CENTRAL 19200` |

---

## Componentes necessários

| Item | Qtd | Observação |
|------|-----|------------|
| ESP32 (DevKit) | 1 | qualquer modelo com 38 ou 30 pinos |
| MAX3232 (módulo) | 1 | **NÃO** usar MAX232 (5V) |
| Cabo DB9 macho | 1 | para conectar na saída RS232 da central |
| Jumpers / fios | 4 | VCC, GND, TX→R1IN, R1OUT→GPIO16 |

---

## Bibliotecas necessárias (Arduino IDE)

| Biblioteca | Instalação |
|------------|------------|
| ArduinoJson (Benoit Blanchon) | Gerenciar Bibliotecas |
| WiFi, WiFiClientSecure, HTTPClient | Incluso no core ESP32 |
