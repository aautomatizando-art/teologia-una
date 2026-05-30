#ifndef CONFIG_H
#define CONFIG_H

// ─── REDE GSM ───────────────────────────────────────────────
// Descomente o APN da sua operadora:
#define APN "claro.com.br"          // Claro
// #define APN "timbrasil.com.br"   // TIM
// #define APN "zap.vivo.com.br"    // Vivo
// #define APN "gprs.oi.com.br"     // Oi
#define APN_USER ""
#define APN_PASS ""

// ─── TELEGRAM ───────────────────────────────────────────────
// 1. Crie o bot em @BotFather e copie o token
// 2. Obtenha seu Chat ID em @userinfobot
#define BOT_TOKEN  "SEU_TOKEN_AQUI"
#define CHAT_ID    "SEU_CHAT_ID_AQUI"

// ─── PINOS ESP32 ────────────────────────────────────────────
#define SIM800_TX_PIN   17   // ESP32 TX → SIM800L RX
#define SIM800_RX_PIN   16   // ESP32 RX → SIM800L TX
#define SIM800_RST_PIN   5   // Reset SIM800L (opcional)

#define TRIG_PIN        32   // Sensor ultrassonico TRIG
#define ECHO_PIN        33   // Sensor ultrassonico ECHO
#define RELAY_PIN       25   // Rele bomba d'agua
#define LED_STATUS      2    // LED onboard (status)

// ─── CAIXA D'AGUA ───────────────────────────────────────────
// Ajuste conforme a altura da sua caixa:
// Distancia sensor → superficie da agua (cm)
#define DIST_CAIXA_VAZIA   90   // cm: caixa quase vazia → liga bomba
#define DIST_CAIXA_CHEIA   15   // cm: caixa cheia → desliga bomba

// ─── INTERVALOS ─────────────────────────────────────────────
#define INTERVALO_MEDICAO_MS   60000UL   // mede a cada 60 segundos
#define INTERVALO_STATUS_MS   300000UL   // envia status a cada 5 minutos

#endif // CONFIG_H
