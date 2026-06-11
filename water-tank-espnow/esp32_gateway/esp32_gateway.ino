/*
 * ESP32 #2 - Gateway (WT32-ETH01, Ethernet RJ45) + Tanque Inferior
 * Funcao: ESP unico que substitui o Gateway do Tanque Superior e o ESP32
 *         do Tanque Inferior:
 *
 *   1) Recebe o nivel da caixa do Tanque Superior via ESP-NOW (ESP32 #1,
 *      sem WiFi), publica no Supabase (tabela "leituras") e dispara
 *      alertas no grupo do WhatsApp via Evolution API
 *   2) Le localmente os sensores do Tanque Inferior (nivel via JSN-SR04T,
 *      4 entradas do quadro/inversor da bomba, temperatura DS18B20 e
 *      vibracao), publica no Supabase (tabela "tanque_inferior") e
 *      dispara alertas no WhatsApp
 *
 * Rede: WT32-ETH01 com PHY LAN8720 (RJ45), conexao via DHCP — sem WiFi/portal.
 * O radio WiFi continua ativo (modo STA, sem se conectar a nenhuma rede)
 * apenas para o ESP-NOW falar com o ESP32 #1 (sensor do Tanque Superior).
 *
 * Placa no Arduino IDE: "WT32-ETH01"
 *
 * Bibliotecas necessarias:
 *   - ArduinoJson       by Benoit Blanchon
 *   - OneWire           by Jim Studt / Paul Stoffregen
 *   - DallasTemperature by Miles Burton
 *   (ETH, HTTPClient, WiFiClientSecure e WiFi ja vem no ESP32 core)
 */

#include <esp_now.h>
#include <WiFi.h>
#include <ETH.h>
#include "esp_wifi.h"
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include "config.h"

// ─── PINOS ETHERNET (WT32-ETH01 + LAN8720) ──────────────────────────────────
#define ETH_PHY_TYPE   ETH_PHY_LAN8720
#define ETH_PHY_ADDR   1
#define ETH_PHY_MDC    23
#define ETH_PHY_MDIO   18
#define ETH_PHY_POWER  16
#define ETH_CLK_MODE   ETH_CLOCK_GPIO0_IN

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
float tiVibracaoValor = 0;
bool  tiVibracaoAlerta = false;
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
// REDE (Ethernet + WiFi so para ESP-NOW)
// ════════════════════════════════════════════════════════════════════════

volatile bool ethConectado = false;

void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(DadosSensor)) {
        memcpy((void *)&dadosTS, data, sizeof(DadosSensor));
        dadosNovosTS    = true;
        tsUltimoRecebido = millis();
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

// ════════════════════════════════════════════════════════════════════════
// WHATSAPP / SUPABASE
// ════════════════════════════════════════════════════════════════════════

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

// Envia a leitura do Tanque Superior para o Supabase (tabela "leituras")
bool enviarSupabaseTanqueSuperior() {
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

// Envia a leitura do Tanque Inferior para o Supabase (tabela "tanque_inferior")
bool enviarSupabaseTanqueInferior() {
    if (!ethConectado) return false;

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
    doc["vibracao_valor"]   = tiVibracaoValor;
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

    // Rede (Ethernet) DEVE iniciar antes do ESP-NOW para fixar o canal WiFi
    conectarRede();

    Serial.print("[GATEWAY] MAC: ");
    Serial.println(WiFi.macAddress());  // <- informe este MAC no config.h do sensor!

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ERRO na inicializacao!");
        return;
    }
    esp_now_register_recv_cb(onDataReceived);

    tsUltimoRecebido = millis();

    enviarWhatsApp(
        "\xF0\x9F\x92\xA7 *Monitor Caixa d'Agua + Tanque Inferior iniciado!*\n"
        "Gateway online, aguardando dados...");

    Serial.println("[SETUP] Gateway pronto!");
}

void loop() {
    unsigned long agora = millis();

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
            tsLeituraInvalidaAvisada = false;

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

        float dist     = medirMediaCm();
        tiLeituraValida = (dist > 0 && dist < 400);
        tiDistanciaCm   = dist;
        tiNivelPct      = tiLeituraValida ? distParaPorcento(dist) : 0;

        lerEntradas();
        tiTemperaturaC  = lerTemperatura();
        tiVibracaoValor = lerVibracao();
        tiVibracaoAlerta = (tiVibracaoValor > VIBRACAO_LIMIAR);

        if (tiLeituraValida) {
            Serial.printf("[TANQUE INFERIOR] %.1fcm -> %d%% | E1(Bomba ligou): %d | E2(Bomba falhou): %d | E3(Falha inversor): %d | E4(Painel sem energia): %d | Temp: %.1fC | Vibracao: %.0f\n",
                dist, tiNivelPct,
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
        tiLeituraInvalidaAvisada = false;

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
            msg += "Leitura do sensor: *" + String(tiVibracaoValor, 0) + "*\n";
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
