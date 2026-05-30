#ifndef CONFIG_H
#define CONFIG_H

// ─── REDE GSM ─────────────────────────────────────────────────────────────────
// Operadora: Algar Telecom M2M/IoT
#define APN      "m2m.algar.com.br"
#define APN_USER ""
#define APN_PASS ""
//
// Caso nao conecte, tente as alternativas abaixo (uma por vez):
// #define APN "algar.com.br"
// (confirme o APN exato no portal Algar M2M ou na documentacao do chip)

// ─── TELEGRAM ─────────────────────────────────────────────────────────────────
// 1. Crie o bot em @BotFather e copie o token
// 2. Obtenha seu Chat ID em @userinfobot
#define BOT_TOKEN  "SEU_TOKEN_AQUI"
#define CHAT_ID    "SEU_CHAT_ID_AQUI"

// ─── PINOS ESP32 + SIM800L ────────────────────────────────────────────────────
// ATENCAO: SIM800L usa Micro SIM — quebre o chip Algar no tamanho do meio
#define SIM800_TX_PIN   17   // ESP32 TX -> SIM800L RXD
#define SIM800_RX_PIN   16   // ESP32 RX <- SIM800L TXD
#define SIM800_RST_PIN   5   // Reset SIM800L (opcional)

#define TRIG_PIN        32   // Sensor ultrassonico TRIG (JSN-SR04T)
#define ECHO_PIN        33   // Sensor ultrassonico ECHO (JSN-SR04T)
#define RELAY_PIN       25   // Rele bomba d'agua
#define LED_STATUS       2   // LED onboard (status do sistema)

// ─── CAIXA D'AGUA ─────────────────────────────────────────────────────────────
// Distancia em cm do sensor ate a superficie da agua.
// Meca na sua caixa e ajuste os valores:
//
//   [Sensor na tampa]
//      |  <- DIST_CAIXA_CHEIA  (ex: 15cm)  => caixa cheia, desliga bomba
//      |
//      |  <- DIST_CAIXA_VAZIA  (ex: 90cm)  => caixa vazia, liga bomba + alerta
//      |
//   [Fundo da caixa]
//
#define DIST_CAIXA_VAZIA   90   // cm: nivel baixo -> liga bomba + alerta Telegram
#define DIST_CAIXA_CHEIA   15   // cm: nivel ok    -> desliga bomba + aviso Telegram

// ─── INTERVALOS ───────────────────────────────────────────────────────────────
#define INTERVALO_MEDICAO_MS   60000UL    // mede nivel a cada 60 segundos
#define INTERVALO_STATUS_MS   300000UL   // envia status a cada 5 minutos

#endif // CONFIG_H
