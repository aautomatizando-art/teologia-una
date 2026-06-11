/*
 * ESP32 #2 - Gateway (proximo ao WiFi)
 * Funcao: recebe dados via ESP-NOW e envia alertas para um GRUPO do WhatsApp
 *         atraves da Evolution API rodando no VPS (Docker)
 *
 * Alertas enviados:
 *   - Nivel baixo + SAIDA 1 acionada (Bomba Ligada)
 *   - SAIDA 2 acionada (Falha na Bomba)
 *   - Caixa abastecida (bomba desligada)
 *   - Sensor sem comunicacao
 *
 * Bibliotecas necessarias:
 *   - ArduinoJson by Benoit Blanchon
 *   (HTTPClient e WiFi ja vem no ESP32 core)
 */

#include <esp_now.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Estrutura identica a do esp32_sensor!
typedef struct {
    float    distancia;
    int      nivel;
    bool     saida1_bombaLigada;
    bool     saida2_falhaBomba;
    unsigned long uptime;
    bool     leituraValida;
} DadosSensor;

DadosSensor dados;
volatile bool dadosNovos = false;

bool alertaNivelEnviado  = false;
bool alertaFalhaEnviado  = false;
bool alertaSemSinal      = false;
bool leituraInvalidaAvisada = false;
unsigned long ultimoStatus   = 0;
unsigned long ultimoRecebido = 0;

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(DadosSensor)) {
        memcpy((void *)&dados, data, sizeof(DadosSensor));
        dadosNovos     = true;
        ultimoRecebido = millis();
    }
}

void conectarWiFi() {
    Serial.print("[WiFi] Conectando");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 20) {
        delay(500); Serial.print("."); t++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Conectado! IP: " + WiFi.localIP().toString());
        Serial.println("[WiFi] Canal: " + String(WiFi.channel()));
    } else {
        Serial.println("\n[WiFi] Falha na conexao!");
    }
}

// Envia mensagem de texto ao grupo do WhatsApp via Evolution API
bool enviarWhatsApp(const String &texto) {
    if (WiFi.status() != WL_CONNECTED) return false;

    HTTPClient http;
    String url = String(EVO_BASE_URL) + "/message/sendText/" + EVO_INSTANCE;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", EVO_APIKEY);
    http.setTimeout(10000);

    JsonDocument doc;
    doc["number"] = WHATS_GROUP_ID;
    doc["text"]   = texto;
    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    bool ok = (code >= 200 && code < 300);
    Serial.printf("[WHATSAPP] HTTP %d %s\n", code, ok ? "OK" : resp.c_str());
    return ok;
}

void setup() {
    Serial.begin(115200);

    // WiFi DEVE iniciar antes do ESP-NOW para fixar o canal
    conectarWiFi();

    Serial.print("[GATEWAY] MAC: ");
    Serial.println(WiFi.macAddress());  // <- informe este MAC no config.h do sensor!

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ERRO na inicializacao!");
        return;
    }
    esp_now_register_recv_cb(onDataReceived);

    ultimoRecebido = millis();

    enviarWhatsApp(
        "\xF0\x9F\x92\xA7 *Monitor Caixa d'Agua iniciado!*\n"
        "Gateway online, aguardando dados do sensor...");

    Serial.println("[SETUP] Gateway pronto!");
}

void loop() {
    unsigned long agora = millis();

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Reconectando...");
        conectarWiFi();
    }

    // Sensor sem comunicacao
    if (agora - ultimoRecebido >= TIMEOUT_SENSOR_MS && ultimoRecebido > 0) {
        if (!alertaSemSinal) {
            enviarWhatsApp(
                "\xE2\x9A\xA0\xEF\xB8\x8F *ATENCAO: Sensor sem comunicacao!*\n"
                "O ESP32 da caixa d'agua nao envia dados ha mais de 2 minutos.\n"
                "Verifique alimentacao e alcance do ESP-NOW.");
            alertaSemSinal = true;
        }
    }

    if (dadosNovos) {
        dadosNovos     = false;
        alertaSemSinal = false;

        Serial.printf("[DADOS] %.1fcm | %d%% | S1(Bomba): %s | S2(Falha): %s | up %lus\n",
            dados.distancia, dados.nivel,
            dados.saida1_bombaLigada ? "ON" : "OFF",
            dados.saida2_falhaBomba  ? "ON" : "OFF",
            dados.uptime);

        // Leitura invalida do sensor
        if (!dados.leituraValida) {
            if (!leituraInvalidaAvisada) {
                enviarWhatsApp(
                    "\xE2\x9A\xA0\xEF\xB8\x8F *Sensor com leitura invalida!*\n"
                    "Verifique o JSN-SR04T na caixa d'agua.");
                leituraInvalidaAvisada = true;
            }
            return;
        }
        leituraInvalidaAvisada = false;

        // ── SAIDA 2: Falha na bomba (prioridade maxima) ──
        if (dados.saida2_falhaBomba && !alertaFalhaEnviado) {
            String msg = "\xF0\x9F\x9A\xA8 *FALHA NA BOMBA!* (Saida 2 acionada)\n\n";
            msg += "A bomba ficou ligada mas o nivel nao subiu.\n";
            msg += "Possiveis causas: bomba queimada, falta d'agua no poco, registro fechado.\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel atual: *" + String(dados.nivel) + "%*\n";
            msg += "\xF0\x9F\x94\xA7 Bomba desligada por seguranca.";
            enviarWhatsApp(msg);
            alertaFalhaEnviado = true;
        }
        if (!dados.saida2_falhaBomba) alertaFalhaEnviado = false;

        // ── Nivel baixo + SAIDA 1 acionada (Bomba Ligada) ──
        if (dados.nivel <= NIVEL_ALERTA && dados.saida1_bombaLigada && !alertaNivelEnviado) {
            String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *Nivel baixo na caixa d'agua!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(dados.nivel) + "%*\n";
            msg += "\xF0\x9F\x93\x8F Distancia: " + String(dados.distancia, 1) + "cm\n";
            msg += "\xF0\x9F\x9F\xA2 Saida 1 acionada: *Bomba LIGADA*";
            enviarWhatsApp(msg);
            alertaNivelEnviado = true;
        }

        // ── Caixa abastecida ──
        if (dados.nivel >= NIVEL_OK && alertaNivelEnviado) {
            String msg = "\xE2\x9C\x85 *Caixa d'agua abastecida!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(dados.nivel) + "%*\n";
            msg += "\xF0\x9F\x94\xB4 Bomba DESLIGADA.";
            enviarWhatsApp(msg);
            alertaNivelEnviado = false;
        }
    }

    // Status periodico (opcional, desativado por padrao)
    if (INTERVALO_STATUS_MS > 0 && agora - ultimoStatus >= INTERVALO_STATUS_MS) {
        ultimoStatus = agora;
        String msg = "\xF0\x9F\x93\x8B *Status do sistema*\n";
        msg += "Nivel: " + String(dados.nivel) + "%\n";
        msg += "Saida 1 (Bomba): " + String(dados.saida1_bombaLigada ? "LIGADA" : "DESLIGADA") + "\n";
        msg += "Saida 2 (Falha): " + String(dados.saida2_falhaBomba ? "ACIONADA" : "normal") + "\n";
        msg += "Sensor online: " + String(dados.uptime / 60) + " min";
        enviarWhatsApp(msg);
    }
}
