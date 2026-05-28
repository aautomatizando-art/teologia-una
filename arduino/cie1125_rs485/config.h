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
#define COOLDOWN_MS   60000UL   // intervalo mín. entre msgs do mesmo evento

// ── Localização geral ─────────────────────────────
#define NOME_LOCAL    "EDIF\xC3\x8DCIO"   // aparece nas mensagens de alarme sem device

// ── Mapa de dispositivos ──────────────────────────
// Preencha com os valores confirmados pelo cie1125_rs485_raw:
//   byte[28] = Laço     (hex, ex: 0x04)
//   byte[29] = Endereço (hex, ex: 0x04)
//   nome     = texto que aparece na notificação Telegram
//
// Valores confirmados em campo:
//   Botoeira Térreo  → Laço 0x04, End. 0x04
//   Botoeira Superior→ Laço 0x05, End. 0x29

struct Dispositivo {
    uint8_t     laco;
    uint8_t     endereco;
    const char* nome;
};

const Dispositivo DISPOSITIVOS[] = {
    { 0x04, 0x04, "BOTOEIRA PAV T\xC3\x89RREO"    },
    { 0x05, 0x29, "BOTOEIRA PAV SUPERIOR"          },
    { 0x00, 0x00, nullptr }   // sentinel – não remova
};
