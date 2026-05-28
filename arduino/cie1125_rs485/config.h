#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID      "NomeDaSuaRede"
#define WIFI_PASSWORD  "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── RS485 (MAX485) ────────────────────────────────
//   CIE D+ → MAX485 A
//   CIE D- → MAX485 B
//   MAX485 RO    → ESP32 GPIO16
//   MAX485 RE+DE → GND  (recepção permanente)
#define PIN_RX      16
#define PIN_TX      17
#define BAUD_RS485  9600

// ── Cooldown ──────────────────────────────────────
// Intervalo mínimo entre mensagens do mesmo Laço.
#define COOLDOWN_MS   60000UL

// ── Mapa de dispositivos ──────────────────────────
// Identificação pelo LAÇO (byte[28]) — o End address (byte[29])
// varia conforme o dispositivo sendo polled no momento do alarme.
//
// Laços confirmados em campo:
//   Botoeira Térreo   → Laço 0x04
//   Botoeira Superior → Laço 0x05
//
// Adicione outros laços conforme necessário.

struct Dispositivo {
    uint8_t     laco;   // byte[28] do frame RS485
    const char* nome;   // texto que aparece no Telegram
};

const Dispositivo DISPOSITIVOS[] = {
    { 0x04, "BOTOEIRA PAV T\xC3\x89RREO"    },
    { 0x05, "BOTOEIRA PAV SUPERIOR"          },
    { 0x00, nullptr }   // sentinel – não remova
};
