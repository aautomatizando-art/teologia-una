/*
 * ESP32 #1 - No sensor (caixa d'agua)
 * Funcao: le nivel, controla bomba (SAIDA 1), detecta falha da bomba (SAIDA 2)
 *         e envia dados via ESP-NOW ao gateway
 * Sem WiFi necessario neste ESP32
 *
 * SAIDA 1 (GPIO25): Rele da bomba  -> "Bomba Ligada"
 * SAIDA 2 (GPIO26): Falha na bomba -> bomba ligada mas nivel nao sobe
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
    bool     saida1_bombaLigada;
    bool     saida2_falhaBomba;
    unsigned long uptime;
    bool     leituraValida;
} DadosSensor;

DadosSensor dados;
bool alertaNivelBaixo = false;
unsigned long ultimaMedicao = 0;

// Controle de deteccao de falha
unsigned long bombaLigadaDesde = 0;
float distanciaAoLigarBomba    = 0;

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

void ligarBomba(float distAtual) {
    digitalWrite(SAIDA1_PIN, HIGH);
    dados.saida1_bombaLigada = true;
    bombaLigadaDesde      = millis();
    distanciaAoLigarBomba = distAtual;
    Serial.println("[SAIDA 1] Bomba LIGADA");
}

void desligarBomba() {
    digitalWrite(SAIDA1_PIN, LOW);
    dados.saida1_bombaLigada = false;
    bombaLigadaDesde = 0;
    Serial.println("[SAIDA 1] Bomba DESLIGADA");
}

void acionarFalha() {
    digitalWrite(SAIDA2_PIN, HIGH);
    dados.saida2_falhaBomba = true;
    Serial.println("[SAIDA 2] FALHA NA BOMBA detectada!");
    if (DESLIGA_BOMBA_NA_FALHA) desligarBomba();
}

void limparFalha() {
    digitalWrite(SAIDA2_PIN, LOW);
    dados.saida2_falhaBomba = false;
}

void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN,   OUTPUT);
    pinMode(ECHO_PIN,   INPUT);
    pinMode(SAIDA1_PIN, OUTPUT);
    pinMode(SAIDA2_PIN, OUTPUT);
    pinMode(LED_PIN,    OUTPUT);
    desligarBomba();
    limparFalha();

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
            Serial.printf("[NIVEL] %.1fcm -> %d%% | S1: %d | S2: %d\n",
                dist, dados.nivel,
                dados.saida1_bombaLigada, dados.saida2_falhaBomba);

            // Nivel baixo -> liga bomba (se nao estiver em falha)
            if (dist >= DIST_CAIXA_VAZIA && !alertaNivelBaixo && !dados.saida2_falhaBomba) {
                ligarBomba(dist);
                alertaNivelBaixo = true;
            }

            // Nivel ok -> desliga bomba e limpa falha
            if (dist <= DIST_CAIXA_CHEIA && alertaNivelBaixo) {
                desligarBomba();
                limparFalha();
                alertaNivelBaixo = false;
            }

            // Deteccao de falha: bomba ligada ha muito tempo sem o nivel subir
            if (dados.saida1_bombaLigada && bombaLigadaDesde > 0 &&
                (agora - bombaLigadaDesde >= FALHA_TEMPO_MS)) {
                float subiu = distanciaAoLigarBomba - dist;  // nivel sobe = distancia diminui
                if (subiu < FALHA_DELTA_CM) {
                    acionarFalha();
                } else {
                    // nivel esta subindo: reinicia janela de verificacao
                    bombaLigadaDesde      = agora;
                    distanciaAoLigarBomba = dist;
                }
            }
        } else {
            Serial.println("[SENSOR] Leitura invalida!");
        }

        esp_now_send(GATEWAY_MAC, (uint8_t *)&dados, sizeof(dados));
    }
}
