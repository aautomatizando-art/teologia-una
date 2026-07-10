/*
 * Bateria Eletronica Profissional - ESP32-S3 (N16R8: 16MB flash, 8MB PSRAM)
 * ------------------------------------------------------------------------
 * Le os golpes dos pads piezo (bumbo, caixa, toms, pratos e chimbal com
 * pedal), calcula a velocidade de cada golpe (algoritmo scan+mask, igual
 * aos modulos de bateria comerciais) e toca amostras reais de bateria
 * acustica (multi-camadas de velocidade) via I2S, com polifonia total.
 * Tambem envia MIDI (DIN 5 pinos) para uso com modulos ou DAW/VST externos.
 *
 * Requisitos de hardware: ver README.md (pasta bateria-eletronica/).
 * IMPORTANTE: selecione a placa "ESP32S3 Dev Module" na Arduino IDE e
 * habilite "PSRAM: OPI PSRAM" (a variante N16R8 usa PSRAM Octal -- se
 * deixar em "QSPI PSRAM" a PSRAM nao e detectada). As amostras de audio
 * sao carregadas inteiras na PSRAM para tocar sem falhas/latencia.
 *
 * Bibliotecas: apenas ESP32 core (SD, SPI e I2S ja vem inclusos)
 */

#include "config.h"
#include "pads.h"
#include "audio_engine.h"
#include "midi_out.h"
#include <SD.h>
#include <SPI.h>

static TaskHandle_t padsTaskHandle  = nullptr;
static TaskHandle_t audioTaskHandle = nullptr;

void setup() {
    Serial.begin(115200);
    delay(200);
    Serial.println();
    Serial.println("=== Bateria Eletronica ESP32 ===");

    pinMode(LED_PIN, OUTPUT);

    // ESP32-S3 usa FSPI (o "VSPI" do ESP32 classico nao existe no S3)
    SPIClass spiSD(FSPI);
    spiSD.begin(SD_SCK_PIN, SD_MISO_PIN, SD_MOSI_PIN, SD_CS_PIN);
    if (!SD.begin(SD_CS_PIN, spiSD)) {
        Serial.println("[SD] ERRO: cartao nao encontrado! Verifique a fiacao e");
        Serial.println("     se o cartao esta formatado em FAT32.");
    } else {
        Serial.println("[SD] Cartao montado. Carregando amostras para a PSRAM...");
    }

    audioInitI2S();
    audioLoadSamples();

    padsInit();
    midiInit();

    xTaskCreatePinnedToCore(audioTask, "audio", 8192, NULL, 5, &audioTaskHandle, 1);
    xTaskCreatePinnedToCore(padsTask,  "pads",  4096, NULL, 4, &padsTaskHandle,  0);

    // 3 piscadas = pronto para tocar
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH); delay(80);
        digitalWrite(LED_PIN, LOW);  delay(80);
    }
    Serial.println("[SETUP] Pronto! Toque nos pads.");
}

void loop() {
    // Toda a logica roda nas tasks dedicadas: audio (core 1, prioridade alta,
    // sem jitter) e leitura dos pads + MIDI (core 0). O loop() fica livre.
    vTaskDelay(1000 / portTICK_PERIOD_MS);
}
