/*
 * CIE 1125 (Intelbras) → ESP32-CAM → Telegram
 *
 * Relê de alarme da central aciona captura de foto do display → Telegram.
 *
 * Ligação:
 *   CIE relê NA  → GPIO13
 *   CIE relê COM → GND
 *
 * Módulo: AI-Thinker ESP32-CAM (OV2640)
 *
 * Biblioteca necessária: esp32-camera (inclusa no core ESP32)
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include "esp_camera.h"
#include "config.h"

// ── Pinagem AI-Thinker ESP32-CAM ──────────────────
#define PWDN_GPIO_NUM   32
#define RESET_GPIO_NUM  -1
#define XCLK_GPIO_NUM    0
#define SIOD_GPIO_NUM   26
#define SIOC_GPIO_NUM   27
#define Y9_GPIO_NUM     35
#define Y8_GPIO_NUM     34
#define Y7_GPIO_NUM     39
#define Y6_GPIO_NUM     36
#define Y5_GPIO_NUM     21
#define Y4_GPIO_NUM     19
#define Y3_GPIO_NUM     18
#define Y2_GPIO_NUM      5
#define VSYNC_GPIO_NUM  25
#define HREF_GPIO_NUM   23
#define PCLK_GPIO_NUM   22
#define PIN_FLASH        4
#define PIN_LED_RED     33   // LED vermelho onboard (LOW = aceso)

// ── Globals ───────────────────────────────────────
bool          alarmAtivo       = false;
unsigned long tUltimaFoto      = 0;
unsigned long tUltimaReconexao = 0;

// ── Camera ────────────────────────────────────────
bool iniciarCamera() {
    camera_config_t cfg = {};
    cfg.ledc_channel = LEDC_CHANNEL_0;
    cfg.ledc_timer   = LEDC_TIMER_0;
    cfg.pin_d0       = Y2_GPIO_NUM;
    cfg.pin_d1       = Y3_GPIO_NUM;
    cfg.pin_d2       = Y4_GPIO_NUM;
    cfg.pin_d3       = Y5_GPIO_NUM;
    cfg.pin_d4       = Y6_GPIO_NUM;
    cfg.pin_d5       = Y7_GPIO_NUM;
    cfg.pin_d6       = Y8_GPIO_NUM;
    cfg.pin_d7       = Y9_GPIO_NUM;
    cfg.pin_xclk     = XCLK_GPIO_NUM;
    cfg.pin_pclk     = PCLK_GPIO_NUM;
    cfg.pin_vsync    = VSYNC_GPIO_NUM;
    cfg.pin_href     = HREF_GPIO_NUM;
    cfg.pin_sscb_sda = SIOD_GPIO_NUM;
    cfg.pin_sscb_scl = SIOC_GPIO_NUM;
    cfg.pin_pwdn     = PWDN_GPIO_NUM;
    cfg.pin_reset    = RESET_GPIO_NUM;
    cfg.xclk_freq_hz = 20000000;
    cfg.pixel_format = PIXFORMAT_JPEG;
    cfg.frame_size   = CAM_FRAMESIZE;
    cfg.jpeg_quality = CAM_QUALIDADE;
    cfg.fb_count     = psramFound() ? 2 : 1;
    cfg.fb_location  = psramFound() ? CAMERA_FB_IN_PSRAM : CAMERA_FB_IN_DRAM;
    if (esp_camera_init(&cfg) != ESP_OK) return false;

    sensor_t* s = esp_camera_sensor_get();
    s->set_brightness(s,  1);   // -2 a 2
    s->set_contrast(s,    1);   // -2 a 2
    s->set_sharpness(s,   2);   // -2 a 2
    s->set_whitebal(s,    1);   // auto white balance
    s->set_awb_gain(s,    1);
    s->set_exposure_ctrl(s, 1); // auto exposure
    s->set_aec2(s,        1);
    return true;
}

// ── Telegram ──────────────────────────────────────
String montarLegenda(bool alarme) {
    String emoji  = alarme ? "\xF0\x9F\x94\xA5" : "\xE2\x9C\x85";
    String titulo = alarme ? "ALARME DE INC\xC3\x8ANDIO" : "NORMALIZADO";
    String msg    = emoji + " <b>" + titulo + "</b>\n";
    char up[10];
    unsigned long s = millis() / 1000;
    snprintf(up, sizeof(up), "%02lu:%02lu:%02lu", s / 3600, (s % 3600) / 60, s % 60);
    msg += "\xF0\x9F\x95\x90 Uptime: " + String(up) + "\n";
    msg += "<i>CIE 1125 | Intelbras</i>";
    return msg;
}

bool enviarTexto(const String& msg) {
    for (int t = 1; t <= 3; t++) {
        WiFiClientSecure c; c.setInsecure(); c.setTimeout(15);
        if (!c.connect("api.telegram.org", 443)) { delay(2000); continue; }
        String body = "{\"chat_id\":\"" + String(TELEGRAM_CHAT_ID) +
                      "\",\"text\":\"" + msg + "\",\"parse_mode\":\"HTML\"}";
        c.println("POST /bot" + String(TELEGRAM_TOKEN) + "/sendMessage HTTP/1.1");
        c.println("Host: api.telegram.org");
        c.println("Content-Type: application/json");
        c.println("Content-Length: " + String(body.length()));
        c.println("Connection: close");
        c.println();
        c.print(body);
        unsigned long ts = millis();
        while (!c.available() && millis() - ts < 8000) delay(10);
        String resp = ""; while (c.available()) resp += (char)c.read();
        if (resp.indexOf("\"ok\":true") >= 0) return true;
        delay(3000);
    }
    return false;
}

bool enviarFoto(camera_fb_t* fb, const String& caption) {
    const String boundary = "ESP32CAMBound";
    String head = "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"chat_id\"\r\n\r\n" +
        String(TELEGRAM_CHAT_ID) + "\r\n"
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"parse_mode\"\r\n\r\nHTML\r\n"
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"caption\"\r\n\r\n" +
        caption + "\r\n"
        "--" + boundary + "\r\n"
        "Content-Disposition: form-data; name=\"photo\"; filename=\"alarme.jpg\"\r\n"
        "Content-Type: image/jpeg\r\n\r\n";
    String tail = "\r\n--" + boundary + "--\r\n";
    int total = head.length() + fb->len + tail.length();

    for (int t = 1; t <= 3; t++) {
        Serial.printf("[CAM] Tentativa %d/3\n", t);
        WiFiClientSecure c; c.setInsecure(); c.setTimeout(30);
        if (!c.connect("api.telegram.org", 443)) { delay(3000); continue; }

        c.println("POST /bot" + String(TELEGRAM_TOKEN) + "/sendPhoto HTTP/1.1");
        c.println("Host: api.telegram.org");
        c.println("Content-Type: multipart/form-data; boundary=" + boundary);
        c.println("Content-Length: " + String(total));
        c.println("Connection: close");
        c.println();
        c.print(head);

        uint8_t* p = fb->buf;
        size_t rem = fb->len;
        while (rem > 0) {
            size_t chunk = min(rem, (size_t)2048);
            c.write(p, chunk);
            p += chunk;
            rem -= chunk;
        }
        c.print(tail);

        unsigned long ts = millis();
        while (!c.available() && millis() - ts < 15000) delay(10);
        String resp = ""; while (c.available()) resp += (char)c.read();
        if (resp.indexOf("\"ok\":true") >= 0) {
            Serial.println("[CAM] Foto enviada.");
            return true;
        }
        Serial.println("[CAM] Erro. Tentando novamente...");
        delay(3000);
    }
    Serial.println("[CAM] Falha ao enviar foto.");
    return false;
}

// ── Captura + envia ───────────────────────────────
void capturarEEnviar() {
    if (millis() - tUltimaFoto < COOLDOWN_MS) {
        Serial.println("[CAM] Cooldown ativo — foto ignorada.");
        return;
    }

#if FLASH_LIGADO
    digitalWrite(PIN_FLASH, HIGH);
    delay(150);
#endif

    // Descarta primeiro frame (sensor precisa estabilizar)
    camera_fb_t* fb = esp_camera_fb_get();
    esp_camera_fb_return(fb);
    delay(100);
    fb = esp_camera_fb_get();

#if FLASH_LIGADO
    digitalWrite(PIN_FLASH, LOW);
#endif

    if (!fb) {
        Serial.println("[CAM] Falha na captura.");
        return;
    }
    Serial.printf("[CAM] Frame capturado: %u bytes\n", fb->len);
    if (enviarFoto(fb, montarLegenda(true)))
        tUltimaFoto = millis();
    esp_camera_fb_return(fb);
}

// ── WiFi ──────────────────────────────────────────
void conectarWiFi() {
    Serial.print("[WiFi] Conectando");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
    int t = 0;
    while (WiFi.status() != WL_CONNECTED && t < 40) { delay(500); Serial.print("."); t++; }
    if (WiFi.status() == WL_CONNECTED) {
        WiFi.config(WiFi.localIP(), WiFi.gatewayIP(), WiFi.subnetMask(),
                    IPAddress(8,8,8,8), IPAddress(8,8,4,4));
        Serial.println("\n[WiFi] IP: " + WiFi.localIP().toString());
        delay(2000);
        String msg = "\xF0\x9F\x9F\xA2 <b>CAM CONECTADA</b>\n"
                     "\xF0\x9F\x8C\x90 IP: <code>" + WiFi.localIP().toString() + "</code>\n"
                     "<i>CIE 1125 | aguardando rel\xC3\xAA de alarme.</i>";
        enviarTexto(msg);
    } else {
        Serial.println("\n[WiFi] Falha.");
    }
    tUltimaReconexao = millis();
}

// ── Setup / Loop ──────────────────────────────────
void setup() {
    Serial.begin(115200);
    pinMode(PIN_FLASH,   OUTPUT); digitalWrite(PIN_FLASH,   LOW);
    pinMode(PIN_LED_RED, OUTPUT); digitalWrite(PIN_LED_RED, HIGH);
    pinMode(PIN_RELE,    INPUT_PULLUP);

    Serial.println("\n=== ESP32-CAM CIE 1125 \xE2\x86\x92 Telegram ===");
    if (iniciarCamera()) Serial.println("[CAM] Camera OK");
    else                 Serial.println("[CAM] ERRO: camera nao iniciou");

    conectarWiFi();
    Serial.println("[OK] Aguardando rele de alarme...\n");
}

void loop() {
    if (WiFi.status() != WL_CONNECTED)
        if (millis() - tUltimaReconexao > 30000) conectarWiFi();

    bool releAtivo = (digitalRead(PIN_RELE) == LOW);

    if (releAtivo && !alarmAtivo) {
        alarmAtivo = true;
        Serial.println("[ALARME] Rele acionado.");
        delay(DEBOUNCE_MS);
        digitalWrite(PIN_LED_RED, LOW);    // LED vermelho aceso
        capturarEEnviar();
    }

    if (!releAtivo && alarmAtivo) {
        alarmAtivo = false;
        Serial.println("[NORMAL] Rele liberado.");
        digitalWrite(PIN_LED_RED, HIGH);   // LED vermelho apagado
        enviarTexto(montarLegenda(false));
    }

    delay(100);
}
