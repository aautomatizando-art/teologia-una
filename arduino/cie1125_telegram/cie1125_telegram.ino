/*
 * CIE 1125 (Intelbras) → Telegram  |  ESP32
 *
 * A CIE 1125 expõe eventos via Modbus UDP (porta 502).
 * O ESP32 consulta a central pela rede WiFi e envia ao Telegram
 * quando detecta mudanças nos registradores de alarme/falha.
 *
 * NÃO usa RS232 / MAX3232 — a ligação é toda via rede.
 *
 * Ligação física:
 *   CIE 1125 RJ45 → switch/roteador → (WiFi) → ESP32
 *
 * Pré-requisitos na central:
 *   Menu → Rede → habilitar Modbus UDP
 *   Menu → Rede → definir IP fixo (recomendado)
 *
 * Bibliotecas:
 *   ArduinoJson (Benoit Blanchon) – instalar via Gerenciar Bibliotecas
 *   WiFi, WiFiUDP, WiFiClientSecure, HTTPClient – inclusas no core ESP32
 */

#include <WiFi.h>
#include <WiFiUDP.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include "config.h"

// ── Constantes Modbus ─────────────────────────────
#define MB_FC_READ_HOLDING  0x03
#define MB_REGISTERS_PER_BATCH  50   // registradores por leitura UDP
#define MB_TIMEOUT_MS           500  // tempo máximo de espera da resposta

// ── Registradores de resumo (sem máscara de bit) ──
// Estes valores são uint16 diretos (verify no manual – Anexo A)
#define REG_TOTAL_ALARMES  200
#define REG_TOTAL_FALHAS   202

// ── Globals ───────────────────────────────────────
WiFiUDP   udp;
uint8_t   rxBuf[512];

// Snapshot anterior dos registradores (para detectar mudança)
uint16_t  regAnterior[256] = {0};
uint16_t  regAtual[256]    = {0};

uint16_t  alarmeAnterior = 0;
uint16_t  falhaAnterior  = 0;

uint16_t  transId = 1;
unsigned long tUltimoPoll      = 0;
unsigned long tUltimaReconexao = 0;

std::map<String, unsigned long> cooldownMap;

// ── Modbus UDP – monta pacote de leitura ──────────
int modbusReadHolding(uint16_t startReg, uint16_t count,
                       uint16_t* outBuf) {
    uint8_t req[12];
    req[0]  = transId >> 8;   req[1]  = transId & 0xFF;
    req[2]  = 0x00;           req[3]  = 0x00;   // Protocol ID
    req[4]  = 0x00;           req[5]  = 0x06;   // Length
    req[6]  = MODBUS_UNIT_ID;
    req[7]  = MB_FC_READ_HOLDING;
    req[8]  = startReg >> 8;  req[9]  = startReg & 0xFF;
    req[10] = count >> 8;     req[11] = count & 0xFF;
    transId++;

    udp.beginPacket(CIE_IP, CIE_PORT);
    udp.write(req, 12);
    udp.endPacket();

    unsigned long t0 = millis();
    while (millis() - t0 < MB_TIMEOUT_MS) {
        int n = udp.parsePacket();
        if (n > 9) {
            udp.read(rxBuf, n);
            // Valida: FC correto, sem código de exceção
            if (rxBuf[7] != MB_FC_READ_HOLDING) return -1;
            uint8_t byteCount = rxBuf[8];
            for (int i = 0; i < byteCount / 2 && i < (int)count; i++) {
                outBuf[i] = ((uint16_t)rxBuf[9 + i*2] << 8) | rxBuf[10 + i*2];
            }
            return byteCount / 2;
        }
        delay(10);
    }
    return -1;  // timeout
}

// ── Lê todos os registradores monitorados ─────────
bool lerRegistradores() {
    bool ok = true;

    // Lê em blocos para não exceder o tamanho de pacote UDP
    for (int base = 0; base < 256; base += MB_REGISTERS_PER_BATCH) {
        uint16_t count = min(MB_REGISTERS_PER_BATCH, 256 - base);
        uint16_t tmpBuf[MB_REGISTERS_PER_BATCH];
        int n = modbusReadHolding(base, count, tmpBuf);
        if (n > 0) {
            for (int i = 0; i < n; i++) {
                regAtual[base + i] = tmpBuf[i];
            }
        } else {
            ok = false;
        }
        delay(20);  // pequena pausa entre leituras
    }
    return ok;
}

// ── Telegram ──────────────────────────────────────
String htmlEsc(const String& s) {
    String r = s;
    r.replace("&", "&amp;");
    r.replace("<", "&lt;");
    r.replace(">", "&gt;");
    return r;
}

bool enviarTelegram(const String& mensagem) {
    WiFiClientSecure cliente;
    cliente.setInsecure();
    HTTPClient http;
    String url = "https://api.telegram.org/bot";
    url += TELEGRAM_TOKEN;
    url += "/sendMessage";

    http.begin(cliente, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    StaticJsonDocument<1024> doc;
    doc["chat_id"]    = TELEGRAM_CHAT_ID;
    doc["text"]       = mensagem;
    doc["parse_mode"] = "HTML";

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    http.end();

    if (code == 200) {
        Serial.println("[TG] Enviado.");
        return true;
    }
    Serial.printf("[TG] Erro HTTP %d\n", code);
    return false;
}

String montarMensagem(const char* emoji, const char* titulo,
                       const char* nome, const char* tipo,
                       uint16_t reg, uint8_t bit) {
    String msg = String(emoji) + " <b>" + titulo + "</b>\n\n";
    msg += "\xF0\x9F\x93\x8B " + htmlEsc(String(nome)) + "\n";
    msg += "\xF0\x9F\x94\xA7 Tipo: " + htmlEsc(String(tipo)) + "\n";
    msg += "\xF0\x9F\x93\x8D Reg: <code>" + String(reg) +
           "</code>  Bit: <code>" + String(bit) + "</code>\n";

    char hora[9];
    // ESP32 não tem RTC, mostra millis como referência
    unsigned long s = millis() / 1000;
    sprintf(hora, "%02lu:%02lu:%02lu", s/3600, (s%3600)/60, s%60);
    msg += "\xF0\x9F\x95\x90 Uptime: " + String(hora) + "\n";
    msg += "\n<i>Central: CIE 1125 | Intelbras</i>";
    return msg;
}

bool podEnviar(const String& chave) {
    auto it = cooldownMap.find(chave);
    if (it == cooldownMap.end()) return true;
    return (millis() - it->second) >= COOLDOWN_MS;
}

// ── Analisa mudanças e notifica ───────────────────
void analisarMudancas() {
    // 1. Verifica contadores de resumo
    uint16_t alarmeAtual = regAtual[REG_TOTAL_ALARMES];
    uint16_t falhaAtual  = regAtual[REG_TOTAL_FALHAS];

    if (alarmeAtual != alarmeAnterior) {
        Serial.printf("[CIE] Alarmes: %d → %d\n", alarmeAnterior, alarmeAtual);
        alarmeAnterior = alarmeAtual;
    }
    if (falhaAtual != falhaAnterior) {
        Serial.printf("[CIE] Falhas: %d → %d\n", falhaAnterior, falhaAtual);
        falhaAnterior = falhaAtual;
    }

    // 2. Verifica dispositivos configurados no config.h
    for (int i = 0; DISPOSITIVOS[i].nome != nullptr; i++) {
        const Dispositivo& d = DISPOSITIVOS[i];
        uint16_t r = d.regAddr;

        bool eraAtivo  = (regAnterior[r] & d.bitMask) != 0;
        bool estaAtivo = (regAtual[r]    & d.bitMask) != 0;

        if (estaAtivo == eraAtivo) continue;

        String chave = String(r) + "_" + String(d.bitMask);
        bool   ativou = estaAtivo && !eraAtivo;

        Serial.printf("[CIE] %s %s\n",
                      d.nome, ativou ? "ATIVADO" : "NORMALIZADO");

        if (ativou && ENVIAR_ALARME && strcmp(d.tipoEvento, "ALARME") == 0) {
            String msg = montarMensagem(
                "\xF0\x9F\x94\xA5", "ALARME DE INC\xC3\x8ANDIO",
                d.nome, d.tipoEvento, r,
                __builtin_ctz(d.bitMask));  // número do bit
            enviarTelegram(msg);
            cooldownMap[chave] = millis();

        } else if (ativou && ENVIAR_FALHA && strcmp(d.tipoEvento, "FALHA") == 0) {
            String msg = montarMensagem(
                "\xE2\x9A\xA0\xEF\xB8\x8F", "FALHA",
                d.nome, d.tipoEvento, r,
                __builtin_ctz(d.bitMask));
            if (podEnviar(chave)) {
                enviarTelegram(msg);
                cooldownMap[chave] = millis();
            }

        } else if (!ativou && ENVIAR_NORMAL) {
            String msg = montarMensagem(
                "\xE2\x9C\x85", "NORMALIZADO",
                d.nome, d.tipoEvento, r,
                __builtin_ctz(d.bitMask));
            if (podEnviar(chave)) {
                enviarTelegram(msg);
                cooldownMap[chave] = millis();
            }
        }
    }

    // 3. Salva snapshot atual
    memcpy(regAnterior, regAtual, sizeof(regAtual));
}

// ── WiFi ──────────────────────────────────────────
void conectarWiFi() {
    Serial.print("[WiFi] Conectando");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 40) {
        delay(500);
        Serial.print(".");
        t++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] IP: " + WiFi.localIP().toString());
        udp.begin(0);   // porta local aleatória
    } else {
        Serial.println("\n[WiFi] Falha.");
    }
    tUltimaReconexao = millis();
}

// ── Setup ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== CIE 1125 → Telegram ===");
    conectarWiFi();
    Serial.printf("[CIE] Central: %s:%d\n", CIE_IP, CIE_PORT);
    Serial.println("[OK] Iniciando polling Modbus...\n");
}

// ── Loop ──────────────────────────────────────────
void loop() {
    // Reconexão WiFi
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - tUltimaReconexao > 30000) conectarWiFi();
        return;
    }

    // Polling periódico
    if (millis() - tUltimoPoll >= POLL_INTERVAL_MS) {
        tUltimoPoll = millis();

        if (lerRegistradores()) {
            analisarMudancas();
        } else {
            Serial.println("[CIE] Timeout Modbus – central indisponível?");
        }
    }
}
