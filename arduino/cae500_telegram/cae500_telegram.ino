/*
 * CAE-500 XMAX → Telegram
 * ESP32 lê eventos da central Ilumac via RS232 (MAX3232)
 * e envia mensagens formatadas para um grupo do Telegram.
 *
 * Bibliotecas necessárias (Arduino IDE → Gerenciar Bibliotecas):
 *   - ArduinoJson  (by Benoit Blanchon)
 *
 * Ligações:
 *   CAE-500 DB9 pino 2 (TX) → MAX3232 R1IN
 *   MAX3232 R1OUT            → ESP32 GPIO16 (RX2)
 *   MAX3232 VCC              → ESP32 3.3V   ← IMPORTANTE: 3.3V, não 5V!
 *   MAX3232 GND              → ESP32 GND
 */

#include <WiFi.h>
#include <WiFiUDP.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include "lwip/dns.h"    // acesso direto à pilha de rede
#include "config.h"

// ── Globals ───────────────────────────────────────
String bufferSerial = "";
std::map<int, unsigned long> ultimoEnvio;   // cooldown por endereço
unsigned long tUltimaReconexao = 0;

// ── Estrutura de evento ───────────────────────────
struct Evento {
    int    address;
    String tipo;
    String descricao;
    String hora;
    String data;
};

// ── Parser de linha da central ────────────────────
// Formato: "DD/MM/AAAA HH:MM:SS  ALARME  END:042  BOTOEIRA PAV 3"
Evento parseLine(const String& linha) {
    Evento ev;
    ev.address = -1;

    int idxEnd = linha.indexOf("END:");
    if (idxEnd == -1) return ev;

    // Endereço
    String numStr = "";
    int i = idxEnd + 4;
    while (i < (int)linha.length() && isDigit(linha[i])) numStr += linha[i++];
    if (numStr.isEmpty()) return ev;
    ev.address = numStr.toInt();

    // Tipo
    if      (linha.indexOf("ALARME")  != -1) ev.tipo = "ALARME";
    else if (linha.indexOf("FALHA")   != -1) ev.tipo = "FALHA";
    else if (linha.indexOf("NORMAL")  != -1) ev.tipo = "NORMAL";
    else if (linha.indexOf("RETORNO") != -1) ev.tipo = "NORMAL";
    else if (linha.indexOf("SUPERV")  != -1) ev.tipo = "SUPERVISAO";
    else                                      ev.tipo = "EVENTO";

    // Descrição: tudo após "END:NNN  "
    while (i < (int)linha.length() && !isAlphaNumeric(linha[i])) i++;
    ev.descricao = linha.substring(i);
    ev.descricao.trim();

    // Data e hora
    for (int j = 0; j < (int)linha.length() - 7; j++) {
        if (isDigit(linha[j]) && linha[j+2] == ':' && isDigit(linha[j+3]) &&
            linha[j+5] == ':' && isDigit(linha[j+6])) {
            ev.hora = linha.substring(j, j + 8);
        }
        if (isDigit(linha[j]) && linha[j+2] == '/' && isDigit(linha[j+3]) &&
            linha[j+5] == '/' && isDigit(linha[j+6])) {
            ev.data = linha.substring(j, j + 10);
        }
    }

    return ev;
}

// ── Escapa caracteres especiais para HTML do Telegram ──
String htmlEsc(const String& s) {
    String r = s;
    r.replace("&", "&amp;");
    r.replace("<", "&lt;");
    r.replace(">", "&gt;");
    return r;
}

// ── Monta mensagem formatada ──────────────────────
String montarMensagem(const Evento& ev) {
    String emoji, titulo;

    if      (ev.tipo == "ALARME")    { emoji = "\xF0\x9F\x94\xA5"; titulo = "ALARME DE INC\xC3\x8ANDIO"; }
    else if (ev.tipo == "FALHA")     { emoji = "\xE2\x9A\xA0\xEF\xB8\x8F";  titulo = "FALHA";             }
    else if (ev.tipo == "NORMAL")    { emoji = "\xE2\x9C\x85";     titulo = "NORMALIZADO";    }
    else if (ev.tipo == "SUPERVISAO"){ emoji = "\xF0\x9F\x94\x94"; titulo = "SUPERVIS\xC3\x83O";       }
    else                             { emoji = "\xE2\x84\xB9\xEF\xB8\x8F";  titulo = "EVENTO";            }

    String addr = String(ev.address);
    while (addr.length() < 3) addr = "0" + addr;

    String msg = emoji + " <b>" + titulo + "</b>\n\n";
    msg += "\xF0\x9F\x93\x8D <b>Endere\xC3\xA7o:</b> <code>" + addr + "</code>\n";
    msg += "\xF0\x9F\x93\x8B " + htmlEsc(ev.descricao) + "\n";

    if (ev.hora.length() > 0) {
        msg += "\xF0\x9F\x95\x90 " + ev.hora;
        if (ev.data.length() > 0) msg += " \xE2\x80\x94 " + ev.data;
        msg += "\n";
    }

    msg += "\n<i>Central: CAE-500 XMAX</i>";
    return msg;
}

// ── Envia mensagem ao Telegram ────────────────────
bool enviarTelegram(const String& mensagem) {
    StaticJsonDocument<1024> doc;
    doc["chat_id"]    = TELEGRAM_CHAT_ID;
    doc["text"]       = mensagem;
    doc["parse_mode"] = "HTML";
    String body;
    serializeJson(doc, body);

    String url = "https://api.telegram.org/bot";
    url += TELEGRAM_TOKEN;
    url += "/sendMessage";

    for (int tentativa = 1; tentativa <= 3; tentativa++) {
        Serial.printf("[TG] Tentativa %d/3  heap livre: %d bytes\n",
                      tentativa, ESP.getFreeHeap());

        WiFiClientSecure cliente;
        cliente.setInsecure();
        cliente.setTimeout(15);

        HTTPClient http;
        http.begin(cliente, url);
        http.addHeader("Content-Type", "application/json");
        http.setTimeout(15000);

        int code = http.POST(body);
        http.end();

        if (code == 200) {
            Serial.println("[TG] Mensagem enviada.");
            return true;
        }

        Serial.printf("[TG] Falhou (HTTP %d). %s\n",
                      code,
                      tentativa < 3 ? "Aguardando 4 s..." : "Desistindo.");
        if (tentativa < 3) delay(4000);
    }
    return false;
}

// ── Verifica cooldown do endereço ─────────────────
bool podEnviar(int address) {
    auto it = ultimoEnvio.find(address);
    if (it == ultimoEnvio.end()) return true;
    return (millis() - it->second) >= COOLDOWN_MS;
}

void registrarEnvio(int address) {
    ultimoEnvio[address] = millis();
}

// ── Processa linha recebida ───────────────────────
void processar(const String& linha) {
    if (linha.length() < 10) return;

    Serial.println("[RX] " + linha);

    Evento ev = parseLine(linha);
    if (ev.address < 0) {
        Serial.println("[PARSE] Linha sem END: ignorada.");
        return;
    }

    // Filtra por tipo e cooldown
    bool deveEnviar = false;
    if (ev.tipo == "ALARME")    deveEnviar = true;
    if (ev.tipo == "FALHA"     && ENVIAR_FALHA)  deveEnviar = true;
    if (ev.tipo == "NORMAL"    && ENVIAR_NORMAL) deveEnviar = true;
    if (ev.tipo == "SUPERVISAO")                  deveEnviar = true;

    if (!deveEnviar) return;

    // Alarmes NUNCA têm cooldown — sempre notificar
    if (ev.tipo != "ALARME" && !podEnviar(ev.address)) {
        Serial.printf("[TG] Cooldown ativo para End:%d\n", ev.address);
        return;
    }

    String msg = montarMensagem(ev);
    if (enviarTelegram(msg)) {
        registrarEnvio(ev.address);
    }
}

// ── Notifica conexão ao Telegram ──────────────────
void enviarConectado() {
    delay(4000);

    // Testa resolução DNS antes de tentar o Telegram
    IPAddress ipTelegram;
    Serial.print("[DNS] Resolvendo api.telegram.org... ");
    if (WiFi.hostByName("api.telegram.org", ipTelegram)) {
        Serial.println("OK → " + ipTelegram.toString());
    } else {
        Serial.println("FALHOU! Verifique a rede.");
        return;
    }

    String ip  = WiFi.localIP().toString();
    String msg = "\xF0\x9F\x9F\xA2 <b>CENTRAL CONECTADA</b>\n\n";
    msg += "\xF0\x9F\x93\xA1 Sistema supervisório <b>online</b>\n";
    msg += "\xF0\x9F\x8C\x90 IP: <code>" + ip + "</code>\n";
    msg += "\xF0\x9F\x93\x8B Central: CAE-500 XMAX\n";
    msg += "\n<i>Monitoramento ativo. Aguardando eventos.</i>";
    Serial.println("[TG] Enviando notificação de conexão...");
    enviarTelegram(msg);
}

// ── WiFi ──────────────────────────────────────────
// Sobrescreve o DNS diretamente na pilha lwIP,
// após o DHCP terminar (garante que não será sobrescrito)
void forceDNS() {
    ip_addr_t dns1, dns2;
    IP4_ADDR(&dns1, 8, 8, 8, 8);   // Google DNS primário
    IP4_ADDR(&dns2, 8, 8, 4, 4);   // Google DNS secundário
    dns_setserver(0, &dns1);
    dns_setserver(1, &dns2);
    Serial.println("[DNS] Servidores forçados: 8.8.8.8 / 8.8.4.4");
}

void conectarWiFi() {
    Serial.print("[WiFi] Conectando a ");
    Serial.print(WIFI_SSID);

    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }

    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Conectado! IP: " + WiFi.localIP().toString());
        forceDNS();      // sobrescreve DNS do DHCP antes de qualquer requisição
        enviarConectado();
    } else {
        Serial.println("\n[WiFi] Falha na conexão.");
    }
    tUltimaReconexao = millis();
}

// ── Setup ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial2.begin(BAUD_CENTRAL, SERIAL_8N1, PIN_RX2, PIN_TX2);

    Serial.println("\n=== CAE-500 XMAX → Telegram ===");
    conectarWiFi();

    Serial.printf("[SER] Serial2 aberta: %d bps  RX=GPIO%d\n", BAUD_CENTRAL, PIN_RX2);
    Serial.println("[OK] Aguardando eventos da central...\n");
}

// ── Loop ──────────────────────────────────────────
void loop() {
    // Reconexão WiFi automática
    if (WiFi.status() != WL_CONNECTED) {
        if (millis() - tUltimaReconexao > 30000) {
            Serial.println("[WiFi] Reconectando...");
            conectarWiFi();
        }
    }

    // Leitura da central
    while (Serial2.available()) {
        char c = (char)Serial2.read();
        if (c == '\n' || c == '\r') {
            bufferSerial.trim();
            if (bufferSerial.length() > 0) {
                processar(bufferSerial);
            }
            bufferSerial = "";
        } else {
            if (bufferSerial.length() < 200) bufferSerial += c;
        }
    }
}
