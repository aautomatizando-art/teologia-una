/*
 * ESP32 - Tanque Inferior (no standalone, WiFi direto, sem ESP-NOW)
 * Funcao: monitora o tanque inferior (reservatorio/cisterna proximo a bomba)
 *         - Nivel de agua via sensor ultrassonico JSN-SR04T
 *         - 3 entradas digitais do quadro/inversor da bomba
 *         - Temperatura da bomba via sensor DS18B20 (OneWire)
 *         - Vibracao da bomba via sensor analogico
 *
 * Envia os dados para o Supabase (tabela "tanque_inferior") e dispara
 * alertas para o grupo do WhatsApp via Evolution API.
 *
 * ENTRADA 2 (GPIO14): Bomba falhou
 * ENTRADA 3 (GPIO13): Falha no inversor
 * ENTRADA 4 (GPIO4):  Painel sem energia (sem rede CA)
 * (Nao existe "Entrada 1" neste no — esse sinal so existe no sensor do
 *  tanque superior)
 *
 * WiFi: configuravel no local via WiFiManager. Se nao conectar na rede salva,
 * o ESP32 cria o AP "TanqueInferior-Setup" (senha 12345678) com portal para
 * escolher a rede e digitar a senha do WiFi do condominio.
 *
 * Bibliotecas necessarias:
 *   - ArduinoJson      by Benoit Blanchon
 *   - WiFiManager      by tzapu
 *   - OneWire          by Jim Studt / Paul Stoffregen
 *   - DallasTemperature by Miles Burton
 *   (HTTPClient, WiFiClientSecure e WiFi ja vem no ESP32 core)
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

// ─── SENSOR DE TEMPERATURA (DS18B20 via OneWire) ───────────────────────
OneWire oneWire(TEMP_PIN);
DallasTemperature sensoresTemp(&oneWire);

// ─── DADOS DA LEITURA ATUAL ─────────────────────────────────────────────
float distanciaCm   = -1;
int   nivelPct      = 0;
bool  entrada2_bombaFalhou      = false;
bool  entrada3_falhaInversor    = false;
bool  entrada4_painelSemEnergia = false;
float temperaturaC  = NAN;
float vibracaoValor = 0;
bool  vibracaoAlerta = false;
bool  leituraValida  = false;

unsigned long ultimaMedicao = 0;

// ─── ESTADOS DE ALERTA (debounce: avisa so na mudanca de estado) ────────
bool alertaBombaFalhouEnviado = false;
bool alertaInversorEnviado    = false;
bool alertaPainelEnviado      = false;
bool alertaNivelBaixoEnviado  = false;
bool alertaTemperaturaEnviado = false;
bool alertaVibracaoEnviado    = false;
bool leituraInvalidaAvisada   = false;

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

// Mede a distancia (cm) ate a superficie da agua, com varias amostras
float medirMediaCm(int amostras = 5) {
    float soma  = 0;
    int validas = 0;
    for (int i = 0; i < amostras; i++) {
        digitalWrite(TRIG_PIN, LOW);
        delayMicroseconds(2);
        digitalWrite(TRIG_PIN, HIGH);
        delayMicroseconds(10);
        digitalWrite(TRIG_PIN, LOW);
        long dur = pulseIn(ECHO_PIN, HIGH, 30000);
        if (dur > 0) {
            float d = dur * 0.034f / 2.0f;
            if (d > 0 && d < 400) { soma += d; validas++; }
        }
        delay(100);
    }
    return validas > 0 ? soma / validas : -1;
}

// Converte distancia (cm) em porcentagem de nivel (0-100%)
int distParaPorcento(float dist) {
    long p = map((long)dist, DIST_CAIXA_VAZIA, DIST_CAIXA_CHEIA, 0, 100);
    return (int)constrain(p, 0, 100);
}

// Le as 3 entradas digitais (contato seco para GND = ativo, INPUT_PULLUP)
void lerEntradas() {
    entrada2_bombaFalhou      = (digitalRead(ENTRADA2_PIN) == LOW);
    entrada3_falhaInversor    = (digitalRead(ENTRADA3_PIN) == LOW);
    entrada4_painelSemEnergia = (digitalRead(ENTRADA4_PIN) == LOW);
}

// Le a temperatura da bomba via DS18B20. Retorna NAN se o sensor
// estiver desconectado ou com erro de leitura.
float lerTemperatura() {
    sensoresTemp.requestTemperatures();
    float t = sensoresTemp.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C) return NAN;
    return t;
}

// Le o sensor de vibracao (saida analogica), com varias amostras
float lerVibracao(int amostras = 5) {
    long soma = 0;
    for (int i = 0; i < amostras; i++) {
        soma += analogRead(VIBRACAO_PIN);
        delay(20);
    }
    return (float)soma / amostras;
}

// Pisca o LED onboard rapidamente (indica envio realizado)
void piscarLed() {
    digitalWrite(LED_PIN, HIGH); delay(80);
    digitalWrite(LED_PIN, LOW);
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
    String url = String(SUPABASE_URL) + "/rest/v1/tanque_inferior";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Prefer", "return=minimal");
    http.setTimeout(10000);

    JsonDocument doc;
    doc["condominio"]                  = CONDOMINIO_NOME;
    doc["distancia_cm"]                = distanciaCm;
    doc["nivel_pct"]                   = nivelPct;
    doc["entrada2_bomba_falhou"]       = entrada2_bombaFalhou;
    doc["entrada3_falha_inversor"]     = entrada3_falhaInversor;
    doc["entrada4_painel_sem_energia"] = entrada4_painelSemEnergia;
    if (isnan(temperaturaC)) {
        doc["temperatura_c"] = nullptr;
    } else {
        doc["temperatura_c"] = temperaturaC;
    }
    doc["vibracao_valor"]   = vibracaoValor;
    doc["vibracao_alerta"]  = vibracaoAlerta;
    doc["sensor_uptime_s"]  = millis() / 1000;
    doc["leitura_valida"]   = leituraValida;
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

    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_PIN,  OUTPUT);

    pinMode(ENTRADA2_PIN, INPUT_PULLUP);
    pinMode(ENTRADA3_PIN, INPUT_PULLUP);
    pinMode(ENTRADA4_PIN, INPUT_PULLUP);

    sensoresTemp.begin();

    conectarWiFi();

    Serial.print("[TANQUE INFERIOR] MAC: ");
    Serial.println(WiFi.macAddress());

    enviarWhatsApp(
        "\xF0\x9F\x94\xBD *Monitor Tanque Inferior iniciado!*\n"
        "Gateway online, aguardando primeira leitura...");

    Serial.println("[SETUP] Tanque Inferior pronto!");
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

    if (agora - ultimaMedicao >= INTERVALO_MEDICAO_MS) {
        ultimaMedicao = agora;

        float dist   = medirMediaCm();
        leituraValida = (dist > 0 && dist < 400);
        distanciaCm   = dist;
        nivelPct      = leituraValida ? distParaPorcento(dist) : 0;

        lerEntradas();
        temperaturaC  = lerTemperatura();
        vibracaoValor = lerVibracao();
        vibracaoAlerta = (vibracaoValor > VIBRACAO_LIMIAR);

        if (leituraValida) {
            Serial.printf("[DADOS] %.1fcm -> %d%% | E2(Bomba falhou): %d | E3(Falha inversor): %d | E4(Painel sem energia): %d | Temp: %.1fC | Vibracao: %.0f\n",
                dist, nivelPct,
                entrada2_bombaFalhou,
                entrada3_falhaInversor,
                entrada4_painelSemEnergia,
                temperaturaC,
                vibracaoValor);
        } else {
            Serial.println("[SENSOR] Leitura invalida!");
        }

        enviarSupabase();
        piscarLed();

        // Leitura invalida do sensor ultrassonico
        if (!leituraValida) {
            if (!leituraInvalidaAvisada) {
                enviarWhatsApp(
                    "\xE2\x9A\xA0\xEF\xB8\x8F *Sensor com leitura invalida!*\n"
                    "Verifique o JSN-SR04T no Tanque Inferior.");
                leituraInvalidaAvisada = true;
            }
            return;
        }
        leituraInvalidaAvisada = false;

        // ── ENTRADA 2: Bomba falhou (prioridade maxima) ──
        if (entrada2_bombaFalhou && !alertaBombaFalhouEnviado) {
            String msg = "\xF0\x9F\x9A\xA8 *FALHA NA BOMBA!* (Tanque Inferior - Entrada 2 acionada)\n\n";
            msg += "O quadro de comando sinalizou falha na bomba do Tanque Inferior.\n";
            msg += "Possiveis causas: bomba queimada, falta d'agua no poco, registro fechado, protecao do motor.\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel atual: *" + String(nivelPct) + "%*";
            enviarWhatsApp(msg);
            alertaBombaFalhouEnviado = true;
        }
        if (!entrada2_bombaFalhou && alertaBombaFalhouEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Falha na bomba normalizada* (Tanque Inferior).");
            alertaBombaFalhouEnviado = false;
        }

        // ── ENTRADA 3: Falha no inversor ──
        if (entrada3_falhaInversor && !alertaInversorEnviado) {
            String msg = "\xF0\x9F\x9A\xA8 *FALHA NO INVERSOR!* (Tanque Inferior - Entrada 3 acionada)\n\n";
            msg += "Verifique o inversor/quadro de energia da bomba do Tanque Inferior.";
            enviarWhatsApp(msg);
            alertaInversorEnviado = true;
        }
        if (!entrada3_falhaInversor && alertaInversorEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Falha no inversor normalizada* (Tanque Inferior).");
            alertaInversorEnviado = false;
        }

        // ── ENTRADA 4: Painel sem energia ──
        if (entrada4_painelSemEnergia && !alertaPainelEnviado) {
            String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *PAINEL SEM ENERGIA!* (Tanque Inferior - Entrada 4 acionada)\n\n";
            msg += "O quadro da bomba do Tanque Inferior esta sem alimentacao da rede eletrica.";
            enviarWhatsApp(msg);
            alertaPainelEnviado = true;
        }
        if (!entrada4_painelSemEnergia && alertaPainelEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Energia do painel restabelecida!* (Tanque Inferior).");
            alertaPainelEnviado = false;
        }

        // ── Nivel baixo ──
        if (nivelPct <= NIVEL_ALERTA && !alertaNivelBaixoEnviado) {
            String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *Nivel baixo no Tanque Inferior!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(nivelPct) + "%*\n";
            msg += "\xF0\x9F\x93\x8F Distancia: " + String(distanciaCm, 1) + "cm";
            enviarWhatsApp(msg);
            alertaNivelBaixoEnviado = true;
        }

        // ── Tanque abastecido ──
        if (nivelPct >= NIVEL_OK && alertaNivelBaixoEnviado) {
            String msg = "\xE2\x9C\x85 *Tanque Inferior abastecido!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(nivelPct) + "%*";
            enviarWhatsApp(msg);
            alertaNivelBaixoEnviado = false;
        }

        // ── Temperatura da bomba ──
        if (!isnan(temperaturaC)) {
            if (temperaturaC > TEMP_ALERTA_C && !alertaTemperaturaEnviado) {
                String msg = "\xF0\x9F\x8C\xA1\xEF\xB8\x8F *Temperatura alta na bomba (Tanque Inferior)!*\n\n";
                msg += "\xF0\x9F\x8C\xA1\xEF\xB8\x8F Temperatura: *" + String(temperaturaC, 1) + " C*\n";
                msg += "Verifique o funcionamento e a refrigeracao da bomba.";
                enviarWhatsApp(msg);
                alertaTemperaturaEnviado = true;
            }
            if (temperaturaC <= TEMP_ALERTA_C && alertaTemperaturaEnviado) {
                enviarWhatsApp("\xE2\x9C\x85 *Temperatura da bomba normalizada* (Tanque Inferior).");
                alertaTemperaturaEnviado = false;
            }
        }

        // ── Vibracao excessiva ──
        if (vibracaoAlerta && !alertaVibracaoEnviado) {
            String msg = "\xF0\x9F\x93\xB3 *Vibracao excessiva detectada na bomba (Tanque Inferior)!*\n\n";
            msg += "Leitura do sensor: *" + String(vibracaoValor, 0) + "*\n";
            msg += "Verifique fixacao, rolamentos e alinhamento da bomba.";
            enviarWhatsApp(msg);
            alertaVibracaoEnviado = true;
        }
        if (!vibracaoAlerta && alertaVibracaoEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Vibracao da bomba normalizada* (Tanque Inferior).");
            alertaVibracaoEnviado = false;
        }
    }
}
