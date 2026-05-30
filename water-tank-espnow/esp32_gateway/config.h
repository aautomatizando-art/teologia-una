#ifndef CONFIG_H
#define CONFIG_H

// ─── WIFI ────────────────────────────────────────────────────────────────────────
#define WIFI_SSID  "SUA_REDE_WIFI"
#define WIFI_PASS  "SUA_SENHA_WIFI"

// ─── TELEGRAM ─────────────────────────────────────────────────────────────────
// 1. Crie o bot em @BotFather e copie o token
// 2. Obtenha seu Chat ID em @userinfobot
#define BOT_TOKEN  "SEU_TOKEN_AQUI"
#define CHAT_ID    "SEU_CHAT_ID_AQUI"

// ─── LIMITES DE ALERTA ──────────────────────────────────────────────────────────
// Percentuais de nivel para envio de alertas no Telegram
#define NIVEL_ALERTA  20   // % abaixo disto -> envia alerta
#define NIVEL_OK      80   // % acima disto  -> envia aviso de caixa cheia

// ─── INTERVALO DE STATUS ────────────────────────────────────────────────────────
#define INTERVALO_STATUS_MS  300000UL   // envia status a cada 5 minutos

// ─── TIMEOUT SEM DADOS ──────────────────────────────────────────────────────────
// Alerta se o sensor ficar mais que X ms sem enviar dados
#define TIMEOUT_SENSOR_MS  120000UL   // 2 minutos sem dados = alerta

#endif
