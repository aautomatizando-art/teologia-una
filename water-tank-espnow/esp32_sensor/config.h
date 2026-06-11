#ifndef CONFIG_H
#define CONFIG_H

// ─── MAC DO GATEWAY (ESP32 #2) ────────────────────────────────────────
// Grave get_mac_address.ino no ESP32 #2 e copie o MAC exibido no Serial Monitor
// Exemplo: {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
uint8_t GATEWAY_MAC[] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF}; // <- substitua!

// ─── PINOS ESP32 SENSOR ─────────────────────────────────────────────
#define TRIG_PIN        32   // JSN-SR04T TRIG
#define ECHO_PIN        33   // JSN-SR04T ECHO
#define LED_PIN          2   // LED onboard (pisca ao enviar)

// ─── CALIBRACAO DA CAIXA ─────────────────────────────────────────────
// Distancia (cm) do sensor ate a superficie da agua:
//
//  [Sensor na tampa]
//     |  <- DIST_CAIXA_CHEIA (15cm) => caixa cheia (100%)
//     |
//     ~  nivel da agua
//     |
//     |  <- DIST_CAIXA_VAZIA (90cm) => caixa vazia (0%)
//     |
//  [Fundo]
//
#define DIST_CAIXA_VAZIA  90   // cm: nivel baixo (caixa vazia)
#define DIST_CAIXA_CHEIA  15   // cm: nivel cheio (caixa cheia)

// ─── INTERVALO DE MEDICAO ─────────────────────────────────────────────
#define INTERVALO_MEDICAO_MS  30000UL   // mede e envia a cada 30 segundos

#endif
