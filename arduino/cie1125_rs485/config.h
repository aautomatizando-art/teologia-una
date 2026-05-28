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

// ── MODO DIAGNÓSTICO ──────────────────────────────
// true  → imprime bytes-chave de cada frame no Monitor Serial.
//         Acione uma botoeira e veja quais bytes mudam.
//         Não envia notificações ao Telegram durante o diagnóstico.
// false → operação normal (após entender o protocolo).
#define DIAG_MODE  true

// ── Mapa de dispositivos ──────────────────────────
// Preencha APÓS rodar o DIAG_MODE e identificar o byte correto.
// O campo `laco` corresponde ao byte[28] do frame quando o alarme ocorre.
//
// Exemplo (ajuste com os valores reais do DIAG):
//   { 0x1C, "BOTOEIRA PAV TÉRREO" }

struct Dispositivo {
    uint8_t     laco;   // byte[28] do frame RS485 no momento do alarme
    const char* nome;
};

const Dispositivo DISPOSITIVOS[] = {
    { 0x1C, "BOTOEIRA PAV T\xC3\x89RREO"    },   // ajuste após DIAG
    { 0x00, nullptr }   // sentinel – não remova
};
