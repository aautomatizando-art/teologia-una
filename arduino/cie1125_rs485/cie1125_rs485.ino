/*
 * CIE 1125 (Intelbras) RS485 Repetidora → Telegram  |  ESP32
 *
 * Lê o barramento RS485 da saída Repetidora (bornes D+ / D-) da CIE 1125.
 * Protocolo confirmado em campo: frames de 42 bytes, delimitados por 0x7E (início)
 * e 0x7C (fim).
 *
 * Posições relevantes no frame (índice 0 = primeiro byte após 0x7E):
 *   [8]  → tipo de frame: 0x18 = poll normal | 0x28 = frame de evento
 *   [10] → contador de alarmes ativos (0 = sem alarme, N = N alarmes)
 *   [28] → Laço do dispositivo que acionou
 *   [29] → Endereço do dispositivo que acionou
 *   [36] → flag global de alarme (0 = normal, 1 = alarme ativo)
 *
 * Ligação:
 *   CIE D+ → MAX485 A (borne verde)
 *   CIE D- → MAX485 B (borne verde)
 *   MAX485 RO    → ESP32 GPIO16
 *   MAX485 RE+DE → GND  (recepção permanente)
 *   MAX485 VCC   → 3.3V
 *   MAX485 GND   → GND
 *
 * Pré-requisito na central:
 *   Sistema → Configurações → Habilitar Repetidoras → R1 = ON
 *
 * Bibliotecas:
 *   ArduinoJson (Benoit Blanchon) – instalar via Gerenciar Bibliotecas
 *   WiFi, WiFiClientSecure, HTTPClient – inclusas no core ESP32
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ── Protocolo RS485 ───────────────────────────────
#define FRAME_START   0x7E
#define FRAME_END     0x7C
#define FRAME_LEN     42      // bytes entre START e END (inclusive os dois)

#define IDX_TIPO      8       // 0x18=normal | 0x28=evento
#define IDX_ALARMES   10      // contador de alarmes ativos
#define IDX_LACO      28      // laço do dispositivo acionado
#define IDX_ENDERECO  29      // endereço do dispositivo acionado
#define IDX_FLAG      36      // flag global de alarme (0/1)

// ── Globals ───────────────────────────────────────
uint8_t       frameBuf[FRAME_LEN];
int           framePos       = -1;   // -1 = aguardando START
bool          alarmAtivo     = false;
uint8_t       lacoAlarme     = 0;
uint8_t       endAlarme      = 0;
uint8_t       ctAlarmesAnt   = 0;

unsigned long tUltimaReconexao = 0;
unsigned long tUltimoEnvio    = 0;

// ── Lookup de dispositivo ─────────────────────────
const char* nomePorEndereco(uint8_t laco, uint8_t end) {
    for (int i = 0; DISPOSITIVOS[i].nome != nullptr; i++) {
        if (DISPOSITIVOS[i].laco == laco && DISPOSITIVOS[i].endereco == end)
            return DISPOSITIVOS[i].nome;
    }
    return nullptr;
}

// ── Telegram ──────────────────────────────────────
String htmlEsc(const String& s) {
    String r = s;
    r.replace("&", "&amp;");
    r.replace("<", "&lt;");
    r.replace(">", "&gt;");
    return r;
}

String montarMensagem(bool alarme, const char* nomeDisp,
                       uint8_t laco, uint8_t end) {
    String emoji  = alarme ? "\xF0\x9F\x94\xA5" : "\xE2\x9C\x85";
    String titulo = alarme ? "ALARME DE INC\xC3\x8ANDIO" : "NORMALIZADO";

    String msg = emoji + " <b>" + titulo + "</b>\n\n";

    if (nomeDisp) {
        msg += "\xF0\x9F\x93\x8B " + htmlEsc(String(nomeDisp)) + "\n";
    } else {
        // dispositivo não mapeado — mostra endereço bruto
        char buf[32];
        snprintf(buf, sizeof(buf), "La\xC3\xA7o 0x%02X  End. 0x%02X", laco, end);
        msg += "\xF0\x9F\x93\x8B " + String(buf) + "\n";
    }

    char uptime[10];
    unsigned long s = millis() / 1000;
    snprintf(uptime, sizeof(uptime), "%02lu:%02lu:%02lu",
             s / 3600, (s % 3600) / 60, s % 60);
    msg += "\xF0\x9F\x95\x90 Uptime: " + String(uptime) + "\n";
    msg += "\n<i>Central: CIE 1125 | Intelbras</i>";
    return msg;
}

bool enviarTelegram(const String& mensagem) {
    String url = "https://api.telegram.org/bot";
    url += TELEGRAM_TOKEN;
    url += "/sendMessage";

    for (int t = 1; t <= 3; t++) {
        Serial.printf("[TG] Tentativa %d/3\n", t);
        WiFiClientSecure c;
        c.setInsecure();
        c.setTimeout(15);
        HTTPClient http;
        http.begin(c, url);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(15000);

        StaticJsonDocument<512> doc;
        doc["chat_id"]    = TELEGRAM_CHAT_ID;
        doc["text"]       = mensagem;
        doc["parse_mode"] = "HTML";
        String body;
        serializeJson(doc, body);

        int code = http.POST(body);
        http.end();

        if (code == 200) { Serial.println("[TG] Enviado."); return true; }
        Serial.printf("[TG] Erro %d.%s\n", code, t < 3 ? " Aguardando 4s..." : "");
        if (t < 3) delay(4000);
    }
    return false;
}

// ── Processa frame completo ───────────────────────
void processarFrame() {
    uint8_t ctAlarmes = frameBuf[IDX_ALARMES];
    uint8_t laco      = frameBuf[IDX_LACO];
    uint8_t end_      = frameBuf[IDX_ENDERECO];

    // Alarme: contador passou de 0 para N
    if (!alarmAtivo && ctAlarmes > 0) {
        alarmAtivo   = true;
        lacoAlarme   = laco;
        endAlarme    = end_;
        ctAlarmesAnt = ctAlarmes;

        const char* nome = nomePorEndereco(laco, end_);
        Serial.printf("[ALARME] La\xC3\xA7o=0x%02X End=0x%02X  %s\n",
                      laco, end_, nome ? nome : "(n\xC3\xA3o mapeado)");

        if (millis() - tUltimoEnvio >= COOLDOWN_MS) {
            if (enviarTelegram(montarMensagem(true, nome, laco, end_)))
                tUltimoEnvio = millis();
        }
    }

    // Normal: contador voltou a 0
    if (alarmAtivo && ctAlarmes == 0) {
        alarmAtivo = false;
        Serial.println("[NORMAL] Restaurado.");

        const char* nome = nomePorEndereco(lacoAlarme, endAlarme);
        if (millis() - tUltimoEnvio >= COOLDOWN_MS) {
            if (enviarTelegram(montarMensagem(false, nome, lacoAlarme, endAlarme)))
                tUltimoEnvio = millis();
        }
    }

    ctAlarmesAnt = ctAlarmes;
}

// ── Parser de frame byte a byte ───────────────────
void parseByte(uint8_t b) {
    if (b == FRAME_START) {
        // Inicia captura de novo frame
        framePos       = 0;
        frameBuf[0]    = b;
        return;
    }

    if (framePos < 0) return;   // ainda não sincronizado

    framePos++;
    if (framePos >= FRAME_LEN) {
        // Frame maior que esperado — descarta e ressincroniza
        framePos = -1;
        return;
    }
    frameBuf[framePos] = b;

    if (b == FRAME_END && framePos == FRAME_LEN - 1) {
        processarFrame();
        framePos = -1;
    }
}

// ── WiFi ──────────────────────────────────────────
void enviarConectado() {
    delay(3000);
    IPAddress ip;
    if (!WiFi.hostByName("api.telegram.org", ip)) return;

    String msg = "\xF0\x9F\x9F\xA2 <b>SUPERVISOR CONECTADO</b>\n\n";
    msg += "\xF0\x9F\x8C\x90 IP: <code>" + WiFi.localIP().toString() + "</code>\n";
    msg += "<i>CIE 1125 RS485 | aguardando eventos.</i>";
    enviarTelegram(msg);
}

void conectarWiFi() {
    Serial.print("[WiFi] Conectando");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 40) {
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
    Serial2.begin(BAUD_RS485, SERIAL_8N1, PIN_RX, PIN_TX);
    Serial.println("\n=== CIE 1125 RS485 \xE2\x86\x92 Telegram ===");
    Serial.printf("[RS485] %d bps  RX=GPIO%d\n", BAUD_RS485, PIN_RX);
    conectarWiFi();
    Serial.println("[OK] Aguardando frames RS485...\n");
}

// ── Loop ──────────────────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - tUltimaReconexao > 30000) conectarWiFi();
    }

    while (Serial2.available()) {
        parseByte((uint8_t)Serial2.read());
    }
}
