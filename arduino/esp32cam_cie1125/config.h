#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID      "NomeDaSuaRede"
#define WIFI_PASSWORD  "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── Relê de alarme (contato seco NA) ──────────────
//   CIE 1125 relê NA  → GPIO13
//   CIE 1125 relê COM → GND
#define PIN_RELE        13

// ── Comportamento ─────────────────────────────────
#define COOLDOWN_MS      10000UL  // mínimo entre fotos (ms) — aumente para 60000 em produção
#define DEBOUNCE_MS        300    // aguarda estabilização do relê (ms)

// ── Flash LED ─────────────────────────────────────
// false = desligado (recomendado se o LCD tem backlight suficiente)
// true  = liga flash branco (GPIO4) durante a captura
#define FLASH_LIGADO    true   // liga flash branco durante captura

// ── Resolução e qualidade JPEG ────────────────────
// FRAMESIZE_UXGA  = 1600×1200 (máxima — melhor para texto)
// FRAMESIZE_SVGA  = 800×600
// FRAMESIZE_VGA   = 640×480
#define CAM_FRAMESIZE   FRAMESIZE_SXGA
#define CAM_QUALIDADE   12   // 0–63: menor = melhor qualidade JPEG
