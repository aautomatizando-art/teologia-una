/*
 * ESP32-CAM (AI-Thinker) - Monitor do Alarme de Incendio
 * Funcao: le 2 entradas digitais do quadro/central de alarme de incendio,
 *         tira uma foto da central com a camera OV2640 onboard, envia
 *         alertas (texto + foto) para um GRUPO do WhatsApp atraves da
 *         Evolution API e publica os dados no Supabase para a dashboard
 *         web (Vercel)
 *
 * ENTRADA 1 (GPIO13): Alarme acionado (central de incendio disparou)
 * ENTRADA 2 (GPIO15): Central com avaria/falha
 *
 * Arquitetura:
 *   - WiFi direto (sem ESP-NOW): este e o unico ESP32 do nó, conecta
 *     direto na rede WiFi do condominio via WiFiManager.
 *   - Camera OV2640 onboard captura uma foto JPEG (VGA) da central
 *     sempre que o alarme dispara, a central entra em avaria, ou a cada
 *     1 hora enquanto o status nao estiver "normal".
 *   - A foto e enviada (upload com upsert/overwrite) para o Supabase
 *     Storage, bucket "fotos-incendio", sempre no mesmo caminho — a foto
 *     anterior e substituida automaticamente pela mais recente.
 *   - A URL publica da foto e gravada na tabela "alarme_incendio" (coluna
 *     foto_url / foto_atualizada_em) para a dashboard exibir.
 *   - Alertas de texto + a foto (sendMedia) sao enviados ao grupo do
 *     WhatsApp via Evolution API.
 *
 * WiFi: configuravel no local via WiFiManager. Se nao conectar na rede
 * salva, o ESP32-CAM cria o AP "AlarmeIncendio-Setup" (senha 12345678)
 * com portal para escolher a rede e digitar a senha do WiFi do condominio.
 *
 * Placa no Arduino IDE: "AI Thinker ESP32-CAM" (com PSRAM habilitada).
 * Gravacao: adaptador FTDI, GPIO0 ligado ao GND durante o upload.
 *
 * Bibliotecas necessarias:
 *   - ArduinoJson by Benoit Blanchon
 *   - WiFiManager by tzapu
 *   (esp_camera, HTTPClient, WiFiClientSecure, WiFi e time ja vem no
 *    ESP32 core)
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <esp_camera.h>
#include <time.h>
#include "config.h"

// ─── PINAGEM DA CAMERA (AI-Thinker ESP32-CAM, OV2640) ───────────────────
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ─── ESTADO DAS ENTRADAS ─────────────────────────────────────────────────
bool entrada1_alarmeAcionado = false;
bool entrada2_centralAvaria  = false;

bool alertaAlarmeEnviado = false;
bool alertaAvariaEnviado = false;

// ─── ESTADO DA ULTIMA FOTO ENVIADA ───────────────────────────────────────
String ultimaFotoUrl       = "";
String ultimaFotoTimestamp = "";
unsigned long ultimoEnvioFoto = 0;

unsigned long ultimoEnvio        = 0;
unsigned long ultimaTentativaWiFi = 0;

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
}

// Inicializa a camera OV2640 (resolucao VGA, JPEG)
bool iniciarCamera() {
    camera_config_t config;
    config.ledc_channel = LEDC_CHANNEL_0;
    config.ledc_timer   = LEDC_TIMER_0;
    config.pin_d0       = Y2_GPIO_NUM;
    config.pin_d1       = Y3_GPIO_NUM;
    config.pin_d2       = Y4_GPIO_NUM;
    config.pin_d3       = Y5_GPIO_NUM;
    config.pin_d4       = Y6_GPIO_NUM;
    config.pin_d5       = Y7_GPIO_NUM;
    config.pin_d6       = Y8_GPIO_NUM;
    config.pin_d7       = Y9_GPIO_NUM;
    config.pin_xclk     = XCLK_GPIO_NUM;
    config.pin_pclk     = PCLK_GPIO_NUM;
    config.pin_vsync    = VSYNC_GPIO_NUM;
    config.pin_href     = HREF_GPIO_NUM;
    config.pin_sccb_sda = SIOD_GPIO_NUM;
    config.pin_sccb_scl = SIOC_GPIO_NUM;
    config.pin_pwdn     = PWDN_GPIO_NUM;
    config.pin_reset    = RESET_GPIO_NUM;
    config.xclk_freq_hz = 20000000;
    config.pixel_format = PIXFORMAT_JPEG;
    config.frame_size   = FRAMESIZE_VGA;
    config.jpeg_quality = 12;
    config.fb_count     = 1;
    config.fb_location  = CAMERA_FB_IN_DRAM;
    config.grab_mode    = CAMERA_GRAB_WHEN_EMPTY;

    if (psramFound()) {
        config.frame_size  = FRAMESIZE_VGA;
        config.jpeg_quality = 12;
        config.fb_count    = 2;
        config.fb_location = CAMERA_FB_IN_PSRAM;
    }

    esp_err_t err = esp_camera_init(&config);
    if (err != ESP_OK) {
        Serial.printf("[CAMERA] ERRO na inicializacao: 0x%x\n", err);
        return false;
    }

    Serial.println("[CAMERA] OK (PSRAM: " + String(psramFound() ? "sim" : "nao") + ")");
    return true;
}

// Retorna o timestamp atual em UTC no formato ISO-8601 (para foto_atualizada_em).
// Se o NTP ainda nao sincronizou, retorna string vazia (campo fica null no JSON).
String timestampISO() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) {
        return "";
    }
    char buf[25];
    strftime(buf, sizeof(buf), "%Y-%m-%dT%H:%M:%SZ", &timeinfo);
    return String(buf);
}

// Gera um identificador "slug" do nome do condominio: minusculas, apenas
// letras/numeros, demais caracteres viram "_". Usado no caminho do arquivo
// no Supabase Storage (um arquivo fixo por condominio, sempre sobrescrito).
String condominioSlug() {
    String nome = String(CONDOMINIO_NOME);
    nome.toLowerCase();
    String slug = "";
    for (size_t i = 0; i < nome.length(); i++) {
        char c = nome[i];
        if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9')) {
            slug += c;
        } else {
            slug += '_';
        }
    }
    return slug;
}

// Captura uma foto da central com a camera e envia (upload com upsert) para
// o Supabase Storage, bucket "fotos-incendio". A foto anterior no mesmo
// caminho e sobrescrita automaticamente.
bool capturarEEnviarFoto() {
    if (WiFi.status() != WL_CONNECTED) return false;

    camera_fb_t *fb = esp_camera_fb_get();
    if (!fb) {
        Serial.println("[CAMERA] ERRO ao capturar frame!");
        return false;
    }

    String path = "incendio/" + condominioSlug() + ".jpg";

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(SUPABASE_URL) + "/storage/v1/object/fotos-incendio/" + path;
    http.begin(client, url);
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Content-Type", "image/jpeg");
    http.addHeader("x-upsert", "true");
    http.setTimeout(15000);

    int code = http.POST(fb->buf, fb->len);
    bool ok = (code >= 200 && code < 300);
    Serial.printf("[FOTO] HTTP %d %s (%u bytes)\n", code, ok ? "OK" : http.getString().c_str(), fb->len);
    http.end();

    esp_camera_fb_return(fb);

    if (ok) {
        ultimaFotoUrl       = String(SUPABASE_URL) + "/storage/v1/object/public/fotos-incendio/" + path;
        ultimaFotoTimestamp = timestampISO();
        ultimoEnvioFoto     = millis();
    }

    return ok;
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
    doc["text"]   = "🏢 *" + String(CONDOMINIO_NOME) + "*\n" + texto;
    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    bool ok = (code >= 200 && code < 300);
    Serial.printf("[WHATSAPP] HTTP %d %s\n", code, ok ? "OK" : resp.c_str());
    return ok;
}

// Envia a ultima foto capturada (ultimaFotoUrl) ao grupo do WhatsApp via
// Evolution API (sendMedia). So deve ser chamada se ultimaFotoUrl != "".
bool enviarWhatsAppFoto(const String &legenda) {
    if (WiFi.status() != WL_CONNECTED) return false;
    if (ultimaFotoUrl == "") return false;

    HTTPClient http;
    String url = String(EVO_BASE_URL) + "/message/sendMedia/" + EVO_INSTANCE;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", EVO_APIKEY);
    http.setTimeout(15000);

    JsonDocument doc;
    doc["number"]    = WHATS_GROUP_ID;
    doc["mediatype"] = "image";
    doc["media"]     = ultimaFotoUrl;
    doc["caption"]   = "🏢 *" + String(CONDOMINIO_NOME) + "*\n" + legenda;
    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    String resp = http.getString();
    http.end();

    bool ok = (code >= 200 && code < 300);
    Serial.printf("[WHATSAPP-FOTO] HTTP %d %s\n", code, ok ? "OK" : resp.c_str());
    return ok;
}

// Le as 2 entradas digitais (contato seco para GND = ativo, INPUT_PULLUP)
void lerEntradas() {
    entrada1_alarmeAcionado = (digitalRead(ENTRADA1_PIN) == LOW);
    entrada2_centralAvaria  = (digitalRead(ENTRADA2_PIN) == LOW);
}

// Envia o status atual para o Supabase (dashboard web)
bool enviarSupabase(const String &status) {
    if (WiFi.status() != WL_CONNECTED) return false;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(SUPABASE_URL) + "/rest/v1/alarme_incendio";
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.addHeader("Prefer", "return=minimal");
    http.setTimeout(10000);

    JsonDocument doc;
    doc["condominio"]               = CONDOMINIO_NOME;
    doc["entrada1_alarme_acionado"] = entrada1_alarmeAcionado;
    doc["entrada2_central_avaria"]  = entrada2_centralAvaria;
    doc["status"]                   = status;
    if (ultimaFotoUrl != "") {
        doc["foto_url"] = ultimaFotoUrl;
    }
    if (ultimaFotoTimestamp != "") {
        doc["foto_atualizada_em"] = ultimaFotoTimestamp;
    }
    doc["sensor_uptime_s"] = millis() / 1000;
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

    pinMode(ENTRADA1_PIN, INPUT_PULLUP);
    pinMode(ENTRADA2_PIN, INPUT_PULLUP);

    iniciarCamera();

    conectarWiFi();

    // NTP em UTC (sem offset) — Postgres TIMESTAMPTZ trata UTC corretamente
    configTime(0, 0, "pool.ntp.org");

    // Aguarda ate 5s pela sincronizacao do NTP, para a 1a foto ja sair com
    // foto_atualizada_em preenchido
    struct tm timeinfo;
    unsigned long ntpInicio = millis();
    while (!getLocalTime(&timeinfo) && millis() - ntpInicio < 5000UL) {
        delay(100);
    }

    lerEntradas();
    alertaAlarmeEnviado = entrada1_alarmeAcionado;
    alertaAvariaEnviado = entrada2_centralAvaria;

    String statusInicial = entrada1_alarmeAcionado ? "alarme"
                          : entrada2_centralAvaria  ? "avaria"
                          : "normal";

    // Foto inicial da central (mesmo com status normal), para a dashboard
    // ja mostrar a imagem em vez de "Aguardando foto da central..."
    capturarEEnviarFoto();
    enviarSupabase(statusInicial);

    enviarWhatsApp(
        "🔥 *Monitor de Alarme de Incendio iniciado!*\n"
        "Aguardando leituras das entradas...");

    ultimoEnvio        = millis();
    ultimaTentativaWiFi = millis();

    Serial.println("[SETUP] Monitor de alarme de incendio pronto!");
}

void loop() {
    unsigned long agora = millis();

    // Queda temporaria do WiFi: reconecta na rede salva (sem abrir o portal)
    if (WiFi.status() != WL_CONNECTED && agora - ultimaTentativaWiFi >= 15000UL) {
        ultimaTentativaWiFi = agora;
        Serial.println("[WiFi] Reconectando...");
        WiFi.reconnect();
    }

    // Le entradas com debounce simples
    lerEntradas();
    delay(50);
    lerEntradas();

    // Determina o status atual (alarme tem prioridade sobre avaria)
    String status;
    if (entrada1_alarmeAcionado) {
        status = "alarme";
    } else if (entrada2_centralAvaria) {
        status = "avaria";
    } else {
        status = "normal";
    }

    // ── ENTRADA 1: Alarme acionado (RISING) ──
    if (entrada1_alarmeAcionado && !alertaAlarmeEnviado) {
        capturarEEnviarFoto();
        enviarWhatsApp(
            "🚨 *ALARME DE INCENDIO ACIONADO!*\n"
            "Verifique a central de alarme imediatamente.");
        enviarWhatsAppFoto("Foto da central no momento do alarme.");
        enviarSupabase("alarme");
        alertaAlarmeEnviado = true;
        ultimoEnvio = agora;
    }

    // ── ENTRADA 1: Alarme normalizado (FALLING, e ENTRADA2 tambem inativa) ──
    if (!entrada1_alarmeAcionado && alertaAlarmeEnviado && !entrada2_centralAvaria) {
        enviarWhatsApp("✅ *Alarme de incendio normalizado.*");
        enviarSupabase("normal");
        alertaAlarmeEnviado = false;
        ultimoEnvio = agora;
    } else if (!entrada1_alarmeAcionado) {
        alertaAlarmeEnviado = false;
    }

    // ── ENTRADA 2: Central com avaria (RISING, e ENTRADA1 inativa) ──
    if (entrada2_centralAvaria && !alertaAvariaEnviado && !entrada1_alarmeAcionado) {
        capturarEEnviarFoto();
        enviarWhatsApp(
            "⚠️ *Central de alarme de incendio com AVARIA/FALHA!*\n"
            "Verifique o painel da central.");
        enviarWhatsAppFoto("Foto da central — avaria detectada.");
        enviarSupabase("avaria");
        alertaAvariaEnviado = true;
        ultimoEnvio = agora;
    }

    // ── ENTRADA 2: Avaria normalizada (FALLING, e ENTRADA1 tambem inativa) ──
    if (!entrada2_centralAvaria && alertaAvariaEnviado && !entrada1_alarmeAcionado) {
        enviarWhatsApp("✅ *Avaria da central de incendio normalizada.*");
        enviarSupabase("normal");
        alertaAvariaEnviado = false;
        ultimoEnvio = agora;
    } else if (!entrada2_centralAvaria) {
        alertaAvariaEnviado = false;
    }

    // Atualiza flags de alerta caso o status tenha sido "absorvido" pela
    // prioridade do alarme (ex: avaria some enquanto alarme ainda ativo)
    if (entrada1_alarmeAcionado) alertaAvariaEnviado = entrada2_centralAvaria;

    // Atualizacao periodica da foto: a cada FOTO_INTERVALO_MS (1h) se
    // alarme/avaria, a cada FOTO_INTERVALO_NORMAL_MS (6h) se normal — assim
    // a foto da dashboard nunca fica muito desatualizada
    unsigned long fotoIntervalo = (status != "normal") ? FOTO_INTERVALO_MS : FOTO_INTERVALO_NORMAL_MS;
    if (agora - ultimoEnvioFoto >= fotoIntervalo) {
        if (capturarEEnviarFoto()) {
            enviarSupabase(status);
        }
    }

    // Heartbeat: envia status periodicamente, mesmo sem mudanca, para a
    // dashboard nao mostrar "sem comunicacao"
    if (agora - ultimoEnvio >= INTERVALO_MEDICAO_MS) {
        ultimoEnvio = agora;
        enviarSupabase(status);
    }
}
