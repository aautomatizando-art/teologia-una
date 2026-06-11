/*
 * ESP32 #2 - Gateway (proximo ao WiFi)
 * Funcao: recebe o nivel da caixa via ESP-NOW, envia alertas para um GRUPO
 *         do WhatsApp atraves da Evolution API e publica os dados no
 *         Supabase para a dashboard web (Vercel)
 *
 * Alertas enviados ao WhatsApp:
 *   - Nivel baixo
 *   - Caixa abastecida (nivel OK)
 *   - Sensor sem comunicacao
 *
 * As 4 entradas do quadro/inversor da bomba (Bomba ligou / Bomba falhou /
 * Falha no inversor / Painel sem energia) NAO sao tratadas neste gateway —
 * sao lidas e alertadas pelo ESP32 do Tanque Inferior.
 *
 * WiFi: configuravel no local via WiFiManager. Se nao conectar na rede salva,
 * o ESP32 cria o AP "CaixaDagua-Setup" (senha 12345678) com portal para
 * escolher a rede e digitar a senha do WiFi do condominio.
 *
 * Bibliotecas necessarias:
 *   - ArduinoJson by Benoit Blanchon
 *   - WiFiManager by tzapu
 *   (HTTPClient, WiFiClientSecure e WiFi ja vem no ESP32 core)
 */

#include <esp_now.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Estrutura identica a do esp32_sensor!
typedef struct {
    float    distancia;
    int      nivel;
    unsigned long uptime;
    bool     leituraValida;
} DadosSensor;

DadosSensor dados;
volatile bool dadosNovos = false;

bool alertaNivelEnviado    = false;
bool alertaSemSinal      = false;
bool leituraInvalidaAvisada = false;
unsigned long ultimoStatus   = 0;
unsigned long ultimoRecebido = 0;

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(DadosSensor)) {
        memcpy((void *)&dados, data, sizeof(DadosSensor));
        dadosNovos     = true;
        ultimoRecebido = millis();
    }
}

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
    Serial.println("[WiFi] Canal: " + String(WiFi.channel()));
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
    doc["text"]   = "\xF0\x9F\x8F\xA2 *" + String(CONDOMINIO_NOME) + "*\n" + texto;
    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    bool ok = (code >= 200 && code < 300);
    Serial.printf("[WHATSAPP] HTTP %d %s\n", code, ok ? "OK" : resp.c_str());
    return ok;
}

// Envia a leitura atual para o Supabase (dashboard web)
bool enviarSupabase() {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(SUPABASE_URL) + "/rest/v1/leituras";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Prefer", "return=minimal");
    http.setTimeout(10000);

    JsonDocument doc;
    doc["condominio"]      = CONDOMINIO_NOME;
    doc["distancia_cm"]    = dados.distancia;
    doc["nivel_pct"]       = dados.nivel;
    doc["sensor_uptime_s"] = dados.uptime;
    doc["leitura_valida"]  = dados.leituraValida;
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

    // WiFi DEVE iniciar antes do ESP-NOW para fixar o canal
    conectarWiFi();

    Serial.print("[GATEWAY] MAC: ");
    Serial.println(WiFi.macAddress());  // <- informe este MAC no config.h do sensor!

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ERRO na inicializacao!");
        return;
    }
    esp_now_register_recv_cb(onDataReceived);

    ultimoRecebido = millis();

    enviarWhatsApp(
        "\xF0\x9F\x92\xA7 *Monitor Caixa d'Agua iniciado!*\n"
        "Gateway online, aguardando dados do sensor...");

    Serial.println("[SETUP] Gateway pronto!");
}

void loop() {
    unsigned long agora = millis();

    // Queda temporaria do WiFi: reconecta na rede salva (sem abrir o portal)
    static unsigned long ultimaTentativaWiFi = 0;
    if (WiFi.status() != WL_CONNECTED && agora - ultimaTentativaWiFi >= 15000UL) {
        ultimaTentativaWiFi = agora;
        Serial.println("[WiFi] Reconectando...");
        WiFi.reconnect();
    }

    // Sensor sem comunicacao
    if (agora - ultimoRecebido >= TIMEOUT_SENSOR_MS && ultimoRecebido > 0) {
        if (!alertaSemSinal) {
            enviarWhatsApp(
                "\xE2\x9A\xA0\xEF\xB8\x8F *ATENCAO: Sensor sem comunicacao!*\n"
                "O ESP32 da caixa d'agua nao envia dados ha mais de 2 minutos.\n"
                "Verifique alimentacao e alcance do ESP-NOW.");
            alertaSemSinal = true;
        }
    }

    if (dadosNovos) {
        dadosNovos = false;

        // Sensor voltou a comunicar depois de uma queda
        if (alertaSemSinal) {
            enviarWhatsApp(
                "\xE2\x9C\x85 *Sensor online novamente!*\n"
                "O ESP32 da caixa d'agua voltou a enviar dados.");
            alertaSemSinal = false;
        }

        Serial.printf("[DADOS] %.1fcm | %d%% | up %lus\n",
            dados.distancia, dados.nivel, dados.uptime);

        enviarSupabase();

        // Leitura invalida do sensor
        if (!dados.leituraValida) {
            if (!leituraInvalidaAvisada) {
                enviarWhatsApp(
                    "\xE2\x9A\xA0\xEF\xB8\x8F *Sensor com leitura invalida!*\n"
                    "Verifique o JSN-SR04T na caixa d'agua.");
                leituraInvalidaAvisada = true;
            }
            return;
        }
        leituraInvalidaAvisada = false;

        // ── Nivel baixo ──
        if (dados.nivel <= NIVEL_ALERTA && !alertaNivelEnviado) {
            String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *Nivel baixo na caixa d'agua!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(dados.nivel) + "%*\n";
            msg += "\xF0\x9F\x93\x8F Distancia: " + String(dados.distancia, 1) + "cm";
            enviarWhatsApp(msg);
            alertaNivelEnviado = true;
        }

        // ── Caixa abastecida ──
        if (dados.nivel >= NIVEL_OK && alertaNivelEnviado) {
            String msg = "\xE2\x9C\x85 *Caixa d'agua abastecida!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(dados.nivel) + "%*";
            enviarWhatsApp(msg);
            alertaNivelEnviado = false;
        }
    }

    // Status periodico (opcional, desativado por padrao)
    if (INTERVALO_STATUS_MS > 0 && agora - ultimoStatus >= INTERVALO_STATUS_MS) {
        ultimoStatus = agora;
        String msg = "\xF0\x9F\x93\x8B *Status do sistema*\n";
        msg += "Nivel: " + String(dados.nivel) + "%\n";
        msg += "Sensor online: " + String(dados.uptime / 60) + " min";
        enviarWhatsApp(msg);
    }
}
