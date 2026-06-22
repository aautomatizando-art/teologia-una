/*
 * ESP32 - Monitor das Bombas de Incendio
 * Funcao: le 3 entradas digitais do quadro de comando das bombas de
 *         incendio, envia alertas para um GRUPO do WhatsApp atraves da
 *         Evolution API e publica os dados no Supabase para a dashboard
 *         web (Vercel)
 *
 * ENTRADA 1 (GPIO27): Bomba principal de incendio ligada
 * ENTRADA 2 (GPIO14): Falha na bomba de incendio
 * ENTRADA 3 (GPIO13): Bomba reserva de incendio ligada
 *
 * WiFi: configuravel no local via WiFiManager. Se nao conectar na rede salva,
 * o ESP32 cria o AP "BombaIncendio-Setup" (senha 12345678) com portal para
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

bool entrada1_bombaPrincipalLigada = false;
bool entrada2_falhaBomba           = false;
bool entrada3_bombaReservaLigada   = false;

bool alertaBombaPrincipalEnviado = false;
bool alertaFalhaBombaEnviado     = false;
bool alertaBombaReservaEnviado   = false;

unsigned long ultimoEnvio        = 0;
unsigned long ultimaTentativaWiFi = 0;

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

// Le as 3 entradas digitais (contato seco para GND = ativo, INPUT_PULLUP)
void lerEntradas() {
    entrada1_bombaPrincipalLigada = (digitalRead(ENTRADA1_PIN) == LOW);
    entrada2_falhaBomba           = (digitalRead(ENTRADA2_PIN) == LOW);
    entrada3_bombaReservaLigada   = (digitalRead(ENTRADA3_PIN) == LOW);
}

// Envia o status atual para o Supabase (dashboard web)
bool enviarSupabase() {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(SUPABASE_URL) + "/rest/v1/bombas_incendio";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Prefer", "return=minimal");
    http.setTimeout(10000);

    JsonDocument doc;
    doc["condominio"]                       = CONDOMINIO_NOME;
    doc["entrada1_bomba_principal_ligada"]  = entrada1_bombaPrincipalLigada;
    doc["entrada2_falha_bomba"]             = entrada2_falhaBomba;
    doc["entrada3_bomba_reserva_ligada"]    = entrada3_bombaReservaLigada;
    doc["sensor_uptime_s"]                  = millis() / 1000;
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

    pinMode(ENTRADA1_PIN, INPUT_PULLUP);
    pinMode(ENTRADA2_PIN, INPUT_PULLUP);
    pinMode(ENTRADA3_PIN, INPUT_PULLUP);

    conectarWiFi();

    lerEntradas();

    enviarWhatsApp("🧯 *Monitor Bombas de Incendio iniciado!*");

    ultimoEnvio        = millis();
    ultimaTentativaWiFi = millis();

    Serial.println("[SETUP] Monitor de bombas de incendio pronto!");
}

void loop() {
    unsigned long agora = millis();

    // Queda temporaria do WiFi: reconecta na rede salva (sem abrir o portal)
    if (WiFi.status() != WL_CONNECTED && agora - ultimaTentativaWiFi >= 15000UL) {
        ultimaTentativaWiFi = agora;
        Serial.println("[WiFi] Reconectando...");
        WiFi.reconnect();
    }

    // Le entradas com debounce simples
    bool e1Antes = entrada1_bombaPrincipalLigada;
    bool e2Antes = entrada2_falhaBomba;
    bool e3Antes = entrada3_bombaReservaLigada;

    lerEntradas();
    delay(50);
    lerEntradas();

    bool houveMudanca = false;

    // ── ENTRADA 1: Bomba principal de incendio ligada ──
    if (entrada1_bombaPrincipalLigada && !alertaBombaPrincipalEnviado) {
        enviarWhatsApp("🧯 *Bomba Principal de Incendio LIGADA!*");
        alertaBombaPrincipalEnviado = true;
        houveMudanca = true;
    }
    if (!entrada1_bombaPrincipalLigada && alertaBombaPrincipalEnviado) {
        enviarWhatsApp("✅ Bomba Principal de Incendio desligada.");
        alertaBombaPrincipalEnviado = false;
        houveMudanca = true;
    }

    // ── ENTRADA 2: Falha na bomba de incendio ──
    if (entrada2_falhaBomba && !alertaFalhaBombaEnviado) {
        enviarWhatsApp("🚨 *FALHA NA BOMBA DE INCENDIO!* Verifique o quadro de comando.");
        alertaFalhaBombaEnviado = true;
        houveMudanca = true;
    }
    if (!entrada2_falhaBomba && alertaFalhaBombaEnviado) {
        enviarWhatsApp("✅ Falha na bomba de incendio normalizada.");
        alertaFalhaBombaEnviado = false;
        houveMudanca = true;
    }

    // ── ENTRADA 3: Bomba reserva de incendio ligada ──
    if (entrada3_bombaReservaLigada && !alertaBombaReservaEnviado) {
        enviarWhatsApp("🧯 *Bomba Reserva de Incendio LIGADA!*");
        alertaBombaReservaEnviado = true;
        houveMudanca = true;
    }
    if (!entrada3_bombaReservaLigada && alertaBombaReservaEnviado) {
        enviarWhatsApp("✅ Bomba Reserva de Incendio desligada.");
        alertaBombaReservaEnviado = false;
        houveMudanca = true;
    }

    if (houveMudanca) {
        Serial.printf("[ENTRADAS] E1(Bomba principal): %s | E2(Falha bomba): %s | E3(Bomba reserva): %s\n",
            entrada1_bombaPrincipalLigada ? "ON" : "OFF",
            entrada2_falhaBomba           ? "ON" : "OFF",
            entrada3_bombaReservaLigada   ? "ON" : "OFF");
        enviarSupabase();
        ultimoEnvio = agora;
    }

    // Heartbeat: envia status periodicamente, mesmo sem mudanca
    if (agora - ultimoEnvio >= INTERVALO_ENVIO_MS) {
        ultimoEnvio = agora;
        enviarSupabase();
    }
}
