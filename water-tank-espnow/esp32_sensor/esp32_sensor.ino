/*
 * ESP32 #1 - No sensor (caixa d'agua)
 * Funcao: le o nivel da caixa, le 4 entradas digitais vindas do quadro/inversor
 *         da bomba e envia tudo via ESP-NOW ao gateway
 * Sem WiFi necessario neste ESP32
 *
 * ENTRADA 1 (GPIO27): Bomba ligou
 * ENTRADA 2 (GPIO14): Bomba falhou
 * ENTRADA 3 (GPIO13): Falha no inversor
 * ENTRADA 4 (GPIO4):  Painel sem energia (sem rede CA)
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
    bool     entrada1_bombaLigada;
    bool     entrada2_bombaFalhou;
    bool     entrada3_falhaInversor;
    bool     entrada4_painelSemEnergia;
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

// Le as 4 entradas digitais (contato seco para GND = ativo, INPUT_PULLUP)
void lerEntradas() {
    dados.entrada1_bombaLigada       = (digitalRead(ENTRADA1_PIN) == LOW);
    dados.entrada2_bombaFalhou       = (digitalRead(ENTRADA2_PIN) == LOW);
    dados.entrada3_falhaInversor     = (digitalRead(ENTRADA3_PIN) == LOW);
    dados.entrada4_painelSemEnergia  = (digitalRead(ENTRADA4_PIN) == LOW);
}

void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_PIN,  OUTPUT);

    pinMode(ENTRADA1_PIN, INPUT_PULLUP);
    pinMode(ENTRADA2_PIN, INPUT_PULLUP);
    pinMode(ENTRADA3_PIN, INPUT_PULLUP);
    pinMode(ENTRADA4_PIN, INPUT_PULLUP);

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

        lerEntradas();

        if (dados.leituraValida) {
            Serial.printf("[NIVEL] %.1fcm -> %d%% | E1(Bomba ligou): %d | E2(Bomba falhou): %d | E3(Falha inversor): %d | E4(Painel sem energia): %d\n",
                dist, dados.nivel,
                dados.entrada1_bombaLigada,
                dados.entrada2_bombaFalhou,
                dados.entrada3_falhaInversor,
                dados.entrada4_painelSemEnergia);
        } else {
            Serial.println("[SENSOR] Leitura invalida!");
        }

        esp_now_send(GATEWAY_MAC, (uint8_t *)&dados, sizeof(dados));
    }
}
