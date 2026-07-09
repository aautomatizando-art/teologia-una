/*
 * Teste/Calibracao dos pads piezo - Bateria Eletronica ESP32
 * ------------------------------------------------------------------------
 * Grave este sketch ANTES de montar a bateria eletronica completa.
 * Bata em cada pad (leve, medio e forte) e anote os valores de PICO
 * mostrados no Serial Monitor (115200 baud). Use esses valores para
 * ajustar "threshold" e "peakMax" de cada pad no config.h do firmware
 * principal (esp32_bateria_eletronica).
 *
 * Tambem mostra continuamente o valor do pedal do chimbal, solto (aberto)
 * e pressionado (fechado), para calibrar HIHAT_PEDAL_CLOSED_MAX e
 * HIHAT_PEDAL_OPEN_MIN.
 *
 * Bibliotecas: nenhuma alem do ESP32 core.
 */

#include <Arduino.h>

const int   pinos[]   = {34, 35, 32, 33, 36, 39, 4, 13};
const char *nomes[]   = {"Kick", "Snare", "Tom1", "Tom2", "Tom3", "Crash", "Ride", "HiHat"};
const int   NUM_PADS  = sizeof(pinos) / sizeof(pinos[0]);
const int   PEDAL_PIN = 14;

const uint16_t LIMIAR              = 150;   // limiar generico so para este teste
const uint16_t TEMPO_VARREDURA_MS  = 8;

bool     emVarredura[8];
uint16_t picos[8];
uint32_t inicioVarredura[8];

void setup() {
    Serial.begin(115200);
    delay(200);
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);

    Serial.println();
    Serial.println("=== Teste/Calibracao dos Pads - Bateria Eletronica ===");
    Serial.println("Bata em cada pad (leve/medio/forte) e anote o PICO mostrado.");
    Serial.println("Use os valores para ajustar 'threshold' e 'peakMax' no config.h.");
    Serial.println();
}

void loop() {
    uint32_t agora = millis();

    for (int i = 0; i < NUM_PADS; i++) {
        uint16_t v = analogRead(pinos[i]);

        if (!emVarredura[i]) {
            if (v > LIMIAR) {
                emVarredura[i]     = true;
                picos[i]           = v;
                inicioVarredura[i] = agora;
            }
        } else {
            if (v > picos[i]) picos[i] = v;
            if (agora - inicioVarredura[i] >= TEMPO_VARREDURA_MS) {
                Serial.printf("[%-6s] pico=%4u  (pino %d)\n", nomes[i], picos[i], pinos[i]);
                emVarredura[i] = false;
            }
        }
    }

    static uint32_t ultimoPedal = 0;
    if (agora - ultimoPedal > 500) {
        ultimoPedal = agora;
        Serial.printf(">> Pedal chimbal (pino %d): %d\n", PEDAL_PIN, analogRead(PEDAL_PIN));
    }
}
