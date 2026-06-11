/*
 * ESP32 #1 - No sensor (caixa d'agua, Tanque Superior)
 * Funcao: le o nivel da caixa com o sensor JSN-SR04T e envia via ESP-NOW
 *         ao gateway. Sem WiFi necessario neste ESP32.
 *
 * As 4 entradas do quadro/inversor da bomba (Bomba ligou / Bomba falhou /
 * Falha no inversor / Painel sem energia) NAO ficam neste no — sao lidas
 * pelo ESP32 do Tanque Inferior (que fica na casa de bombas, com WiFi).
 *
 * Bibliotecas: apenas ESP32 core (nenhuma adicional necessaria)
 */

#include <esp_now.h>
#include <WiFi.h>
#include "config.h"

// Estrutura de dados enviada via ESP-NOW (identica no gateway!)
typedef struct {
    float    distancia;
    int      nivel;
    unsigned long uptime;
    bool     leituraValida;
} DadosSensor;

DadosSensor dados;
unsigned long ultimaMedicao = 0;

void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    bool ok = (status == ESP_NOW_SEND_SUCCESS);
    Serial.println(ok ? "[ESP-NOW] Enviado com sucesso" : "[ESP-NOW] Falha no envio");
    digitalWrite(LED_PIN, HIGH); delay(80);
    digitalWrite(LED_PIN, LOW);
}

float medirMediaCm(int amostras = 5) {
    float soma  = 0;
    int validas = 0;
    for (int i = 0; i < amostras; i++) {
        digitalWrite(TRIG_PIN, LOW);
        delayMicroseconds(2);
        digitalWrite(TRIG_PIN, HIGH);
        delayMicroseconds(10);
        digitalWrite(TRIG_PIN, LOW);
        long dur = pulseIn(ECHO_PIN, HIGH, 30000);
        if (dur > 0) {
            float d = dur * 0.034f / 2.0f;
            if (d > 0 && d < 400) { soma += d; validas++; }
        }
        delay(100);
    }
    return validas > 0 ? soma / validas : -1;
}

int distParaPorcento(float dist) {
    long p = map((long)dist, DIST_CAIXA_VAZIA, DIST_CAIXA_CHEIA, 0, 100);
    return (int)constrain(p, 0, 100);
}

void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_PIN,  OUTPUT);

    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    Serial.print("[SENSOR] MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ERRO na inicializacao!");
        return;
    }
    esp_now_register_send_cb(onDataSent);

    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, GATEWAY_MAC, 6);
    peer.channel = 0;
    peer.encrypt = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ESP-NOW] ERRO ao registrar gateway!");
        return;
    }

    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_PIN, HIGH); delay(200);
        digitalWrite(LED_PIN, LOW);  delay(200);
    }
    Serial.println("[SETUP] Sensor pronto!");
}

void loop() {
    unsigned long agora = millis();

    if (agora - ultimaMedicao >= INTERVALO_MEDICAO_MS) {
        ultimaMedicao = agora;

        float dist = medirMediaCm();
        dados.leituraValida = (dist > 0 && dist < 400);
        dados.distancia     = dist;
        dados.nivel         = dados.leituraValida ? distParaPorcento(dist) : 0;
        dados.uptime        = agora / 1000;

        if (dados.leituraValida) {
            Serial.printf("[NIVEL] %.1fcm -> %d%%\n", dist, dados.nivel);
        } else {
            Serial.println("[SENSOR] Leitura invalida!");
        }

        esp_now_send(GATEWAY_MAC, (uint8_t *)&dados, sizeof(dados));
    }
}
