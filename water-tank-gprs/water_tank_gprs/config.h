#ifndef CONFIG_H
#define CONFIG_H

// ─── PLACA ────────────────────────────────────────────────────────────────────
// LilyGO T-SIM7000G (ESP32 + SIM7000G integrados na mesma placa)

// ─── REDE ─────────────────────────────────────────────────────────────────────
// Chip: Algar Telecom M2M/IoT
#define APN      "m2m.algar.com.br"
#define APN_USER ""
#define APN_PASS ""
// Alternativa caso nao conecte:
// #define APN "algar.com.br"

// Modo de rede preferido:
// 2  = NB-IoT apenas      (melhor cobertura, baixo consumo)
// 3  = GPRS apenas        (fallback 2G)
// 38 = LTE-M apenas
// 51 = NB-IoT + LTE-M    (automatico, recomendado)
#define NETWORK_MODE 51

// ─── TELEGRAM ─────────────────────────────────────────────────────────────────
// 1. Crie o bot em @BotFather e copie o token
// 2. Obtenha seu Chat ID em @userinfobot
#define BOT_TOKEN  "SEU_TOKEN_AQUI"
#define CHAT_ID    "SEU_CHAT_ID_AQUI"

// ─── PINOS LilyGO T-SIM7000G ──────────────────────────────────────────────────
// Modem (internos da placa, nao altere)
#define SIM7000_TX_PIN    27
#define SIM7000_RX_PIN    26
#define SIM7000_PWR_PIN    4

// Perifericos externos (pode alterar conforme sua fiacao)
#define TRIG_PIN          32   // Sensor JSN-SR04T TRIG
#define ECHO_PIN          33   // Sensor JSN-SR04T ECHO
#define RELAY_PIN         25   // Rele bomba d'agua
#define LED_STATUS        12   // LED de status

// ─── CALIBRACAO DA CAIXA D'AGUA ───────────────────────────────────────────────
// Distancia (cm) do sensor ate a superficie da agua:
//
//  [Sensor na tampa]
//     |  <- DIST_CAIXA_CHEIA (15cm)  caixa cheia  -> desliga bomba
//     |
//     ~  nivel da agua
//     |
//     |  <- DIST_CAIXA_VAZIA (90cm)  caixa vazia  -> liga bomba + alerta
//     |
//  [Fundo]
//
#define DIST_CAIXA_VAZIA   90   // cm: nivel baixo -> liga bomba + alerta Telegram
#define DIST_CAIXA_CHEIA   15   // cm: nivel ok    -> desliga bomba + aviso Telegram

// ─── INTERVALOS ───────────────────────────────────────────────────────────────
#define INTERVALO_MEDICAO_MS    60000UL   // mede nivel a cada 60 segundos
#define INTERVALO_STATUS_MS    300000UL   // envia status a cada 5 minutos

#endif // CONFIG_H
