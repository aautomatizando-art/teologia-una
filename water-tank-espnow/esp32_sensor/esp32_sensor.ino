/*
 * ESP32 #1 - No sensor (caixa d'agua)
 * Funcao: le nivel, controla bomba, envia dados via ESP-NOW
 * Sem WiFi necessario neste ESP32
 *
 * Bibliotecas: apenas ESP32 core (nenhuma adicional necessaria)
 */

#include <esp_now.h>
#include <WiFi.h>
#include "config.h"

// Estrutura de dados enviada via ESP-NOW
typedef struct {
    float    distancia;
    int      nivel;
    bool     bombaLigada;
    unsigned long uptime;
    bool     leituraValida;
} DadosSensor;

DadosSensor dados;
bool alertaEnviado  = false;
unsigned long ultimaMedicao = 0;

// Callback de confirmacao de envio
void onDataSent(const uint8_t *mac, esp_now_send_status_t status) {
    bool ok = (status == ESP_NOW_SEND_SUCCESS);
    Serial.println(ok ? "[ESP-NOW] Enviado com sucesso" : "[ESP-NOW] Falha no envio");
    // Pisca LED para indicar envio
    digitalWrite(LED_PIN, HIGH); delay(80);
    digitalWrite(LED_PIN, LOW);
}

// Leitura media do sensor (filtra leituras invalidas)
float medirMediaCm(int amostras = 5) {
    float soma   = 0;
    int validas  = 0;
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

void ligarBomba() {
    digitalWrite(RELAY_PIN, HIGH);
    dados.bombaLigada = true;
    Serial.println("[BOMBA] Ligada!");
}

void desligarBomba() {
    digitalWrite(RELAY_PIN, LOW);
    dados.bombaLigada = false;
    Serial.println("[BOMBA] Desligada.");
}

void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN,  OUTPUT);
    pinMode(ECHO_PIN,  INPUT);
    pinMode(RELAY_PIN, OUTPUT);
    pinMode(LED_PIN,   OUTPUT);
    desligarBomba();

    // ESP-NOW exige WiFi em modo STA (sem conectar a nenhuma rede)
    WiFi.mode(WIFI_STA);
    WiFi.disconnect();

    Serial.print("[SENSOR] MAC: ");
    Serial.println(WiFi.macAddress());

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ERRO na inicializacao!");
        return;
    }
    esp_now_register_send_cb(onDataSent);

    // Registra o ESP32 gateway como peer
    esp_now_peer_info_t peer = {};
    memcpy(peer.peer_addr, GATEWAY_MAC, 6);
    peer.channel  = 0;
    peer.encrypt  = false;
    if (esp_now_add_peer(&peer) != ESP_OK) {
        Serial.println("[ESP-NOW] ERRO ao registrar gateway!");
        return;
    }

    // LED: 3 piscadas = pronto
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

            // Controle local da bomba
            if (dist >= DIST_CAIXA_VAZIA && !alertaEnviado) {
                ligarBomba();
                alertaEnviado = true;
            }
            if (dist <= DIST_CAIXA_CHEIA && alertaEnviado) {
                desligarBomba();
                alertaEnviado = false;
            }
        } else {
            Serial.println("[SENSOR] Leitura invalida!");
        }

        // Envia dados ao gateway via ESP-NOW
        esp_now_send(GATEWAY_MAC, (uint8_t *)&dados, sizeof(dados));
    }
}
