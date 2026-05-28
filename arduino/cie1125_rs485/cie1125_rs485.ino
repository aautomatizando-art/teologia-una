/*
 * CIE 1125 (Intelbras) RS485 Repetidora → Telegram  |  ESP32
 *
 * DIAG_MODE = true  → imprime bytes-chave de cada frame (sem enviar Telegram).
 *                      Acione a botoeira e observe quais bytes mudam.
 *                      Anote os valores e ajuste IDX_ALARME e DISPOSITIVOS.
 * DIAG_MODE = false → operação normal.
 *
 * Ligação:
 *   CIE D+ → MAX485 A (borne verde)
 *   CIE D- → MAX485 B (borne verde)
 *   MAX485 RO    → ESP32 GPIO16
 *   MAX485 RE+DE → GND  (recepção permanente)
 *
 * Pré-requisito na central:
 *   Sistema → Configurações → Habilitar Repetidoras → R1 = ON
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ── Protocolo RS485 ───────────────────────────────
#define FRAME_START  0x7E
#define FRAME_END    0x7C
#define FRAME_LEN    42

// Posições a observar no DIAG_MODE (ajuste após análise):
#define IDX_B8    8
#define IDX_B10   10
#define IDX_B28   28
#define IDX_B29   29
#define IDX_B36   36

// Byte que indica alarme ATIVO (ajuste após diagnóstico):
// Na versão atual usamos byte[36]; mude para o byte correto após ver o DIAG.
#define IDX_ALARME   36
#define VAL_ALARME   1    // valor que indica alarme (ajuste se necessário)

// ── Globals ───────────────────────────────────────
uint8_t  frameBuf[FRAME_LEN];
int      framePos = -1;

// Operação normal
bool     sistemaPronto = false;
bool     alarmAtivo    = false;
uint8_t  lacoAlarme    = 0;
uint8_t  endAlarme     = 0;
unsigned long tEnvio[256];
unsigned long tUltimaReconexao = 0;

// ── Lookup de dispositivo por byte[28] ────────────
const char* nomePorLaco(uint8_t laco) {
    for (int i = 0; DISPOSITIVOS[i].nome != nullptr; i++) {
        if (DISPOSITIVOS[i].laco == laco) return DISPOSITIVOS[i].nome;
    }
    return nullptr;
}

// ── Telegram ──────────────────────────────────────
String htmlEsc(const String& s) {
    String r = s; r.replace("&","&amp;"); r.replace("<","&lt;"); r.replace(">","&gt;");
    return r;
}

String montarMensagem(bool alarme, const char* nomeDisp) {
    String emoji  = alarme ? "\xF0\x9F\x94\xA5" : "\xE2\x9C\x85";
    String titulo = alarme ? "ALARME DE INC\xC3\x8ANDIO" : "NORMALIZADO";
    String msg    = emoji + " <b>" + titulo + "</b>\n\n";
    if (nomeDisp)
        msg += "\xF0\x9F\x93\x8B " + htmlEsc(String(nomeDisp)) + "\n";
    char up[10]; unsigned long s = millis()/1000;
    snprintf(up, sizeof(up), "%02lu:%02lu:%02lu", s/3600, (s%3600)/60, s%60);
    msg += "\xF0\x9F\x95\x90 Uptime: " + String(up) + "\n";
    msg += "\n<i>Central: CIE 1125 | Intelbras</i>";
    return msg;
}

bool enviarTelegram(const String& mensagem) {
    String url = "https://api.telegram.org/bot"; url += TELEGRAM_TOKEN; url += "/sendMessage";
    for (int t = 1; t <= 3; t++) {
        Serial.printf("[TG] Tentativa %d/3\n", t);
        WiFiClientSecure c; c.setInsecure(); c.setTimeout(15);
        HTTPClient http; http.begin(c, url);
        http.addHeader("Content-Type","application/json"); http.setTimeout(15000);
        StaticJsonDocument<512> doc;
        doc["chat_id"] = TELEGRAM_CHAT_ID; doc["text"] = mensagem; doc["parse_mode"] = "HTML";
        String body; serializeJson(doc, body);
        int code = http.POST(body); http.end();
        if (code == 200) { Serial.println("[TG] Enviado."); return true; }
        Serial.printf("[TG] Erro %d.%s\n", code, t < 3 ? " 4s..." : "");
        if (t < 3) delay(4000);
    }
    return false;
}

// ── Diagnóstico: mostra bytes-chave quando mudam ──
void diagnosticoFrame() {
    static uint8_t prev[5] = {0xFF,0xFF,0xFF,0xFF,0xFF};
    uint8_t cur[5] = {
        frameBuf[IDX_B8],
        frameBuf[IDX_B10],
        frameBuf[IDX_B28],
        frameBuf[IDX_B29],
        frameBuf[IDX_B36]
    };

    if (memcmp(cur, prev, 5) == 0) return;   // sem mudança — não imprime

    Serial.printf("[D] b8=%02X  b10=%02X  b28=%02X  b29=%02X  b36=%02X",
                  cur[0], cur[1], cur[2], cur[3], cur[4]);

    // Marca quais bytes mudaram em relação ao frame anterior
    if (prev[0] != 0xFF) {
        Serial.print("   <<");
        if (cur[0]!=prev[0]) Serial.printf(" b8:%02X→%02X", prev[0], cur[0]);
        if (cur[1]!=prev[1]) Serial.printf(" b10:%02X→%02X", prev[1], cur[1]);
        if (cur[2]!=prev[2]) Serial.printf(" b28:%02X→%02X", prev[2], cur[2]);
        if (cur[3]!=prev[3]) Serial.printf(" b29:%02X→%02X", prev[3], cur[3]);
        if (cur[4]!=prev[4]) Serial.printf(" b36:%02X→%02X", prev[4], cur[4]);
        Serial.print(" >>");
    }
    Serial.println();
    memcpy(prev, cur, 5);
}

// ── Operação normal ───────────────────────────────
void operacaoNormal() {
    uint8_t flagAlarme = frameBuf[IDX_ALARME];
    uint8_t laco       = frameBuf[IDX_B28];
    uint8_t end_       = frameBuf[IDX_B29];

    if (!sistemaPronto) {
        if (flagAlarme == 0) {
            sistemaPronto = true;
            Serial.println("[INIT] Sistema normal. Monitoramento ativo.");
        }
        return;
    }

    // Alarme detectado
    if (!alarmAtivo && flagAlarme == VAL_ALARME) {
        alarmAtivo = true;
        lacoAlarme = laco;
        endAlarme  = end_;
        const char* nome = nomePorLaco(laco);
        Serial.printf("[ALARME] b28=0x%02X b29=0x%02X → %s\n",
                      laco, end_, nome ? nome : "(não mapeado)");
        if (millis() - tEnvio[laco] >= COOLDOWN_MS) {
            if (enviarTelegram(montarMensagem(true, nome)))
                tEnvio[laco] = millis();
        }
    }

    // Normalizado
    if (alarmAtivo && flagAlarme == 0) {
        alarmAtivo = false;
        const char* nome = nomePorLaco(lacoAlarme);
        Serial.printf("[NORMAL] b28=0x%02X restaurado.\n", lacoAlarme);
        if (millis() - tEnvio[lacoAlarme] >= COOLDOWN_MS) {
            if (enviarTelegram(montarMensagem(false, nome)))
                tEnvio[lacoAlarme] = millis();
        }
    }
}

// ── Processa frame completo ───────────────────────
void processarFrame() {
#if DIAG_MODE
    diagnosticoFrame();
#else
    operacaoNormal();
#endif
}

// ── Parser de frame byte a byte ───────────────────
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
    msg += "<i>DIAG_MODE ativo — sem notificações de alarme.</i>";
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

// ── Setup ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    memset(tEnvio, 0, sizeof(tEnvio));
    Serial2.begin(BAUD_RS485, SERIAL_8N1, PIN_RX, PIN_TX);
    Serial.println("\n=== CIE 1125 RS485 \xE2\x86\x92 Telegram ===");
    Serial.printf("[RS485] %d bps  RX=GPIO%d\n", BAUD_RS485, PIN_RX);
#if DIAG_MODE
    Serial.println("[DIAG] Modo diagnóstico ativo.");
    Serial.println("[DIAG] Acione a botoeira e observe quais bytes mudam.");
    Serial.println("[DIAG] Formato: b8  b10  b28  b29  b36");
    Serial.println("[DIAG] << mudancas marcadas com >> \n");
#endif
    conectarWiFi();
    Serial.println("[OK] Aguardando frames RS485...\n");
}

// ── Loop ──────────────────────────────────────────
void loop() {
    if (WiFi.status() != WL_CONNECTED)
        if (millis() - tUltimaReconexao > 30000) conectarWiFi();
    while (Serial2.available()) parseByte((uint8_t)Serial2.read());
}
