#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID      "NomeDaSuaRede"
#define WIFI_PASSWORD  "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── RS485 (MAX485) ────────────────────────────────
//   CIE D+ → MAX485 A    MAX485 RO → GPIO16    RE+DE → GND
#define PIN_RX      16
#define PIN_TX      17
#define BAUD_RS485  9600

// ── Cooldown ──────────────────────────────────────
#define COOLDOWN_MS   60000UL

// ── MODO DIAGNÓSTICO ──────────────────────────────
// true  → imprime só frames com byte[36]=1 (sem Telegram).
//         Use para descobrir o b28 de novos dispositivos.
// false → operação normal.
#define DIAG_MODE  false

// ── Mapa de dispositivos ──────────────────────────
// Byte de alarme confirmado em campo: byte[36] = 1 quando dispositivo ativa.
// O campo `laco` = byte[28] capturado no frame de alarme.
//
// Endereços confirmados nesta instalação:
//   b28=0x30 → botoeira que gerou [ALARME] b28=0x30 b29=0x1E
//   b28=0x31 → botoeira que gerou [ALARME] b28=0x31 b29=0x21
//
// Eventos de supervisão automática (NÃO são botoeiras — ignorados):
//   b28=0x27, b28=0x29 → aparecem no boot, sem ação do usuário
//
// Renomeie os dispositivos abaixo conforme a instalação real.

struct Dispositivo {
    uint8_t     laco;   // byte[28] do frame de alarme
    const char* nome;
};

const Dispositivo DISPOSITIVOS[] = {
    { 0x30, "BOTOEIRA PAV T\xC3\x89RREO"    },
    { 0x31, "BOTOEIRA PAV SUPERIOR"          },
    { 0x00, nullptr }   // sentinel – não remova
};
