#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID      "NomeDaSuaRede"
#define WIFI_PASSWORD  "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── Serial (pino 39 do PIC18F4520) ───────────────
#define BAUD_CENTRAL  4800    // confirmado
#define PIN_RX2       16      // ESP32 GPIO16 ← pino 39 do PIC (via resistor 1kΩ)
#define PIN_TX2       17      // não usado

// ── Nome da central (aparece nas mensagens) ───────
#define NOME_CENTRAL  "ASCAEL ACDE 24/16"

// ── Localização (aparece nas mensagens) ───────────
// Descreve onde está instalada esta central.
#define NOME_LOCAL    "BOTOEIRA PAV T\xC3\x89RREO"

// ── Cooldown e filtros ────────────────────────────
#define COOLDOWN_MS           60000UL
#define TIMEOUT_NORMAL_MS     30000UL  // 30s sem byte de alarme → normalizado
#define ENVIAR_NORMAL         true     // envia "normalizado" quando alarme cessa

// ── RAW MODE ─────────────────────────────────────
// true  → imprime cada byte no Monitor Serial (diagnóstico)
// false → parser ativo, envia notificações ao Telegram
#define RAW_MODE  false

// ── Bytes de alarme da central ────────────────────
// A saída serial do PIC18F4520 (pino 39) não distingue qual dispositivo
// acionou — qualquer byte diferente de 0x00, 0x02 e 0xFF é alarme.
// Bytes confirmados em campo:
//   0xBF → alarme ativo (qualquer botoeira)
