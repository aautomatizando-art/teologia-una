/*
 * ESP32 - Monitor da Agua da Rua
 * Funcao: le um sensor de fluxo por efeito Hall (ex: YF-S201) instalado na
 *         entrada de agua da rua, calcula a vazao em litros/minuto, envia
 *         alertas para um GRUPO do WhatsApp atraves da Evolution API caso o
 *         fluxo pare por muito tempo, e publica os dados no Supabase para a
 *         dashboard web (Vercel)
 *
 * FLUXO_PIN (GPIO27): Sensor de fluxo (hall effect, ex: YF-S201)
 *
 * WiFi: configuravel no local via WiFiManager. Se nao conectar na rede salva,
 * o ESP32 cria o AP "AguaRua-Setup" (senha 12345678) com portal para
 * escolher a rede e digitar a senha do WiFi do condominio.
 *
 * Bibliotecas necessarias:
 *   - ArduinoJson by Benoit Blanchon
 *   - WiFiManager by tzapu
 *   (HTTPClient, WiFiClientSecure e WiFi ja vem no ESP32 core)
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

volatile unsigned long contadorPulsos = 0;

float fluxo_lpm   = 0;
bool  fluxo_ativo = true;

unsigned long tempoSemFluxo  = 0;
bool          alertaSemFluxoEnviado = false;

unsigned long ultimaMedicao      = 0;
unsigned long ultimaTentativaWiFi = 0;

// Interrupcao: incrementa o contador a cada pulso do sensor de fluxo
void IRAM_ATTR pulsoFluxo() {
    contadorPulsos++;
}

// Conecta na rede salva; se falhar, abre o portal de configuracao no local
void conectarWiFi() {
    WiFi.mode(WIFI_STA);

    WiFiManager wm;
    wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
    wm.setConnectTimeout(20);

    Serial.println("[WiFi] Conectando na rede salva (ou abrindo portal)...");
    if (!wm.autoConnect(AP_CONFIG_NOME, AP_CONFIG_SENHA)) {
        // Sem rede salva e ninguem configurou no portal: reinicia e tenta de novo
        Serial.println("[WiFi] Portal expirou sem configuracao. Reiniciando...");
        delay(2000);
        ESP.restart();
    }

    Serial.println("[WiFi] Conectado! IP: " + WiFi.localIP().toString());
}

// Envia mensagem de texto ao grupo do WhatsApp via Evolution API
// (prefixa com o nome do condominio para identificar a origem no grupo)
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
    doc["text"]   = "🏢 *" + String(CONDOMINIO_NOME) + "*\n" + texto;
    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    bool ok = (code >= 200 && code < 300);
    Serial.printf("[WHATSAPP] HTTP %d %s\n", code, ok ? "OK" : resp.c_str());
    return ok;
}

// Envia o status atual para o Supabase (dashboard web)
bool enviarSupabase() {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(SUPABASE_URL) + "/rest/v1/agua_rua";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Prefer", "return=minimal");
    http.setTimeout(10000);

    JsonDocument doc;
    doc["condominio"]      = CONDOMINIO_NOME;
    doc["fluxo_lpm"]       = fluxo_lpm;
    doc["fluxo_ativo"]     = fluxo_ativo;
    doc["sensor_uptime_s"] = millis() / 1000;
    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    bool ok = (code >= 200 && code < 300);
    Serial.printf("[SUPABASE] HTTP %d %s\n", code, ok ? "OK" : http.getString().c_str());
    http.end();
    return ok;
}

void setup() {
    Serial.begin(115200);

    pinMode(FLUXO_PIN, INPUT_PULLUP);
    attachInterrupt(digitalPinToInterrupt(FLUXO_PIN), pulsoFluxo, FALLING);

    conectarWiFi();

    enviarWhatsApp("🚰 *Monitor Agua da Rua iniciado!*");

    ultimaMedicao       = millis();
    ultimaTentativaWiFi = millis();
    tempoSemFluxo       = 0;

    Serial.println("[SETUP] Monitor da agua da rua pronto!");
}

void loop() {
    unsigned long agora = millis();

    // Queda temporaria do WiFi: reconecta na rede salva (sem abrir o portal)
    if (WiFi.status() != WL_CONNECTED && agora - ultimaTentativaWiFi >= 15000UL) {
        ultimaTentativaWiFi = agora;
        Serial.println("[WiFi] Reconectando...");
        WiFi.reconnect();
    }

    // A cada INTERVALO_MEDICAO_MS: calcula a vazao com base nos pulsos
    // acumulados, zera o contador e atualiza o estado de fluxo
    if (agora - ultimaMedicao >= INTERVALO_MEDICAO_MS) {
        unsigned long intervaloDecorrido = agora - ultimaMedicao;
        ultimaMedicao = agora;

        noInterrupts();
        unsigned long pulsos = contadorPulsos;
        contadorPulsos = 0;
        interrupts();

        fluxo_lpm   = (pulsos / FLUXO_FATOR_CALIBRACAO) / (intervaloDecorrido / 1000.0) * 60.0;
        fluxo_ativo = fluxo_lpm > FLUXO_MINIMO_LPM;

        Serial.printf("[FLUXO] %lu pulsos | %.2f L/min | %s\n",
            pulsos, fluxo_lpm, fluxo_ativo ? "ATIVO" : "PARADO");

        bool houveMudanca = false;

        if (fluxo_ativo) {
            // Fluxo voltou: zera o contador de tempo sem fluxo
            tempoSemFluxo = 0;

            if (alertaSemFluxoEnviado) {
                enviarWhatsApp("✅ Agua da rua normalizada — fluxo detectado novamente.");
                alertaSemFluxoEnviado = false;
                houveMudanca = true;
            }
        } else {
            // Sem fluxo: acumula o tempo parado
            tempoSemFluxo += intervaloDecorrido;

            if (tempoSemFluxo >= FLUXO_TIMEOUT_MS && !alertaSemFluxoEnviado) {
                enviarWhatsApp("🚱 *Agua da rua parou!* Sem fluxo detectado ha mais de 30 minutos.");
                alertaSemFluxoEnviado = true;
                houveMudanca = true;
            }
        }

        // Heartbeat: envia status para o Supabase a cada medicao (com ou sem mudanca)
        enviarSupabase();
        (void)houveMudanca;
    }
}
