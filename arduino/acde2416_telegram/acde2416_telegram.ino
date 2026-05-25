/*
 * ASCAEL ACDE 24/16 → Telegram
 * ESP32 lê eventos da central via RS232 (MAX3232) e envia
 * mensagens formatadas para um grupo do Telegram.
 *
 * Bibliotecas necessárias (Arduino IDE → Gerenciar Bibliotecas):
 *   - ArduinoJson  (by Benoit Blanchon)
 *
 * Ligações:
 *   ACDE 24/16 DB9 pino 2 (TX) → MAX3232 R1IN
 *   MAX3232 R1OUT               → ESP32 GPIO16 (RX2)
 *   MAX3232 VCC                 → ESP32 3.3V   ← IMPORTANTE: 3.3V, não 5V!
 *   MAX3232 GND                 → ESP32 GND
 *
 * Se a central tiver DB9 fêmea (conector fêmea na central), use um
 * cabo DB9 macho–macho null-modem ou apenas o pino 2 e GND.
 *
 * PRIMEIRO USO → RAW_MODE true em config.h:
 *   Abra o Monitor Serial, acione uma botoeira e copie a saída.
 *   Depois ajuste o parser abaixo conforme o formato real da central.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <map>
#include "config.h"

// ── Globals ───────────────────────────────────────
String         bufferSerial     = "";
unsigned long  tUltimaReconexao = 0;

std::map<int, unsigned long> ultimoEnvio;   // cooldown por zona/endereço

// ── Estrutura de evento ───────────────────────────
struct Evento {
    int    zone;        // número da zona / endereço (-1 = não identificado)
    String tipo;        // "ALARME", "FALHA", "NORMAL", "SUPERVISAO", "EVENTO"
    String descricao;   // texto livre da central
    String hora;        // "HH:MM:SS" se disponível
    String data;        // "DD/MM/AAAA" se disponível
};

// ── Busca nome da zona no mapeamento ─────────────
const char* nomeZona(int numero) {
    for (int i = 0; ZONAS[i].descricao != nullptr; i++) {
        if (ZONAS[i].numero == numero) return ZONAS[i].descricao;
    }
    return nullptr;
}

// ── Parser da linha da central ────────────────────
//
// Formatos conhecidos de centrais ASCAEL / genéricas RS232:
//   A) "DD/MM/AAAA HH:MM:SS  ALARME  ZONA:01  BOTOEIRA PAV 1"
//   B) "ALARME  Z01  BOTOEIRA PAV 1  HH:MM:SS"
//   C) "01 ALARME INCENDIO 12:30:45"
//   D) "<01>FIRE<BOTOEIRA>"  (formato compacto com delimitadores)
//
// RAW_MODE captura a saída real — ajuste parseLine() conforme necessário.
Evento parseLine(const String& linha) {
    Evento ev;
    ev.zone = -1;

    // ── Tenta encontrar número de zona/endereço ─────────────────────────
    // Padrão 1: "ZONA:NNN" ou "ZONA NNN" ou "Z:NNN" ou "END:NNN"
    static const char* prefixos[] = { "ZONA:", "ZONA ", "Z:", "END:", "ZN:", nullptr };
    for (int p = 0; prefixos[p] != nullptr; p++) {
        int idx = linha.indexOf(prefixos[p]);
        if (idx == -1) continue;
        int i = idx + strlen(prefixos[p]);
        // pula espaços
        while (i < (int)linha.length() && linha[i] == ' ') i++;
        String num = "";
        while (i < (int)linha.length() && isDigit(linha[i])) num += linha[i++];
        if (!num.isEmpty()) { ev.zone = num.toInt(); break; }
    }

    // Padrão 2: número logo no início da linha "01 ALARME..."
    if (ev.zone < 0) {
        int i = 0;
        // pula espaços iniciais
        while (i < (int)linha.length() && linha[i] == ' ') i++;
        String num = "";
        while (i < (int)linha.length() && isDigit(linha[i])) num += linha[i++];
        // valida: após os dígitos deve vir espaço ou letra (não ponto decimal)
        if (!num.isEmpty() && i < (int)linha.length() && linha[i] != '.') {
            ev.zone = num.toInt();
        }
    }

    // ── Tipo do evento ──────────────────────────────────────────────────
    String lU = linha;
    lU.toUpperCase();

    if      (lU.indexOf("ALARME")  != -1) ev.tipo = "ALARME";
    else if (lU.indexOf("ALARM")   != -1) ev.tipo = "ALARME";
    else if (lU.indexOf("FIRE")    != -1) ev.tipo = "ALARME";
    else if (lU.indexOf("INCEND")  != -1) ev.tipo = "ALARME";
    else if (lU.indexOf("FALHA")   != -1) ev.tipo = "FALHA";
    else if (lU.indexOf("FAULT")   != -1) ev.tipo = "FALHA";
    else if (lU.indexOf("TROUBLE") != -1) ev.tipo = "FALHA";
    else if (lU.indexOf("NORMAL")  != -1) ev.tipo = "NORMAL";
    else if (lU.indexOf("RETORNO") != -1) ev.tipo = "NORMAL";
    else if (lU.indexOf("RESTORE") != -1) ev.tipo = "NORMAL";
    else if (lU.indexOf("SUPERV")  != -1) ev.tipo = "SUPERVISAO";
    else if (lU.indexOf("TEST")    != -1) ev.tipo = "SUPERVISAO";
    else                                   ev.tipo = "EVENTO";

    // ── Descrição ───────────────────────────────────────────────────────
    // Remove prefixos de data/hora e palavras-chave para obter texto limpo
    ev.descricao = linha;
    ev.descricao.trim();

    // ── Data e hora ─────────────────────────────────────────────────────
    for (int j = 0; j < (int)linha.length() - 7; j++) {
        // HH:MM:SS
        if (isDigit(linha[j]) && j+7 <= (int)linha.length() &&
            linha[j+2] == ':' && isDigit(linha[j+3]) &&
            linha[j+5] == ':' && isDigit(linha[j+6])) {
            ev.hora = linha.substring(j, j + 8);
        }
        // DD/MM/AAAA
        if (isDigit(linha[j]) && j+10 <= (int)linha.length() &&
            linha[j+2] == '/' && isDigit(linha[j+3]) &&
            linha[j+5] == '/' && isDigit(linha[j+6])) {
            ev.data = linha.substring(j, j + 10);
        }
    }

    return ev;
}

// ── Escapa HTML para Telegram ─────────────────────
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

    if      (ev.tipo == "ALARME")     { emoji = "\xF0\x9F\x94\xA5"; titulo = "ALARME DE INC\xC3\x8ANDIO"; }
    else if (ev.tipo == "FALHA")      { emoji = "\xE2\x9A\xA0\xEF\xB8\x8F";  titulo = "FALHA";             }
    else if (ev.tipo == "NORMAL")     { emoji = "\xE2\x9C\x85";     titulo = "NORMALIZADO";    }
    else if (ev.tipo == "SUPERVISAO") { emoji = "\xF0\x9F\x94\x94"; titulo = "SUPERVIS\xC3\x83O";       }
    else                              { emoji = "\xE2\x84\xB9\xEF\xB8\x8F";  titulo = "EVENTO";            }

    String msg = emoji + " <b>" + titulo + "</b>\n\n";

    // Zona
    if (ev.zone >= 0) {
        String zStr = String(ev.zone);
        while (zStr.length() < 2) zStr = "0" + zStr;
        msg += "\xF0\x9F\x93\x8D <b>Zona:</b> <code>" + zStr + "</code>";

        const char* nz = nomeZona(ev.zone);
        if (nz) msg += " \xE2\x80\x94 " + htmlEsc(String(nz));
        msg += "\n";
    }

    // Descrição
    msg += "\xF0\x9F\x93\x8B " + htmlEsc(ev.descricao) + "\n";

    // Horário
    if (ev.hora.length() > 0) {
        msg += "\xF0\x9F\x95\x90 " + ev.hora;
        if (ev.data.length() > 0) msg += " \xE2\x80\x94 " + ev.data;
        msg += "\n";
    }

    msg += "\n<i>Central: ASCAEL ACDE 24/16</i>";
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

// ── Cooldown ──────────────────────────────────────
bool podEnviar(int zone) {
    auto it = ultimoEnvio.find(zone);
    if (it == ultimoEnvio.end()) return true;
    return (millis() - it->second) >= COOLDOWN_MS;
}

void registrarEnvio(int zone) {
    ultimoEnvio[zone] = millis();
}

// ── Processa linha recebida ───────────────────────
void processar(const String& linha) {
    if (linha.length() < 4) return;

    // ── RAW MODE: mostra tudo no Serial e não filtra ──────────────────
    if (RAW_MODE) {
        Serial.println("[RAW] " + linha);
        // Mostra bytes em HEX para identificar caracteres não-imprimíveis
        Serial.print("      HEX:");
        for (int i = 0; i < (int)linha.length(); i++) {
            Serial.printf(" %02X", (uint8_t)linha[i]);
        }
        Serial.println();
        return;   // não envia ao Telegram em RAW_MODE
    }

    // ── Modo normal: parser + Telegram ───────────────────────────────
    Serial.println("[RX] " + linha);

    Evento ev = parseLine(linha);

    // Descarta linhas sem tipo relevante
    bool deveEnviar = false;
    if (ev.tipo == "ALARME"    && ENVIAR_ALARME) deveEnviar = true;
    if (ev.tipo == "FALHA"     && ENVIAR_FALHA)  deveEnviar = true;
    if (ev.tipo == "NORMAL"    && ENVIAR_NORMAL) deveEnviar = true;
    if (ev.tipo == "SUPERVISAO")                  deveEnviar = true;

    if (!deveEnviar) {
        Serial.println("[PARSE] Tipo ignorado: " + ev.tipo);
        return;
    }

    // Alarmes nunca têm cooldown — sempre notificar
    int chave = (ev.zone >= 0) ? ev.zone : -1;
    if (ev.tipo != "ALARME" && chave >= 0 && !podEnviar(chave)) {
        Serial.printf("[TG] Cooldown ativo para zona %d\n", chave);
        return;
    }

    String msg = montarMensagem(ev);
    if (enviarTelegram(msg)) {
        if (chave >= 0) registrarEnvio(chave);
    }
}

// ── Notifica conexão ao Telegram ──────────────────
void enviarConectado() {
    delay(4000);

    IPAddress ipTelegram;
    Serial.print("[DNS] Resolvendo api.telegram.org... ");
    if (WiFi.hostByName("api.telegram.org", ipTelegram)) {
        Serial.println("OK \xE2\x86\x92 " + ipTelegram.toString());
    } else {
        Serial.println("FALHOU! Verifique a rede.");
        return;
    }

    String ip  = WiFi.localIP().toString();
    String msg = "\xF0\x9F\x9F\xA2 <b>CENTRAL CONECTADA</b>\n\n";
    msg += "\xF0\x9F\x93\xA1 Sistema supervisório <b>online</b>\n";
    msg += "\xF0\x9F\x8C\x90 IP: <code>" + ip + "</code>\n";
    msg += "\xF0\x9F\x93\x8B Central: ASCAEL ACDE 24/16\n";

    if (RAW_MODE) {
        msg += "\n\xE2\x9A\xA0\xEF\xB8\x8F <i>RAW_MODE ativo — capturando dados brutos.\n";
        msg +=   "Veja o Monitor Serial e informe o formato para ajustar o parser.</i>";
    } else {
        msg += "\n<i>Monitoramento ativo. Aguardando eventos.</i>";
    }

    Serial.println("[TG] Enviando notificação de conexão...");
    enviarTelegram(msg);
}

// ── WiFi ──────────────────────────────────────────
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
        // Força DNS do Google após DHCP para evitar falhas de resolução
        WiFi.config(WiFi.localIP(),
                    WiFi.gatewayIP(),
                    WiFi.subnetMask(),
                    IPAddress(8, 8, 8, 8),
                    IPAddress(8, 8, 4, 4));
        Serial.println("\n[WiFi] Conectado! IP: " + WiFi.localIP().toString());
        Serial.println("[DNS] Forçado: 8.8.8.8 / 8.8.4.4");
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

    Serial.println("\n=== ASCAEL ACDE 24/16 \xE2\x86\x92 Telegram ===");
    if (RAW_MODE) {
        Serial.println("[AVISO] RAW_MODE ativo — exibindo dados brutos da central.");
        Serial.println("        Acione uma botoeira e copie a saida para ajustar o parser.");
    }

    conectarWiFi();

    Serial.printf("[SER] Serial2: %d bps  RX=GPIO%d\n", BAUD_CENTRAL, PIN_RX2);
    Serial.println("[OK] Aguardando eventos da central...\n");
}

// ── Loop ──────────────────────────────────────────
void loop() {
    // Reconexão WiFi automática (exceto em RAW_MODE, que não precisa)
    if (!RAW_MODE && WiFi.status() != WL_CONNECTED) {
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
