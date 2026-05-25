/*
 * CIE 1125 (Intelbras) → Relé → ESP32 → Telegram
 *
 * Lê as saídas de relé (contato seco) da central via optoacoplador PC817
 * e envia mensagem ao grupo Telegram quando detecta ativação ou normalização.
 *
 * Não usa rede com a central — ligação é somente por fio (relé).
 *
 * Circuito por relé:
 *   ESP32 3.3V ──[1kΩ]──── PC817 pino 1 (Anodo)
 *                          PC817 pino 2 (Catodo) ── Relé NO (normalmente aberto)
 *                                                   Relé COM              ── GND
 *   ESP32 GND ──────────── PC817 pino 3 (Emissor)
 *   ESP32 GPIOx ─────────── PC817 pino 4 (Coletor)   ← INPUT_PULLUP
 *
 * Lógica: relé FECHADO → LED do PC817 acende → transistor conduz → GPIO = LOW
 *         relé ABERTO  → LED apagado         → transistor corta  → GPIO = HIGH
 *
 * Bibliotecas:
 *   ArduinoJson (Benoit Blanchon) – instalar via Gerenciar Bibliotecas
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ── Estado de cada relé ───────────────────────────
struct EstadoRele {
    bool     ativo;           // estado atual confirmado
    bool     leituraRaw;      // leitura bruta do pino
    uint32_t tDebounce;       // momento da última mudança raw
    uint32_t tAtivou;         // momento em que ficou ativo (para calcular duração)
    uint32_t tUltimoEnvio;    // para cooldown
};

// Tamanho máximo do array (conta entradas até o sentinel)
#define MAX_RELES 16
static EstadoRele estado[MAX_RELES];
static int        nReles = 0;

// ── WiFi ──────────────────────────────────────────
static uint32_t tUltimaReconexao = 0;

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
    Serial.println(WiFi.status() == WL_CONNECTED
        ? "\n[WiFi] Conectado: " + WiFi.localIP().toString()
        : "\n[WiFi] Falha.");
    tUltimaReconexao = millis();
}

// ── Telegram ──────────────────────────────────────
String htmlEsc(const String& s) {
    String r = s;
    r.replace("&", "&amp;");
    r.replace("<", "&lt;");
    r.replace(">", "&gt;");
    return r;
}

// Formata duração em "Xh Ym Zs"
String formatarDuracao(uint32_t ms) {
    uint32_t s = ms / 1000;
    uint32_t m = s / 60;   s %= 60;
    uint32_t h = m / 60;   m %= 60;
    String r = "";
    if (h > 0) r += String(h) + "h ";
    if (m > 0) r += String(m) + "m ";
    r += String(s) + "s";
    return r;
}

bool enviarTelegram(const String& mensagem) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure cliente;
    cliente.setInsecure();
    HTTPClient http;

    String url = "https://api.telegram.org/bot";
    url += TELEGRAM_TOKEN;
    url += "/sendMessage";

    http.begin(cliente, url);
    http.addHeader("Content-Type", "application/json");
    http.setTimeout(10000);

    StaticJsonDocument<512> doc;
    doc["chat_id"]    = TELEGRAM_CHAT_ID;
    doc["text"]       = mensagem;
    doc["parse_mode"] = "HTML";

    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    http.end();

    Serial.printf("[TG] %s – HTTP %d\n",
                  code == 200 ? "OK" : "ERRO", code);
    return code == 200;
}

void notificar(int i, bool ativou) {
    const ReleConfig& r = RELES[i];

    String emoji, titulo;
    if (ativou) {
        if      (strcmp(r.tipo, "ALARME")    == 0) { emoji = "\xF0\x9F\x94\xA5"; titulo = "ALARME DE INC\xC3\x8ANDIO"; }
        else if (strcmp(r.tipo, "FALHA")     == 0) { emoji = "\xE2\x9A\xA0\xEF\xB8\x8F";  titulo = "FALHA NA CENTRAL";    }
        else                                       { emoji = "\xF0\x9F\x94\x94"; titulo = "SUPERVIS\xC3\x83O";         }
    } else {
        emoji  = "\xE2\x9C\x85";
        titulo = "NORMALIZADO";
    }

    String msg = emoji + " <b>" + titulo + "</b>\n\n";
    msg += "\xF0\x9F\x93\x8B " + htmlEsc(String(r.nome)) + "\n";

    if (!ativou) {
        uint32_t dur = millis() - estado[i].tAtivou;
        msg += "\xE2\x8F\xB1 Dura\xC3\xA7\xC3\xA3o: " + formatarDuracao(dur) + "\n";
    }

    msg += "\n<i>Central: CIE 1125 | Intelbras</i>";

    enviarTelegram(msg);
    estado[i].tUltimoEnvio = millis();
}

// ── Setup ─────────────────────────────────────────
void setup() {
    Serial.begin(115200);
    Serial.println("\n=== CIE 1125 Relé → Telegram ===");

    // Conta e inicializa relés
    for (nReles = 0; RELES[nReles].nome != nullptr; nReles++) {
        if (nReles >= MAX_RELES - 1) break;
        pinMode(RELES[nReles].pino, INPUT_PULLUP);
        bool raw = (digitalRead(RELES[nReles].pino) == LOW);
        estado[nReles] = { raw, raw, millis(), millis(), 0 };
        Serial.printf("[PIN] GPIO%d → %s (%s)\n",
                      RELES[nReles].pino, RELES[nReles].nome,
                      raw ? "ATIVO" : "normal");
    }
    Serial.printf("[OK] %d relé(s) monitorado(s)\n\n", nReles);

    conectarWiFi();
}

// ── Loop ──────────────────────────────────────────
void loop() {
    uint32_t agora = millis();

    // Reconexão WiFi automática
    if (WiFi.status() != WL_CONNECTED) {
        if (agora - tUltimaReconexao > 30000) conectarWiFi();
    }

    // Leitura e debounce de cada relé
    for (int i = 0; i < nReles; i++) {
        bool raw = (digitalRead(RELES[i].pino) == LOW);  // LOW = relé fechado = ativo

        if (raw != estado[i].leituraRaw) {
            // Houve mudança: reinicia temporizador de debounce
            estado[i].leituraRaw = raw;
            estado[i].tDebounce  = agora;
        }

        // Só confirma a mudança após DEBOUNCE_MS estável
        if (raw != estado[i].ativo &&
            (agora - estado[i].tDebounce) >= DEBOUNCE_MS) {

            estado[i].ativo = raw;
            Serial.printf("[RELE] %s → %s\n",
                          RELES[i].nome, raw ? "ATIVO" : "normal");

            bool deveEnviar = false;

            if (raw) {
                // Ativação
                estado[i].tAtivou = agora;
                // Alarmes sempre notificam; outros respeitam cooldown
                if (strcmp(RELES[i].tipo, "ALARME") == 0) {
                    deveEnviar = true;
                } else {
                    deveEnviar = (agora - estado[i].tUltimoEnvio) >= COOLDOWN_MS;
                }
            } else {
                // Normalização: respeita cooldown (exceto se acabou de ativar)
                deveEnviar = (agora - estado[i].tUltimoEnvio) >= COOLDOWN_MS
                          || (agora - estado[i].tAtivou) < COOLDOWN_MS;
            }

            if (deveEnviar) notificar(i, raw);
        }
    }

    delay(10);
}
