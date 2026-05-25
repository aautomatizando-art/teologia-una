#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID     "NomeDaSuaRede"
#define WIFI_PASSWORD "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
// 1. Abra o Telegram e procure por @BotFather
// 2. Digite /newbot e siga as instruções
// 3. Cole o token abaixo (formato: 123456789:ABCdef...)
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"

// Chat ID do grupo (número negativo para grupos, ex: -1001234567890)
// Para descobrir: adicione @userinfobot ao grupo e ele responde com o ID
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── Serial da Central ─────────────────────────────
#define BAUD_CENTRAL  9600   // ajuste se necessário (4800, 19200)
#define PIN_RX2       16     // GPIO16 do ESP32 (RX2) → MAX3232 R1OUT
#define PIN_TX2       17     // GPIO17 (não usado, mas obrigatório)

// ── Comportamento ─────────────────────────────────
// Tempo mínimo entre mensagens do mesmo endereço (ms)
#define COOLDOWN_MS   60000UL   // 60 segundos

// Enviar mensagem também para eventos NORMAL (normalização)?
#define ENVIAR_NORMAL true

// Enviar mensagem para FALHA?
#define ENVIAR_FALHA  true
