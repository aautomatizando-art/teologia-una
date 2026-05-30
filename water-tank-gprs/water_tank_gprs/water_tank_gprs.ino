/*
 * Controle de Caixa d'Agua via GPRS + Telegram
 * Hardware: ESP32 + SIM800L + Sensor JSN-SR04T + Rele
 *
 * Bibliotecas necessarias (Arduino IDE):
 *   - TinyGSM         by vshymanskyy
 *   - SSLClient       by OPEnSLab-OSU
 *   - UniversalTelegramBot by Brian Lough
 *   - ArduinoJson     by Benoit Blanchon
 */

#define TINY_GSM_MODEM_SIM800
#include <TinyGsmClient.h>
#include <SSLClient.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "config.h"

// ─── MODEM ──────────────────────────────────────────────────
HardwareSerial SerialAT(1);
TinyGsm        modem(SerialAT);
TinyGsmClient  gsmClient(modem);
SSLClient      secureClient(&gsmClient);
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

// ─── ESTADO ─────────────────────────────────────────────────
bool alertaEnviado  = false;
bool bombaLigada    = false;
unsigned long ultimaMedicao  = 0;
unsigned long ultimoStatus   = 0;

// ─── SENSOR ─────────────────────────────────────────────────
float medirDistanciaCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long duracao = pulseIn(ECHO_PIN, HIGH, 30000);
    if (duracao == 0) return -1;
    return duracao * 0.034f / 2.0f;
}

float medirMediaCm(int amostras = 5) {
    float soma = 0;
    int validas = 0;
    for (int i = 0; i < amostras; i++) {
        float d = medirDistanciaCm();
        if (d > 0 && d < 400) {
            soma += d;
            validas++;
        }
        delay(100);
    }
    return (validas > 0) ? soma / validas : -1;
}

int distParaPorcento(float dist) {
    long pct = map((long)dist, DIST_CAIXA_VAZIA, DIST_CAIXA_CHEIA, 0, 100);
    return (int)constrain(pct, 0, 100);
}

// ─── BOMBA ──────────────────────────────────────────────────
void ligarBomba() {
    digitalWrite(RELAY_PIN, HIGH);
    bombaLigada = true;
    digitalWrite(LED_STATUS, HIGH);
}

void desligarBomba() {
    digitalWrite(RELAY_PIN, LOW);
    bombaLigada = false;
    digitalWrite(LED_STATUS, LOW);
}

// ─── GPRS ───────────────────────────────────────────────────
void conectarGPRS() {
    Serial.println("[GPRS] Reiniciando modem...");
    modem.restart();
    delay(3000);

    Serial.println("[GPRS] Aguardando rede GSM...");
    if (!modem.waitForNetwork(90000L)) {
        Serial.println("[GPRS] ERRO: sem cobertura de rede!");
        return;
    }

    Serial.print("[GPRS] Operadora: ");
    Serial.println(modem.getOperator());
    Serial.print("[GPRS] Sinal (RSSI): ");
    Serial.println(modem.getSignalQuality());

    Serial.println("[GPRS] Conectando dados...");
    if (!modem.gprsConnect(APN, APN_USER, APN_PASS)) {
        Serial.println("[GPRS] ERRO: falha ao conectar GPRS!");
        return;
    }
    Serial.println("[GPRS] Conectado!");
}

// ─── SETUP ──────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN,    OUTPUT);
    pinMode(ECHO_PIN,    INPUT);
    pinMode(RELAY_PIN,   OUTPUT);
    pinMode(LED_STATUS,  OUTPUT);
    desligarBomba();

    // Pisca LED para indicar inicializacao
    for (int i = 0; i < 3; i++) {
        digitalWrite(LED_STATUS, HIGH); delay(200);
        digitalWrite(LED_STATUS, LOW);  delay(200);
    }

    SerialAT.begin(9600, SERIAL_8N1, SIM800_RX_PIN, SIM800_TX_PIN);
    delay(2000);

    conectarGPRS();

    String msg = "Sistema iniciado!\n";
    msg += "Monitorando caixa d'agua via GPRS.\n";
    msg += "Operadora: " + modem.getOperator();
    bot.sendMessage(CHAT_ID, msg, "");

    Serial.println("[SETUP] Pronto!");
}

// ─── LOOP ───────────────────────────────────────────────────
void loop() {
    unsigned long agora = millis();

    // Reconecta GPRS se necessario
    if (!modem.isGprsConnected()) {
        Serial.println("[GPRS] Reconectando...");
        conectarGPRS();
    }

    // Medicao periodica
    if (agora - ultimaMedicao >= INTERVALO_MEDICAO_MS) {
        ultimaMedicao = agora;

        float dist = medirMediaCm();

        if (dist < 0) {
            Serial.println("[SENSOR] Leitura invalida!");
            return;
        }

        int nivel = distParaPorcento(dist);
        Serial.printf("[NIVEL] %.1fcm → %d%%\n", dist, nivel);

        // Nível baixo: alerta + liga bomba
        if (dist >= DIST_CAIXA_VAZIA && !alertaEnviado) {
            String msg = "ALERTA: Caixa d'agua baixa!\n";
            msg += "Nivel: " + String(nivel) + "%\n";
            msg += "Distancia: " + String(dist, 1) + "cm\n";
            msg += "Ligando bomba automaticamente...";
            bot.sendMessage(CHAT_ID, msg, "");
            ligarBomba();
            alertaEnviado = true;
            Serial.println("[ALERTA] Bomba ligada!");
        }

        // Nivel ok: desliga bomba
        if (dist <= DIST_CAIXA_CHEIA && alertaEnviado) {
            String msg = "Caixa d'agua abastecida!\n";
            msg += "Nivel: " + String(nivel) + "%\n";
            msg += "Bomba desligada.";
            bot.sendMessage(CHAT_ID, msg, "");
            desligarBomba();
            alertaEnviado = false;
            Serial.println("[OK] Bomba desligada.");
        }
    }

    // Relatorio de status periodico
    if (agora - ultimoStatus >= INTERVALO_STATUS_MS) {
        ultimoStatus = agora;
        float dist = medirMediaCm();
        int nivel   = distParaPorcento(dist);
        int rssi    = modem.getSignalQuality();

        String msg = "Status do sistema:\n";
        msg += "Nivel: " + String(nivel) + "%\n";
        msg += "Bomba: " + String(bombaLigada ? "LIGADA" : "DESLIGADA") + "\n";
        msg += "Sinal GSM: " + String(rssi) + "/31\n";
        msg += "Uptime: " + String(agora / 60000) + " min";
        bot.sendMessage(CHAT_ID, msg, "");
    }
}
