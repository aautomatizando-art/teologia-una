/*
 * ESP32 #2 - Gateway (proximo ao WiFi)
 * Funcao: recebe dados via ESP-NOW, envia alertas para um GRUPO do WhatsApp
 *         atraves da Evolution API e publica os dados no Supabase para a
 *         dashboard web (Vercel)
 *
 * Alertas enviados ao WhatsApp:
 *   - Nivel baixo + Entrada 1 (Bomba ligou)
 *   - Entrada 2 acionada (Bomba falhou)
 *   - Entrada 3 acionada (Falha no inversor)
 *   - Entrada 4 acionada (Painel sem energia)
 *   - Caixa abastecida (nivel OK)
 *   - Sensor sem comunicacao
 *
 * Bibliotecas necessarias:
 *   - ArduinoJson by Benoit Blanchon
 *   (HTTPClient, WiFiClientSecure e WiFi ja vem no ESP32 core)
 */

#include <esp_now.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include "config.h"

// Estrutura identica a do esp32_sensor!
typedef struct {
    float    distancia;
    int      nivel;
    bool     entrada1_bombaLigada;
    bool     entrada2_bombaFalhou;
    bool     entrada3_falhaInversor;
    bool     entrada4_painelSemEnergia;
    unsigned long uptime;
    bool     leituraValida;
} DadosSensor;

DadosSensor dados;
volatile bool dadosNovos = false;

bool alertaNivelEnviado    = false;
bool alertaBombaFalhouEnviado = false;
bool alertaInversorEnviado    = false;
bool alertaPainelEnviado      = false;
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

void conectarWiFi() {
    Serial.print("[WiFi] Conectando");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 20) {
        delay(500); Serial.print("."); t++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Conectado! IP: " + WiFi.localIP().toString());
        Serial.println("[WiFi] Canal: " + String(WiFi.channel()));
    } else {
        Serial.println("\n[WiFi] Falha na conexao!");
    }
}

// Envia mensagem de texto ao grupo do WhatsApp via Evolution API
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
    doc["text"]   = texto;
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
    doc["distancia_cm"]                = dados.distancia;
    doc["nivel_pct"]                   = dados.nivel;
    doc["entrada1_bomba_ligada"]       = dados.entrada1_bombaLigada;
    doc["entrada2_bomba_falhou"]       = dados.entrada2_bombaFalhou;
    doc["entrada3_falha_inversor"]     = dados.entrada3_falhaInversor;
    doc["entrada4_painel_sem_energia"] = dados.entrada4_painelSemEnergia;
    doc["sensor_uptime_s"]             = dados.uptime;
    doc["leitura_valida"]              = dados.leituraValida;
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

    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Reconectando...");
        conectarWiFi();
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
        dadosNovos     = false;
        alertaSemSinal = false;

        Serial.printf("[DADOS] %.1fcm | %d%% | E1(Bomba ligou): %s | E2(Bomba falhou): %s | E3(Falha inversor): %s | E4(Painel sem energia): %s | up %lus\n",
            dados.distancia, dados.nivel,
            dados.entrada1_bombaLigada      ? "ON" : "OFF",
            dados.entrada2_bombaFalhou      ? "ON" : "OFF",
            dados.entrada3_falhaInversor    ? "ON" : "OFF",
            dados.entrada4_painelSemEnergia ? "ON" : "OFF",
            dados.uptime);

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

        // ── ENTRADA 2: Bomba falhou (prioridade maxima) ──
        if (dados.entrada2_bombaFalhou && !alertaBombaFalhouEnviado) {
            String msg = "\xF0\x9F\x9A\xA8 *FALHA NA BOMBA!* (Entrada 2 acionada)\n\n";
            msg += "O quadro de comando sinalizou falha na bomba.\n";
            msg += "Possiveis causas: bomba queimada, falta d'agua no poco, registro fechado, protecao do motor.\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel atual: *" + String(dados.nivel) + "%*";
            enviarWhatsApp(msg);
            alertaBombaFalhouEnviado = true;
        }
        if (!dados.entrada2_bombaFalhou && alertaBombaFalhouEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Falha na bomba normalizada* (Entrada 2 desativada).");
            alertaBombaFalhouEnviado = false;
        }

        // ── ENTRADA 3: Falha no inversor ──
        if (dados.entrada3_falhaInversor && !alertaInversorEnviado) {
            String msg = "\xF0\x9F\x9A\xA8 *FALHA NO INVERSOR!* (Entrada 3 acionada)\n\n";
            msg += "Verifique o inversor/quadro de energia do sistema.";
            enviarWhatsApp(msg);
            alertaInversorEnviado = true;
        }
        if (!dados.entrada3_falhaInversor && alertaInversorEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Falha no inversor normalizada* (Entrada 3 desativada).");
            alertaInversorEnviado = false;
        }

        // ── ENTRADA 4: Painel sem energia ──
        if (dados.entrada4_painelSemEnergia && !alertaPainelEnviado) {
            String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *PAINEL SEM ENERGIA!* (Entrada 4 acionada)\n\n";
            msg += "O quadro esta sem alimentacao da rede eletrica.";
            enviarWhatsApp(msg);
            alertaPainelEnviado = true;
        }
        if (!dados.entrada4_painelSemEnergia && alertaPainelEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Energia do painel restabelecida!* (Entrada 4 desativada).");
            alertaPainelEnviado = false;
        }

        // ── Nivel baixo + ENTRADA 1 acionada (Bomba Ligada) ──
        if (dados.nivel <= NIVEL_ALERTA && dados.entrada1_bombaLigada && !alertaNivelEnviado) {
            String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *Nivel baixo na caixa d'agua!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(dados.nivel) + "%*\n";
            msg += "\xF0\x9F\x93\x8F Distancia: " + String(dados.distancia, 1) + "cm\n";
            msg += "\xF0\x9F\x9F\xA2 Entrada 1 acionada: *Bomba LIGADA*";
            enviarWhatsApp(msg);
            alertaNivelEnviado = true;
        }

        // ── Caixa abastecida ──
        if (dados.nivel >= NIVEL_OK && alertaNivelEnviado) {
            String msg = "\xE2\x9C\x85 *Caixa d'agua abastecida!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(dados.nivel) + "%*\n";
            msg += "\xF0\x9F\x94\xB4 Bomba DESLIGADA.";
            enviarWhatsApp(msg);
            alertaNivelEnviado = false;
        }
    }

    // Status periodico (opcional, desativado por padrao)
    if (INTERVALO_STATUS_MS > 0 && agora - ultimoStatus >= INTERVALO_STATUS_MS) {
        ultimoStatus = agora;
        String msg = "\xF0\x9F\x93\x8B *Status do sistema*\n";
        msg += "Nivel: " + String(dados.nivel) + "%\n";
        msg += "Entrada 1 (Bomba ligou): " + String(dados.entrada1_bombaLigada ? "ON" : "OFF") + "\n";
        msg += "Entrada 2 (Bomba falhou): " + String(dados.entrada2_bombaFalhou ? "ON" : "OFF") + "\n";
        msg += "Entrada 3 (Falha inversor): " + String(dados.entrada3_falhaInversor ? "ON" : "OFF") + "\n";
        msg += "Entrada 4 (Painel sem energia): " + String(dados.entrada4_painelSemEnergia ? "ON" : "OFF") + "\n";
        msg += "Sensor online: " + String(dados.uptime / 60) + " min";
        enviarWhatsApp(msg);
    }
}
