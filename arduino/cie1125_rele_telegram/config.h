#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID      "NomeDaSuaRede"
#define WIFI_PASSWORD  "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── Debounce ──────────────────────────────────────
// Tempo mínimo que o relé precisa ficar no novo estado
// antes de ser considerado real (evita falsas leituras)
#define DEBOUNCE_MS  100

// ── Cooldown (somente para FALHA e NORMAL) ────────
// Alarmes SEMPRE notificam. Falhas/normais: 1 notif. por minuto.
#define COOLDOWN_MS  60000UL

// ── Relés monitorados ─────────────────────────────
// Cada relé da CIE 1125 liga num GPIO do ESP32 via optoacoplador PC817.
// O pino é configurado como INPUT_PULLUP:
//   relé FECHADO (alarme) → GPIO lê LOW
//   relé ABERTO  (normal) → GPIO lê HIGH
//
// Adicione ou remova linhas conforme os relés disponíveis na sua central.
// A CIE 1125 possui relés de: Alarme Geral, Falha Geral e Supervisão.
// Consulte o manual para os terminais exatos.

struct ReleConfig {
    uint8_t     pino;       // GPIO do ESP32
    const char* nome;       // descrição que aparece no Telegram
    const char* tipo;       // "ALARME", "FALHA" ou "SUPERVISAO"
};

const ReleConfig RELES[] = {
    {  4, "ALARME GERAL",     "ALARME"    },  // Relé de alarme da CIE 1125
    {  5, "FALHA GERAL",      "FALHA"     },  // Relé de falha da CIE 1125
    { 18, "SUPERVISAO",       "SUPERVISAO"},  // Relé de supervisão (se houver)
    // Adicione mais GPIOs se usar relés de zona adicionais:
    // { 19, "ZONA 1", "ALARME" },
    // { 21, "ZONA 2", "ALARME" },
    {  0, nullptr, nullptr }                  // sentinel – não remover
};
