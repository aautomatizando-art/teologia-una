/*
 * CIE 1125 (Intelbras) RS485 Repetidora → Telegram  |  ESP32
 *
 * DIAG_MODE = true  → imprime SOMENTE frames com byte[36]=1 (alarme).
 *                      Acione cada botoeira e anote o b28 que aparece.
 *                      Esse b28 é o que entra em DISPOSITIVOS[].
 * DIAG_MODE = false → operação normal (Telegram ativo).
 *
 * Ligação:
 *   CIE D+ → MAX485 A   MAX485 RO → GPIO16   RE+DE → GND
 *
 * Pré-requisito: Repetidoras → R1 = ON
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ── Protocolo ─────────────────────────────────────
#define FRAME_START  0x7E
#define FRAME_END    0x7C
#define FRAME_LEN    42

#define IDX_B8    8
#define IDX_B10   10
#define IDX_B28   28
#define IDX_B29   29
#define IDX_B36   36    // byte de alarme confirmado em campo

// ── Globals ───────────────────────────────────────
uint8_t  frameBuf[FRAME_LEN];
int      framePos = -1;

bool     sistemaPronto = false;
bool     alarmAtivo    = false;
uint8_t  lacoAlarme    = 0;
uint8_t  endAlarme     = 0;
unsigned long tEnvio[256];
unsigned long tUltimaReconexao = 0;

// ── Lookup ────────────────────────────────────────
const char* nomePorLaco(uint8_t laco) {
    for (int i = 0; DISPOSITIVOS[i].nome != nullptr; i++)
        if (DISPOSITIVOS[i].laco == laco) return DISPOSITIVOS[i].nome;
    return nullptr;
}

// ── Telegram ──────────────────────────────────────
String htmlEsc(const String& s) {
    String r = s; r.replace("&","&amp;"); r.replace("<","&lt;"); r.replace(">","&gt;");
    return r;
}

String montarMensagem(bool alarme, const char* nome) {
    String emoji  = alarme ? "\xF0\x9F\x94\xA5" : "\xE2\x9C\x85";
    String titulo = alarme ? "ALARME DE INC\xC3\x8ANDIO" : "NORMALIZADO";
    String msg    = emoji + " <b>" + titulo + "</b>\n\n";
    if (nome) msg += "\xF0\x9F\x93\x8B " + htmlEsc(String(nome)) + "\n";
    char up[10]; unsigned long s = millis()/1000;
    snprintf(up, sizeof(up), "%02lu:%02lu:%02lu", s/3600, (s%3600)/60, s%60);
    msg += "\xF0\x9F\x95\x90 Uptime: " + String(up) + "\n";
    msg += "\n<i>Central: CIE 1125 | Intelbras</i>";
    return msg;
}

bool enviarTelegram(const String& msg) {
    String url = "https://api.telegram.org/bot"; url += TELEGRAM_TOKEN; url += "/sendMessage";
    for (int t = 1; t <= 3; t++) {
        Serial.printf("[TG] Tentativa %d/3\n", t);
        WiFiClientSecure c; c.setInsecure(); c.setTimeout(15);
        HTTPClient http; http.begin(c, url);
        http.addHeader("Content-Type","application/json"); http.setTimeout(15000);
        StaticJsonDocument<512> doc;
        doc["chat_id"] = TELEGRAM_CHAT_ID; doc["text"] = msg; doc["parse_mode"] = "HTML";
        String body; serializeJson(doc, body);
        int code = http.POST(body); http.end();
        if (code == 200) { Serial.println("[TG] Enviado."); return true; }
        Serial.printf("[TG] Erro %d.%s\n", code, t<3 ? " 4s..." : "");
        if (t < 3) delay(4000);
    }
    return false;
}

// ── Processa frame ────────────────────────────────
void processarFrame() {
    uint8_t flag = frameBuf[IDX_B36];
    uint8_t laco = frameBuf[IDX_B28];
    uint8_t end_ = frameBuf[IDX_B29];

#if DIAG_MODE
    // Imprime APENAS frames com byte[36]=1 (dispositivo em alarme).
    // Acione a botoeira e veja qual b28 aparece.
    if (flag == 1) {
        Serial.printf("[D] b28=0x%02X  b29=0x%02X  b8=0x%02X  b10=0x%02X\n",
                      laco, end_, frameBuf[IDX_B8], frameBuf[IDX_B10]);
    }
    return;
#endif

    // ── Operação normal ───────────────────────────

    // Aguarda sistema limpo antes de monitorar
    if (!sistemaPronto) {
        if (flag == 0) {
            sistemaPronto = true;
            Serial.println("[INIT] Sistema normal. Monitoramento ativo.");
        }
        return;
    }

    // Alarme: byte[36]=1 e dispositivo mapeado
    if (!alarmAtivo && flag == 1) {
        const char* nome = nomePorLaco(laco);
        if (nome) {
            alarmAtivo = true;
            lacoAlarme = laco;
            endAlarme  = end_;
            Serial.printf("[ALARME] b28=0x%02X b29=0x%02X \xE2\x86\x92 %s\n", laco, end_, nome);
            if (millis() - tEnvio[laco] >= COOLDOWN_MS) {
                if (enviarTelegram(montarMensagem(true, nome)))
                    tEnvio[laco] = millis();
            }
        } else {
            // Dispositivo não mapeado — imprime para diagnóstico sem notificar
            Serial.printf("[SCAN] b28=0x%02X b29=0x%02X (nao mapeado)\n", laco, end_);
        }
    }

    // Normal: o MESMO dispositivo que alarmou voltou a byte[36]=0
    if (alarmAtivo && flag == 0 && laco == lacoAlarme) {
        alarmAtivo = false;
        const char* nome = nomePorLaco(lacoAlarme);
        Serial.printf("[NORMAL] b28=0x%02X restaurado.\n", lacoAlarme);
        if (millis() - tEnvio[lacoAlarme] >= COOLDOWN_MS) {
            if (enviarTelegram(montarMensagem(false, nome)))
                tEnvio[lacoAlarme] = millis();
        }
    }
}

// ── Parser ────────────────────────────────────────
void parseByte(uint8_t b) {
    if (b == FRAME_START) { framePos = 0; frameBuf[0] = b; return; }
    if (framePos < 0) return;
    framePos++;
    if (framePos >= FRAME_LEN) { framePos = -1; return; }
    frameBuf[framePos] = b;
    if (b == FRAME_END && framePos == FRAME_LEN - 1) { processarFrame(); framePos = -1; }
}

// ── WiFi ──────────────────────────────────────────
void enviarConectado() {
    delay(3000);
    IPAddress ip;
    if (!WiFi.hostByName("api.telegram.org", ip)) return;
    String msg = "\xF0\x9F\x9F\xA2 <b>SUPERVISOR CONECTADO</b>\n\n";
    msg += "\xF0\x9F\x8C\x90 IP: <code>" + WiFi.localIP().toString() + "</code>\n";
#if DIAG_MODE
    msg += "<i>DIAG_MODE ativo \xE2\x80\x94 veja Monitor Serial.</i>";
#else
    msg += "<i>CIE 1125 RS485 | aguardando eventos.</i>";
#endif
    enviarTelegram(msg);
}

void conectarWiFi() {
    Serial.print("[WiFi] Conectando");
    WiFi.mode(WIFI_STA); WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 40) { delay(500); Serial.print("."); t++; }
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
                    IPAddress(8,8,8,8), IPAddress(8,8,4,4));
        Serial.println("\n[WiFi] IP: " + WiFi.localIP().toString());
        enviarConectado();
    } else { Serial.println("\n[WiFi] Falha."); }
    tUltimaReconexao = millis();
}

// ── Setup / Loop ──────────────────────────────────
void setup() {
    Serial.begin(115200);
    memset(tEnvio, 0, sizeof(tEnvio));
    Serial2.begin(BAUD_RS485, SERIAL_8N1, PIN_RX, PIN_TX);
    Serial.println("\n=== CIE 1125 RS485 \xE2\x86\x92 Telegram ===");
    Serial.printf("[RS485] %d bps  RX=GPIO%d\n", BAUD_RS485, PIN_RX);
#if DIAG_MODE
    Serial.println("[DIAG] Acione cada botoeira e anote o b28 que aparece.");
    Serial.println("[DIAG] Esse valor vai em DISPOSITIVOS[] no config.h\n");
#endif
    conectarWiFi();
    Serial.println("[OK] Aguardando frames RS485...\n");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED)
        if (millis() - tUltimaReconexao > 30000) conectarWiFi();
    while (Serial2.available()) parseByte((uint8_t)Serial2.read());
}
