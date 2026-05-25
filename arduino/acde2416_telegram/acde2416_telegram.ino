/*
 * ASCAEL ACDE 24/16 → Telegram  |  ESP32
 *
 * Protocolo binário a 4800 baud via pino 39 (RB6/KBI2/PGC) do PIC18F4520.
 *
 * Ligação direta (sem MAX3232):
 *   PIC pino 39 (RB6) → ESP32 GPIO16 (RX2)
 *   PIC pino 31 (GND) → ESP32 GND
 *
 * Protocolo confirmado em campo:
 *   0x00 = vazio / separador
 *   0x02 = dispositivo supervisionado (normal)
 *   0xFF = status geral normal
 *   Qualquer outro byte (ex: 0xBF) = alarme ativo
 *
 * Nota: a saída serial do PIC não distingue qual dispositivo acionou.
 * O byte de alarme é fixo (0xBF) independente do endereço DIP da botoeira.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ── Estado ────────────────────────────────────────
bool          alarmAtivo        = false;
unsigned long tUltimoByte       = 0;
unsigned long tEnvioAlarme      = 0;
unsigned long tEnvioNormal      = 0;
unsigned long tUltimaReconexao  = 0;

// ── Escapa HTML ───────────────────────────────────
String htmlEsc(const String& s) {
    String r = s;
    r.replace("&", "&amp;");
    r.replace("<", "&lt;");
    r.replace(">", "&gt;");
    return r;
}

// ── Monta mensagem Telegram ───────────────────────
String montarMensagem(bool alarme) {
    String emoji  = alarme ? "\xF0\x9F\x94\xA5" : "\xE2\x9C\x85";
    String titulo = alarme ? "ALARME DE INC\xC3\x8ANDIO" : "NORMALIZADO";

    String msg = emoji + " <b>" + titulo + "</b>\n\n";
    msg += "\xF0\x9F\x93\x8D " + htmlEsc(String(NOME_LOCAL)) + "\n";
    msg += "\n<i>Central: " + htmlEsc(String(NOME_CENTRAL)) + "</i>";
    return msg;
}

// ── Envia ao Telegram ─────────────────────────────
bool enviarTelegram(const String& mensagem) {
    StaticJsonDocument<512> doc;
    doc["chat_id"]    = TELEGRAM_CHAT_ID;
    doc["text"]       = mensagem;
    doc["parse_mode"] = "HTML";
    String body;
    serializeJson(doc, body);

    String url = "https://api.telegram.org/bot";
    url += TELEGRAM_TOKEN;
    url += "/sendMessage";

    for (int t = 1; t <= 3; t++) {
        Serial.printf("[TG] Tentativa %d/3  heap: %d\n", t, ESP.getFreeHeap());
        WiFiClientSecure c;
        c.setInsecure();
        c.setTimeout(15);
        HTTPClient http;
        http.begin(c, url);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(15000);
        int code = http.POST(body);
        http.end();
        if (code == 200) { Serial.println("[TG] Enviado."); return true; }
        Serial.printf("[TG] Erro %d. %s\n", code, t < 3 ? "Aguardando 4s..." : "Desistindo.");
        if (t < 3) delay(4000);
    }
    return false;
}

// ── Processa byte recebido da central ─────────────
void processarByte(uint8_t b) {
    if (RAW_MODE) {
        Serial.printf("0x%02X\n", b);
        return;
    }

    // Bytes normais — ignora
    if (b == 0x00 || b == 0x02 || b == 0xFF) return;

    // Byte de alarme detectado
    tUltimoByte = millis();

    if (!alarmAtivo) {
        alarmAtivo = true;
        Serial.printf("[ALARME] Byte: 0x%02X\n", b);

        if (millis() - tEnvioAlarme >= COOLDOWN_MS) {
            if (enviarTelegram(montarMensagem(true))) {
                tEnvioAlarme = millis();
            }
        }
    }
}

// ── Verifica se alarme normalizou ─────────────────
void verificarNormal() {
    if (!alarmAtivo) return;
    if (millis() - tUltimoByte < TIMEOUT_NORMAL_MS) return;

    alarmAtivo = false;
    Serial.println("[NORMAL] Restaurado.");

    if (ENVIAR_NORMAL && millis() - tEnvioNormal >= COOLDOWN_MS) {
        if (enviarTelegram(montarMensagem(false))) {
            tEnvioNormal = millis();
        }
    }
}

// ── Notifica conexão ──────────────────────────────
void enviarConectado() {
    delay(4000);
    IPAddress ip;
    Serial.print("[DNS] Resolvendo api.telegram.org... ");
    if (!WiFi.hostByName("api.telegram.org", ip)) {
        Serial.println("FALHOU.");
        return;
    }
    Serial.println("OK → " + ip.toString());

    String msg = "\xF0\x9F\x9F\xA2 <b>CENTRAL CONECTADA</b>\n\n";
    msg += "\xF0\x9F\x93\xA1 Sistema supervisório <b>online</b>\n";
    msg += "\xF0\x9F\x8C\x90 IP: <code>" + WiFi.localIP().toString() + "</code>\n";
    msg += "\xF0\x9F\x93\x8B Central: " + htmlEsc(String(NOME_CENTRAL)) + "\n";
    msg += "\xF0\x9F\x93\x8D Local: " + htmlEsc(String(NOME_LOCAL)) + "\n";
    msg += "\n<i>Monitoramento ativo. Aguardando eventos.</i>";
    enviarTelegram(msg);
}

// ── WiFi ──────────────────────────────────────────
void conectarWiFi() {
    Serial.print("[WiFi] Conectando a ");
    Serial.print(WIFI_SSID);
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 20) {
        delay(500); Serial.print("."); t++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
                    IPAddress(8, 8, 8, 8), IPAddress(8, 8, 4, 4));
        Serial.println("\n[WiFi] IP: " + WiFi.localIP().toString());
        enviarConectado();
    } else {
        Serial.println("\n[WiFi] Falha.");
    }
    tUltimaReconexao = millis();
}

// ── Setup ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial2.begin(BAUD_CENTRAL, SERIAL_8N1, PIN_RX2, PIN_TX2);
    Serial.println("\n=== " + String(NOME_CENTRAL) + " \xE2\x86\x92 Telegram ===");
    conectarWiFi();
    Serial.printf("[SER] Serial2: %d bps  RX=GPIO%d\n", BAUD_CENTRAL, PIN_RX2);
    Serial.println("[OK] Aguardando eventos...\n");
}

// ── Loop ──────────────────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - tUltimaReconexao > 30000) conectarWiFi();
    }

    while (Serial2.available()) {
        processarByte((uint8_t)Serial2.read());
    }

    verificarNormal();
}
