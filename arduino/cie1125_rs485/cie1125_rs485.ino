/*
 * CIE 1125 (Intelbras) RS485 Repetidora → Telegram  |  ESP32
 *
 * Lê o barramento RS485 da saída Repetidora (bornes D+ / D-) da CIE 1125.
 * Protocolo confirmado em campo: frames de 42 bytes, delimitados por 0x7E (início)
 * e 0x7C (fim).
 *
 * Posições relevantes no frame (índice 0 = byte START inclusive):
 *   [10] → contador global de alarmes ativos (0 = sem alarme, N = N alarmes)
 *   [28] → Laço do dispositivo deste frame  ← usado para identificação
 *   [29] → Endereço dentro do Laço          (auxiliar — varia com o poll)
 *
 * Estratégia de detecção:
 *   - Aguarda ctAlarmes=0 ao menos uma vez (sistemaPronto) antes de monitorar.
 *   - Quando ctAlarmes passa de 0→N, varre frames até encontrar Laço mapeado.
 *   - Quando ctAlarmes volta a 0, envia NORMALIZADO.
 *   - Cooldown independente por Laço — eventos de laços desconhecidos não
 *     bloqueiam a notificação dos laços mapeados.
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
#define FRAME_LEN     42

#define IDX_ALARMES   10    // contador global de alarmes
#define IDX_LACO      28    // Laço do dispositivo neste frame
#define IDX_ENDERECO  29    // Endereço do dispositivo neste frame

// ── Globals ───────────────────────────────────────
uint8_t  frameBuf[FRAME_LEN];
int      framePos = -1;

bool     sistemaPronto = false;   // true após primeiro ctAlarmes==0
bool     alarmAtivo    = false;
bool     buscando      = false;   // ctAlarmes>0 mas Laço ainda não identificado
uint8_t  lacoAlarme    = 0;
uint8_t  endAlarme     = 0;

// Cooldown independente por Laço (índice = número do Laço, 0-255)
unsigned long tEnvio[256];

unsigned long tUltimaReconexao = 0;

// ── Lookup de dispositivo por Laço ────────────────
const char* nomePorLaco(uint8_t laco) {
    for (int i = 0; DISPOSITIVOS[i].nome != nullptr; i++) {
        if (DISPOSITIVOS[i].laco == laco) return DISPOSITIVOS[i].nome;
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

String montarMensagem(bool alarme, const char* nomeDisp) {
    String emoji  = alarme ? "\xF0\x9F\x94\xA5" : "\xE2\x9C\x85";
    String titulo = alarme ? "ALARME DE INC\xC3\x8ANDIO" : "NORMALIZADO";

    String msg = emoji + " <b>" + titulo + "</b>\n\n";
    if (nomeDisp) {
        msg += "\xF0\x9F\x93\x8B " + htmlEsc(String(nomeDisp)) + "\n";
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

    // Fase 1: aguarda sistema em estado limpo antes de monitorar
    if (!sistemaPronto) {
        if (ctAlarmes == 0) {
            sistemaPronto = true;
            Serial.println("[INIT] Sistema normal. Monitoramento ativo.");
        } else {
            Serial.printf("[INIT] Aguardando (alarmes=%d la\xC3\xA7o=%02X)...\n",
                          ctAlarmes, laco);
        }
        return;
    }

    // Fase 2: detecção de início de alarme
    if (ctAlarmes > 0 && !alarmAtivo) {
        buscando = true;
    }

    // Fase 3: varre frames para encontrar Laço mapeado
    if (buscando && ctAlarmes > 0) {
        const char* nome = nomePorLaco(laco);
        if (nome) {
            buscando   = false;
            alarmAtivo = true;
            lacoAlarme = laco;
            endAlarme  = end_;

            Serial.printf("[ALARME] La\xC3\xA7o=0x%02X End=0x%02X \xE2\x86\x92 %s\n",
                          laco, end_, nome);

            if (millis() - tEnvio[laco] >= COOLDOWN_MS) {
                if (enviarTelegram(montarMensagem(true, nome)))
                    tEnvio[laco] = millis();
            }
        } else {
            Serial.printf("[SCAN] La\xC3\xA7o=0x%02X End=0x%02X (n\xC3\xA3o mapeado)\n",
                          laco, end_);
        }
    }

    // Fase 4: sistema normalizado
    if (ctAlarmes == 0) {
        buscando = false;
        if (alarmAtivo) {
            alarmAtivo = false;
            const char* nome = nomePorLaco(lacoAlarme);
            Serial.printf("[NORMAL] La\xC3\xA7o=0x%02X restaurado.\n", lacoAlarme);

            if (millis() - tEnvio[lacoAlarme] >= COOLDOWN_MS) {
                if (enviarTelegram(montarMensagem(false, nome)))
                    tEnvio[lacoAlarme] = millis();
            }
        }
    }
}

// ── Parser de frame byte a byte ───────────────────
void parseByte(uint8_t b) {
    if (b == FRAME_START) {
        framePos    = 0;
        frameBuf[0] = b;
        return;
    }
    if (framePos < 0) return;

    framePos++;
    if (framePos >= FRAME_LEN) { framePos = -1; return; }
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
    memset(tEnvio, 0, sizeof(tEnvio));
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
