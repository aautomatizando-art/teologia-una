/*
 * ESP32 #2 - Gateway (modulo WT32-ETH01, conexao via cabo de rede)
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
 * Rede: WT32-ETH01 com PHY LAN8720 (RJ45), conexao via DHCP — sem WiFi/portal.
 * O radio WiFi continua ativo (modo STA, sem se conectar a nenhuma rede)
 * apenas para o ESP-NOW falar com o ESP32 #1 (sensor).
 *
 * Placa no Arduino IDE: "WT32-ETH01"
 *
 * Bibliotecas necessarias:
 *   - ArduinoJson by Benoit Blanchon
 *   (ETH, HTTPClient, WiFiClientSecure e WiFi ja vem no ESP32 core)
 */

#include <esp_now.h>
#include <WiFi.h>
#include <ETH.h>
#include "esp_wifi.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// ─── PINOS ETHERNET (WT32-ETH01 + LAN8720) ──────────────────────────────────
#define ETH_PHY_TYPE   ETH_PHY_LAN8720
#define ETH_PHY_ADDR   1
#define ETH_PHY_MDC    23
#define ETH_PHY_MDIO   18
#define ETH_PHY_POWER  16
#define ETH_CLK_MODE   ETH_CLOCK_GPIO0_IN

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

volatile bool ethConectado = false;

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(DadosSensor)) {
        memcpy((void *)&dados, data, sizeof(DadosSensor));
        dadosNovos     = true;
        ultimoRecebido = millis();
    }
}

void onEthEvent(WiFiEvent_t event) {
    switch (event) {
        case ARDUINO_EVENT_ETH_START:
            ETH.setHostname("gateway-caixa-dagua");
            break;
        case ARDUINO_EVENT_ETH_GOT_IP:
            Serial.print("[ETH] Conectado! IP: ");
            Serial.println(ETH.localIP());
            ethConectado = true;
            break;
        case ARDUINO_EVENT_ETH_DISCONNECTED:
        case ARDUINO_EVENT_ETH_STOP:
            ethConectado = false;
            break;
        default:
            break;
    }
}

// Liga o cabo de rede (Ethernet); mantem o radio WiFi em modo STA sem se
// conectar a nenhuma rede, apenas para o ESP-NOW falar com o ESP32 #1
void conectarRede() {
    WiFi.onEvent(onEthEvent);

    WiFi.mode(WIFI_STA);
    esp_wifi_set_channel(1, WIFI_SECOND_CHAN_NONE);

    Serial.println("[ETH] Conectando via cabo de rede...");
    ETH.begin(ETH_PHY_TYPE, ETH_PHY_ADDR, ETH_PHY_MDC, ETH_PHY_MDIO, ETH_PHY_POWER, ETH_CLK_MODE);

    unsigned long inicio = millis();
    while (!ethConectado && millis() - inicio < 20000UL) {
        delay(200);
    }
    if (!ethConectado) {
        Serial.println("[ETH] Sem conexao apos 20s. Reiniciando...");
        delay(2000);
        ESP.restart();
    }
}

// Envia mensagem de texto ao grupo do WhatsApp via Evolution API
// (prefixa com o nome do condominio para identificar a origem no grupo)
bool enviarWhatsApp(const String &texto) {
    if (!ethConectado) return false;

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
    if (!ethConectado) return false;

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

    // Rede (Ethernet) DEVE iniciar antes do ESP-NOW para fixar o canal WiFi
    conectarRede();

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
