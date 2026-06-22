/*
 * ESP32 #2 - Gateway (ESP32 Dev, WiFi) + Tanque Inferior
 * Funcao: ESP unico que substitui o Gateway do Tanque Superior e o ESP32
 *         do Tanque Inferior:
 *
 *   1) Recebe o nivel da caixa do Tanque Superior via ESP-NOW (ESP32 #1,
 *      sem WiFi), publica no Supabase (tabela "leituras") e dispara
 *      alertas no grupo do WhatsApp via Evolution API
 *   2) Le localmente os sensores do Tanque Inferior (nivel via JSN-SR04T,
 *      4 entradas do quadro/inversor da bomba, temperatura DS18B20 e
 *      vibracao via acelerometro MPU-6050/I2C), publica no Supabase
 *      (tabela "tanque_inferior") e dispara alertas no WhatsApp
 *
 * WiFi: configuravel no local via WiFiManager. Se nao conectar na rede salva,
 * o ESP32 cria o AP "CaixaDagua-Setup" (senha 12345678) com portal para
 * escolher a rede e digitar a senha do WiFi do condominio.
 *
 * Placa no Arduino IDE: "ESP32 Dev Module"
 *
 * Bibliotecas necessarias:
 *   - ArduinoJson       by Benoit Blanchon
 *   - WiFiManager       by tzapu
 *   - OneWire           by Jim Studt / Paul Stoffregen
 *   - DallasTemperature by Miles Burton
 *   (HTTPClient, WiFiClientSecure, WiFi e Wire ja vem no ESP32 core;
 *    o MPU-6050 e lido direto pelos registradores I2C, sem biblioteca extra)
 */

#include <esp_now.h>
#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <Wire.h>
#include "config.h"

// ════════════════════════════════════════════════════════════════════════
// TANQUE SUPERIOR (recebido via ESP-NOW do ESP32 #1)
// ════════════════════════════════════════════════════════════════════════

// Estrutura identica a do esp32_sensor!
typedef struct {
    float    distancia;
    int      nivel;
    unsigned long uptime;
    bool     leituraValida;
} DadosSensor;

DadosSensor dadosTS;
volatile bool dadosNovosTS = false;

bool tsAlertaNivelEnviado     = false;
bool tsAlertaSemSinal         = false;
bool tsLeituraInvalidaAvisada = false;
unsigned long ultimoStatus     = 0;
unsigned long tsUltimoRecebido = 0;

// ════════════════════════════════════════════════════════════════════════
// TANQUE INFERIOR (sensores e entradas ligados neste mesmo ESP32)
// ════════════════════════════════════════════════════════════════════════

OneWire oneWire(TEMP_PIN);
DallasTemperature sensoresTemp(&oneWire);

float tiDistanciaCm = -1;
int   tiNivelPct    = 0;
bool  tiEntrada1_bombaLigada      = false;
bool  tiEntrada2_bombaFalhou      = false;
bool  tiEntrada3_falhaInversor    = false;
bool  tiEntrada4_painelSemEnergia = false;
float tiTemperaturaC  = NAN;
float tiVibracaoValor = -1;   // vibracao RMS em mili-g (MPU-6050); -1 = sensor ausente
bool  tiVibracaoAlerta = false;
bool  mpuOk = false;          // MPU-6050 detectado e inicializado no I2C
bool  tiLeituraValida  = false;

unsigned long tiUltimaMedicao = 0;

bool tiAlertaBombaFalhouEnviado = false;
bool tiAlertaInversorEnviado    = false;
bool tiAlertaPainelEnviado      = false;
bool tiAlertaNivelBaixoEnviado  = false;
bool tiAlertaTemperaturaEnviado = false;
bool tiAlertaVibracaoEnviado    = false;
bool tiLeituraInvalidaAvisada   = false;

// ════════════════════════════════════════════════════════════════════════
// ESTADO DA CONEXAO WIFI (alerta de queda/reconexao)
// ════════════════════════════════════════════════════════════════════════

bool wifiEstavaConectado = true;   // assume conectado: conectarWiFi() ja confirmou no setup()
unsigned long wifiQuedaInicio = 0;

// ════════════════════════════════════════════════════════════════════════
// MONITORAMENTO DA CAMERA (ESP32-CAM, Alarme de Incendio) via Supabase
// ════════════════════════════════════════════════════════════════════════

String camUltimoCreatedAt    = "";
int    camChecksSemMudanca   = 0;
bool   camAlertaOffline      = false;
unsigned long camUltimaVerificacao = 0;

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(DadosSensor)) {
        memcpy((void *)&dadosTS, data, sizeof(DadosSensor));
        dadosNovosTS     = true;
        tsUltimoRecebido = millis();
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

// ════════════════════════════════════════════════════════════════════════
// WHATSAPP / SUPABASE
// ════════════════════════════════════════════════════════════════════════

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

// Envia a leitura do Tanque Superior para o Supabase (tabela "leituras")
bool enviarSupabaseTanqueSuperior() {
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
    doc["distancia_cm"]    = dadosTS.distancia;
    doc["nivel_pct"]       = dadosTS.nivel;
    doc["sensor_uptime_s"] = dadosTS.uptime;
    doc["leitura_valida"]  = dadosTS.leituraValida;
    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    bool ok = (code >= 200 && code < 300);
    Serial.printf("[SUPABASE] HTTP %d %s\n", code, ok ? "OK" : http.getString().c_str());
    http.end();
    return ok;
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

// Le as 4 entradas digitais (contato seco para GND = ativo, INPUT_PULLUP)
void lerEntradas() {
    tiEntrada1_bombaLigada      = (digitalRead(ENTRADA1_PIN) == LOW);
    tiEntrada2_bombaFalhou      = (digitalRead(ENTRADA2_PIN) == LOW);
    tiEntrada3_falhaInversor    = (digitalRead(ENTRADA3_PIN) == LOW);
    tiEntrada4_painelSemEnergia = (digitalRead(ENTRADA4_PIN) == LOW);
}

// Le a temperatura da bomba via DS18B20. Retorna NAN se o sensor
// estiver desconectado ou com erro de leitura.
float lerTemperatura() {
    sensoresTemp.requestTemperatures();
    float t = sensoresTemp.getTempCByIndex(0);
    if (t == DEVICE_DISCONNECTED_C) return NAN;
    return t;
}

// ─── MPU-6050 (acelerometro I2C na carcaca da bomba) ────────────────────
// Leitura direta pelos registradores, sem biblioteca extra. Faixa padrao
// de ±2g -> 16384 LSB por g.

// Acorda o MPU-6050 (sai do sleep de fabrica). Retorna true se respondeu.
bool iniciarMPU() {
    Wire.beginTransmission(MPU_I2C_ADDR);
    Wire.write(0x6B);   // PWR_MGMT_1
    Wire.write(0x00);   // clock interno, sleep desligado
    mpuOk = (Wire.endTransmission() == 0);
    if (mpuOk) {
        Serial.println("[MPU6050] Acelerometro detectado e inicializado!");
    } else {
        Serial.println("[MPU6050] NAO detectado! Verifique SDA/SCL/VCC/GND.");
    }
    return mpuOk;
}

// Le uma amostra bruta do acelerometro (registradores 0x3B-0x40)
bool lerAcelRaw(int16_t &ax, int16_t &ay, int16_t &az) {
    Wire.beginTransmission(MPU_I2C_ADDR);
    Wire.write(0x3B);   // ACCEL_XOUT_H
    if (Wire.endTransmission(false) != 0) return false;
    if (Wire.requestFrom((int)MPU_I2C_ADDR, 6) != 6) return false;
    ax = (Wire.read() << 8) | Wire.read();
    ay = (Wire.read() << 8) | Wire.read();
    az = (Wire.read() << 8) | Wire.read();
    return true;
}

// Mede a vibracao com o MPU-6050: coleta varias amostras da magnitude da
// aceleracao e calcula o RMS do desvio em torno da media (assim a gravidade
// e a inclinacao do sensor se cancelam — sobra so a vibracao). Retorna o
// valor em mili-g, ou -1 se o sensor nao estiver respondendo.
float lerVibracao(int amostras = 200) {
    if (!mpuOk && !iniciarMPU()) return -1;   // tenta religar se caiu

    float soma = 0, somaQuad = 0;
    int validas = 0;
    for (int i = 0; i < amostras; i++) {
        int16_t ax, ay, az;
        if (lerAcelRaw(ax, ay, az)) {
            float mag = sqrtf((float)ax * ax + (float)ay * ay + (float)az * az);
            soma     += mag;
            somaQuad += mag * mag;
            validas++;
        }
        delay(2);   // ~200 amostras em ~0,5s
    }
    if (validas < amostras / 2) {   // maioria das leituras falhou
        mpuOk = false;
        return -1;
    }
    float media    = soma / validas;
    float variancia = somaQuad / validas - media * media;
    if (variancia < 0) variancia = 0;
    // desvio RMS em LSB -> mili-g (±2g = 16384 LSB/g)
    return sqrtf(variancia) / 16384.0f * 1000.0f;
}

// Pisca o LED onboard rapidamente (indica envio realizado)
void piscarLed() {
    digitalWrite(LED_PIN, HIGH); delay(80);
    digitalWrite(LED_PIN, LOW);
}

// Mede o nivel, le as entradas, temperatura e vibracao do Tanque Inferior,
// atualizando as variaveis globais ti*. Usada tanto no loop (a cada
// INTERVALO_MEDICAO_MS) quanto uma vez no setup() para a mensagem inicial.
void medirTanqueInferior() {
    float dist      = medirMediaCm();
    tiLeituraValida = (dist > 0 && dist < 400);
    tiDistanciaCm   = dist;
    tiNivelPct      = tiLeituraValida ? distParaPorcento(dist) : 0;

    lerEntradas();
    tiTemperaturaC   = lerTemperatura();
    tiVibracaoValor  = lerVibracao();
    tiVibracaoAlerta = (tiVibracaoValor >= 0 && tiVibracaoValor > VIBRACAO_LIMIAR);
}

// Envia a leitura do Tanque Inferior para o Supabase (tabela "tanque_inferior")
bool enviarSupabaseTanqueInferior() {
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
    doc["distancia_cm"]                = tiDistanciaCm;
    doc["nivel_pct"]                   = tiNivelPct;
    doc["entrada1_bomba_ligada"]       = tiEntrada1_bombaLigada;
    doc["entrada2_bomba_falhou"]       = tiEntrada2_bombaFalhou;
    doc["entrada3_falha_inversor"]     = tiEntrada3_falhaInversor;
    doc["entrada4_painel_sem_energia"] = tiEntrada4_painelSemEnergia;
    if (isnan(tiTemperaturaC)) {
        doc["temperatura_c"] = nullptr;
    } else {
        doc["temperatura_c"] = tiTemperaturaC;
    }
    if (tiVibracaoValor < 0) {
        doc["vibracao_valor"] = nullptr;   // MPU-6050 ausente/sem resposta
    } else {
        doc["vibracao_valor"] = tiVibracaoValor;
    }
    doc["vibracao_alerta"]  = tiVibracaoAlerta;
    doc["sensor_uptime_s"]  = millis() / 1000;
    doc["leitura_valida"]   = tiLeituraValida;
    String body;
    serializeJson(doc, body);

    int code = http.POST(body);
    bool ok = (code >= 200 && code < 300);
    Serial.printf("[SUPABASE] HTTP %d %s\n", code, ok ? "OK" : http.getString().c_str());
    http.end();
    return ok;
}

// Substitui espacos por %20 (uso em query string da PostgREST)
String urlEncodeCondominio() {
    String s = String(CONDOMINIO_NOME);
    s.replace(" ", "%20");
    return s;
}

// Consulta a ultima leitura da tabela "alarme_incendio" (ESP32-CAM) no
// Supabase. Se o "created_at" nao mudar por CAM_TIMEOUT_CHECKS verificacoes
// seguidas, considera a camera/alarme offline e avisa no WhatsApp (e de
// novo quando ela voltar a enviar dados).
void verificarCamera() {
    if (WiFi.status() != WL_CONNECTED) return;

    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    String url = String(SUPABASE_URL) +
        "/rest/v1/alarme_incendio?select=created_at&condominio=eq." +
        urlEncodeCondominio() + "&order=created_at.desc&limit=1";
    http.begin(client, url);
    http.addHeader("apikey", SUPABASE_KEY);
    http.addHeader("Authorization", "Bearer " + String(SUPABASE_KEY));
    http.setTimeout(10000);

    int code = http.GET();
    if (code != 200) {
        Serial.printf("[CAMERA] HTTP %d ao verificar status\n", code);
        http.end();
        return;
    }

    String body = http.getString();
    http.end();

    JsonDocument doc;
    if (deserializeJson(doc, body) != DeserializationError::Ok || doc.size() == 0) {
        return;  // tabela vazia (camera nunca enviou ainda) ou erro de parse
    }

    String createdAt = doc[0]["created_at"].as<String>();

    if (camUltimoCreatedAt == "") {
        camUltimoCreatedAt = createdAt;
        return;
    }

    if (createdAt == camUltimoCreatedAt) {
        camChecksSemMudanca++;
        if (camChecksSemMudanca >= CAM_TIMEOUT_CHECKS && !camAlertaOffline) {
            enviarWhatsApp(
                "\xE2\x9A\xA0\xEF\xB8\x8F *ATENCAO: Camera do Alarme de Incendio sem comunicacao!*\n"
                "O ESP32-CAM nao envia dados ha alguns minutos.\n"
                "Verifique alimentacao e WiFi do modulo.");
            camAlertaOffline = true;
        }
    } else {
        camUltimoCreatedAt  = createdAt;
        camChecksSemMudanca = 0;
        if (camAlertaOffline) {
            enviarWhatsApp(
                "\xE2\x9C\x85 *Camera do Alarme de Incendio online novamente!*");
            camAlertaOffline = false;
        }
    }
}

void setup() {
    Serial.begin(115200);

    // Pinos do Tanque Inferior (sensores e entradas locais)
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    pinMode(LED_PIN,  OUTPUT);
    pinMode(ENTRADA1_PIN, INPUT_PULLUP);
    pinMode(ENTRADA2_PIN, INPUT_PULLUP);
    pinMode(ENTRADA3_PIN, INPUT_PULLUP);
    pinMode(ENTRADA4_PIN, INPUT_PULLUP);
    sensoresTemp.begin();

    // Acelerometro MPU-6050 (vibracao da bomba) via I2C
    Wire.begin(MPU_SDA_PIN, MPU_SCL_PIN);
    iniciarMPU();

    // WiFi DEVE conectar antes do ESP-NOW para fixar o canal
    conectarWiFi();

    Serial.print("[GATEWAY] MAC: ");
    Serial.println(WiFi.macAddress());  // <- informe este MAC no config.h do sensor!

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ERRO na inicializacao!");
        return;
    }
    esp_now_register_recv_cb(onDataReceived);

    tsUltimoRecebido = millis();

    // Primeira leitura do Tanque Inferior (sensores locais), para a
    // mensagem inicial ja sair com o nivel atual
    medirTanqueInferior();
    tiUltimaMedicao = millis();

    // Aguarda ate 12s pela primeira leitura do Tanque Superior via ESP-NOW
    // (o ESP32 #1 envia a cada 30s, entao pode nao chegar a tempo)
    Serial.println("[SETUP] Aguardando 1a leitura do Tanque Superior (ESP-NOW)...");
    unsigned long esperaInicio = millis();
    while (!dadosNovosTS && millis() - esperaInicio < 12000UL) {
        delay(100);
    }

    String msgInicial = "\xF0\x9F\x94\x8C *Gateway reiniciado!*\n\n";
    if (dadosNovosTS) {
        msgInicial += "\xF0\x9F\x92\xA7 Tanque Superior: *" + String(dadosTS.nivel) + "%*\n";
    } else {
        msgInicial += "\xF0\x9F\x92\xA7 Tanque Superior: aguardando sinal do sensor...\n";
    }
    msgInicial += "\xF0\x9F\x9A\xB0 Tanque Inferior: *" + String(tiNivelPct) + "%*";
    enviarWhatsApp(msgInicial);
    // dadosNovosTS NAO e zerado aqui: o loop() ainda processa essa 1a
    // leitura normalmente (envia ao Supabase e roda os alertas de nivel)

    Serial.println("[SETUP] Gateway pronto!");
}

void loop() {
    unsigned long agora = millis();

    // ════════ WIFI: deteccao de queda/reconexao ════════
    bool wifiConectado = (WiFi.status() == WL_CONNECTED);

    if (!wifiConectado && wifiEstavaConectado) {
        // Acabou de cair
        wifiQuedaInicio    = agora;
        wifiEstavaConectado = false;
        Serial.println("[WiFi] Conexao perdida!");
    }

    if (wifiConectado && !wifiEstavaConectado) {
        // Reconectou sozinho: avisa quanto tempo ficou offline
        unsigned long quedaS = (agora - wifiQuedaInicio) / 1000;
        String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *WiFi do gateway caiu e reconectou sozinho*\n\n";
        msg += "Ficou offline por aproximadamente " + String(quedaS) + "s.";
        enviarWhatsApp(msg);
        wifiEstavaConectado = true;
    }

    // Queda temporaria do WiFi: reconecta na rede salva (sem abrir o portal)
    static unsigned long ultimaTentativaWiFi = 0;
    if (!wifiConectado && agora - ultimaTentativaWiFi >= 15000UL) {
        ultimaTentativaWiFi = agora;
        Serial.println("[WiFi] Reconectando...");
        WiFi.reconnect();
    }

    // ════════ CAMERA (Alarme de Incendio): verificacao via Supabase ════════
    if (agora - camUltimaVerificacao >= CAM_CHECK_INTERVALO_MS) {
        camUltimaVerificacao = agora;
        verificarCamera();
    }

    // ════════ TANQUE SUPERIOR (via ESP-NOW) ════════

    // Sensor sem comunicacao
    if (agora - tsUltimoRecebido >= TIMEOUT_SENSOR_MS && tsUltimoRecebido > 0) {
        if (!tsAlertaSemSinal) {
            enviarWhatsApp(
                "\xE2\x9A\xA0\xEF\xB8\x8F *ATENCAO: Sensor sem comunicacao!*\n"
                "O ESP32 da caixa d'agua nao envia dados ha mais de 2 minutos.\n"
                "Verifique alimentacao e alcance do ESP-NOW.");
            tsAlertaSemSinal = true;
        }
    }

    if (dadosNovosTS) {
        dadosNovosTS = false;

        // Sensor voltou a comunicar depois de uma queda
        if (tsAlertaSemSinal) {
            enviarWhatsApp(
                "\xE2\x9C\x85 *Sensor online novamente!*\n"
                "O ESP32 da caixa d'agua voltou a enviar dados.");
            tsAlertaSemSinal = false;
        }

        Serial.printf("[CAIXA SUPERIOR] %.1fcm | %d%% | up %lus\n",
            dadosTS.distancia, dadosTS.nivel, dadosTS.uptime);

        enviarSupabaseTanqueSuperior();

        // Leitura invalida do sensor
        if (!dadosTS.leituraValida) {
            if (!tsLeituraInvalidaAvisada) {
                enviarWhatsApp(
                    "\xE2\x9A\xA0\xEF\xB8\x8F *Sensor com leitura invalida!*\n"
                    "Verifique o JSN-SR04T na caixa d'agua.");
                tsLeituraInvalidaAvisada = true;
            }
        } else {
            // Sensor voltou a dar leitura valida depois de invalida
            if (tsLeituraInvalidaAvisada) {
                enviarWhatsApp(
                    "\xE2\x9C\x85 *Leitura do JSN-SR04T normalizada* (caixa d'agua).");
                tsLeituraInvalidaAvisada = false;
            }

            // ── Nivel baixo ──
            if (dadosTS.nivel <= NIVEL_ALERTA && !tsAlertaNivelEnviado) {
                String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *Nivel baixo na caixa d'agua!*\n\n";
                msg += "\xF0\x9F\x93\x8A Nivel: *" + String(dadosTS.nivel) + "%*\n";
                msg += "\xF0\x9F\x93\x8F Distancia: " + String(dadosTS.distancia, 1) + "cm";
                enviarWhatsApp(msg);
                tsAlertaNivelEnviado = true;
            }

            // ── Caixa abastecida ──
            if (dadosTS.nivel >= NIVEL_OK && tsAlertaNivelEnviado) {
                String msg = "\xE2\x9C\x85 *Caixa d'agua abastecida!*\n\n";
                msg += "\xF0\x9F\x93\x8A Nivel: *" + String(dadosTS.nivel) + "%*";
                enviarWhatsApp(msg);
                tsAlertaNivelEnviado = false;
            }
        }
    }

    // Status periodico (opcional, desativado por padrao)
    if (INTERVALO_STATUS_MS > 0 && agora - ultimoStatus >= INTERVALO_STATUS_MS) {
        ultimoStatus = agora;
        String msg = "\xF0\x9F\x93\x8B *Status do sistema*\n";
        msg += "Caixa Superior: " + String(dadosTS.nivel) + "%\n";
        msg += "Sensor online: " + String(dadosTS.uptime / 60) + " min";
        enviarWhatsApp(msg);
    }

    // ════════ TANQUE INFERIOR (sensores locais) ════════

    if (agora - tiUltimaMedicao >= INTERVALO_MEDICAO_MS) {
        tiUltimaMedicao = agora;

        medirTanqueInferior();

        if (tiLeituraValida) {
            Serial.printf("[TANQUE INFERIOR] %.1fcm -> %d%% | E1(Bomba ligou): %d | E2(Bomba falhou): %d | E3(Falha inversor): %d | E4(Painel sem energia): %d | Temp: %.1fC | Vibracao: %.0f mg\n",
                tiDistanciaCm, tiNivelPct,
                tiEntrada1_bombaLigada,
                tiEntrada2_bombaFalhou,
                tiEntrada3_falhaInversor,
                tiEntrada4_painelSemEnergia,
                tiTemperaturaC,
                tiVibracaoValor);
        } else {
            Serial.println("[TANQUE INFERIOR] Leitura invalida!");
        }

        enviarSupabaseTanqueInferior();
        piscarLed();

        // Leitura invalida do sensor ultrassonico
        if (!tiLeituraValida) {
            if (!tiLeituraInvalidaAvisada) {
                enviarWhatsApp(
                    "\xE2\x9A\xA0\xEF\xB8\x8F *Sensor com leitura invalida!*\n"
                    "Verifique o JSN-SR04T no Tanque Inferior.");
                tiLeituraInvalidaAvisada = true;
            }
            return;
        }
        // Sensor voltou a dar leitura valida depois de invalida
        if (tiLeituraInvalidaAvisada) {
            enviarWhatsApp(
                "\xE2\x9C\x85 *Leitura do JSN-SR04T normalizada* (Tanque Inferior).");
            tiLeituraInvalidaAvisada = false;
        }

        // ── ENTRADA 2: Bomba falhou (prioridade maxima) ──
        if (tiEntrada2_bombaFalhou && !tiAlertaBombaFalhouEnviado) {
            String msg = "\xF0\x9F\x9A\xA8 *FALHA NA BOMBA!* (Tanque Inferior - Entrada 2 acionada)\n\n";
            msg += "O quadro de comando sinalizou falha na bomba do Tanque Inferior.\n";
            msg += "Possiveis causas: bomba queimada, falta d'agua no poco, registro fechado, protecao do motor.\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel atual: *" + String(tiNivelPct) + "%*";
            enviarWhatsApp(msg);
            tiAlertaBombaFalhouEnviado = true;
        }
        if (!tiEntrada2_bombaFalhou && tiAlertaBombaFalhouEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Falha na bomba normalizada* (Tanque Inferior).");
            tiAlertaBombaFalhouEnviado = false;
        }

        // ── ENTRADA 3: Falha no inversor ──
        if (tiEntrada3_falhaInversor && !tiAlertaInversorEnviado) {
            String msg = "\xF0\x9F\x9A\xA8 *FALHA NO INVERSOR!* (Tanque Inferior - Entrada 3 acionada)\n\n";
            msg += "Verifique o inversor/quadro de energia da bomba do Tanque Inferior.";
            enviarWhatsApp(msg);
            tiAlertaInversorEnviado = true;
        }
        if (!tiEntrada3_falhaInversor && tiAlertaInversorEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Falha no inversor normalizada* (Tanque Inferior).");
            tiAlertaInversorEnviado = false;
        }

        // ── ENTRADA 4: Painel sem energia ──
        if (tiEntrada4_painelSemEnergia && !tiAlertaPainelEnviado) {
            String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *PAINEL SEM ENERGIA!* (Tanque Inferior - Entrada 4 acionada)\n\n";
            msg += "O quadro da bomba do Tanque Inferior esta sem alimentacao da rede eletrica.";
            enviarWhatsApp(msg);
            tiAlertaPainelEnviado = true;
        }
        if (!tiEntrada4_painelSemEnergia && tiAlertaPainelEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Energia do painel restabelecida!* (Tanque Inferior).");
            tiAlertaPainelEnviado = false;
        }

        // ── Nivel baixo ──
        if (tiNivelPct <= TI_NIVEL_ALERTA && !tiAlertaNivelBaixoEnviado) {
            String msg = "\xE2\x9A\xA0\xEF\xB8\x8F *Nivel baixo no Tanque Inferior!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(tiNivelPct) + "%*\n";
            msg += "\xF0\x9F\x93\x8F Distancia: " + String(tiDistanciaCm, 1) + "cm";
            if (tiEntrada1_bombaLigada) {
                msg += "\n\xF0\x9F\x9F\xA2 Entrada 1 acionada: Bomba LIGADA";
            }
            enviarWhatsApp(msg);
            tiAlertaNivelBaixoEnviado = true;
        }

        // ── Tanque abastecido ──
        if (tiNivelPct >= TI_NIVEL_OK && tiAlertaNivelBaixoEnviado) {
            String msg = "\xE2\x9C\x85 *Tanque Inferior abastecido!*\n\n";
            msg += "\xF0\x9F\x93\x8A Nivel: *" + String(tiNivelPct) + "%*";
            enviarWhatsApp(msg);
            tiAlertaNivelBaixoEnviado = false;
        }

        // ── Temperatura da bomba ──
        if (!isnan(tiTemperaturaC)) {
            if (tiTemperaturaC > TEMP_ALERTA_C && !tiAlertaTemperaturaEnviado) {
                String msg = "\xF0\x9F\x8C\xA1\xEF\xB8\x8F *Temperatura alta na bomba (Tanque Inferior)!*\n\n";
                msg += "\xF0\x9F\x8C\xA1\xEF\xB8\x8F Temperatura: *" + String(tiTemperaturaC, 1) + " C*\n";
                msg += "Verifique o funcionamento e a refrigeracao da bomba.";
                enviarWhatsApp(msg);
                tiAlertaTemperaturaEnviado = true;
            }
            if (tiTemperaturaC <= TEMP_ALERTA_C && tiAlertaTemperaturaEnviado) {
                enviarWhatsApp("\xE2\x9C\x85 *Temperatura da bomba normalizada* (Tanque Inferior).");
                tiAlertaTemperaturaEnviado = false;
            }
        }

        // ── Vibracao excessiva ──
        if (tiVibracaoAlerta && !tiAlertaVibracaoEnviado) {
            String msg = "\xF0\x9F\x93\xB3 *Vibracao excessiva detectada na bomba (Tanque Inferior)!*\n\n";
            msg += "Vibracao: *" + String(tiVibracaoValor, 0) + " mg RMS* (limite: " + String((float)VIBRACAO_LIMIAR, 0) + " mg)\n";
            msg += "Verifique fixacao, rolamentos e alinhamento da bomba.";
            enviarWhatsApp(msg);
            tiAlertaVibracaoEnviado = true;
        }
        if (!tiVibracaoAlerta && tiAlertaVibracaoEnviado) {
            enviarWhatsApp("\xE2\x9C\x85 *Vibracao da bomba normalizada* (Tanque Inferior).");
            tiAlertaVibracaoEnviado = false;
        }
    }
}
