/*
 * Controle de Caixa d'Agua via LTE-M/NB-IoT + Telegram
 * Hardware : LilyGO T-SIM7000G + Sensor JSN-SR04T + Rele
 * Chip SIM : Algar Telecom M2M/IoT
 *
 * Bibliotecas necessarias (Arduino IDE):
 *   - TinyGSM             by vshymanskyy
 *   - SSLClient           by OPEnSLab-OSU
 *   - UniversalTelegramBot by Brian Lough
 *   - ArduinoJson         by Benoit Blanchon
 */

#define TINY_GSM_MODEM_SIM7000
#include <TinyGsmClient.h>
#include <SSLClient.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "config.h"

// ─── MODEM ────────────────────────────────────────────────────────────────────
HardwareSerial SerialAT(1);
TinyGsm        modem(SerialAT);
TinyGsmClient  gsmClient(modem);
SSLClient      secureClient(&gsmClient);
UniversalTelegramBot bot(BOT_TOKEN, secureClient);

// ─── ESTADO ───────────────────────────────────────────────────────────────────
bool alertaEnviado = false;
bool bombaLigada   = false;
unsigned long ultimaMedicao = 0;
unsigned long ultimoStatus  = 0;

// ─── MODEM: LIGAR / DESLIGAR ──────────────────────────────────────────────────
void ligarModem() {
    pinMode(SIM7000_PWR_PIN, OUTPUT);
    digitalWrite(SIM7000_PWR_PIN, HIGH);
    delay(1000);
    digitalWrite(SIM7000_PWR_PIN, LOW);
    delay(5000);  // aguarda o modem inicializar
}

void desligarModem() {
    pinMode(SIM7000_PWR_PIN, OUTPUT);
    digitalWrite(SIM7000_PWR_PIN, HIGH);
    delay(1500);
    digitalWrite(SIM7000_PWR_PIN, LOW);
}

// ─── GPRS / LTE-M / NB-IoT ───────────────────────────────────────────────────
void conectar() {
    Serial.println("[MODEM] Inicializando...");
    ligarModem();

    SerialAT.begin(9600, SERIAL_8N1, SIM7000_RX_PIN, SIM7000_TX_PIN);
    delay(2000);

    if (!modem.restart()) {
        Serial.println("[MODEM] Falha no restart. Tentando init...");
        modem.init();
    }

    // Define modo de rede (NB-IoT + LTE-M por padrao)
    modem.sendAT("+CNMP=" + String(NETWORK_MODE));
    delay(500);
    modem.sendAT("+CMNB=3");  // NB-IoT + LTE-M
    delay(500);

    Serial.println("[MODEM] Aguardando rede...");
    if (!modem.waitForNetwork(90000L)) {
        Serial.println("[MODEM] ERRO: sem cobertura!");
        return;
    }

    String op   = modem.getOperator();
    int    rssi = modem.getSignalQuality();
    Serial.println("[MODEM] Operadora : " + op);
    Serial.println("[MODEM] Sinal     : " + String(rssi) + "/31");

    Serial.println("[MODEM] Conectando dados...");
    if (!modem.gprsConnect(APN, APN_USER, APN_PASS)) {
        Serial.println("[MODEM] ERRO: falha no GPRS/LTE!");
        return;
    }
    Serial.println("[MODEM] Conectado!");
}

// ─── SENSOR ULTRASSONICO ──────────────────────────────────────────────────────
float medirDistanciaCm() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);
    long dur = pulseIn(ECHO_PIN, HIGH, 30000);
    if (dur == 0) return -1;
    return dur * 0.034f / 2.0f;
}

float medirMediaCm(int amostras = 5) {
    float soma = 0;
    int validas = 0;
    for (int i = 0; i < amostras; i++) {
        float d = medirDistanciaCm();
        if (d > 0 && d < 400) { soma += d; validas++; }
        delay(100);
    }
    return validas > 0 ? soma / validas : -1;
}

int distParaPorcento(float dist) {
    long p = map((long)dist, DIST_CAIXA_VAZIA, DIST_CAIXA_CHEIA, 0, 100);
    return (int)constrain(p, 0, 100);
}

// ─── BOMBA ────────────────────────────────────────────────────────────────────
void ligarBomba() {
    digitalWrite(RELAY_PIN,   HIGH);
    digitalWrite(LED_STATUS,  HIGH);
    bombaLigada = true;
}

void desligarBomba() {
    digitalWrite(RELAY_PIN,  LOW);
    digitalWrite(LED_STATUS, LOW);
    bombaLigada = false;
}

// ─── SETUP ────────────────────────────────────────────────────────────────────
void setup() {
    Serial.begin(115200);

    pinMode(TRIG_PIN,   OUTPUT);
    pinMode(ECHO_PIN,   INPUT);
    pinMode(RELAY_PIN,  OUTPUT);
    pinMode(LED_STATUS, OUTPUT);
    desligarBomba();

    // Pisca LED: sistema iniciando
    for (int i = 0; i < 4; i++) {
        digitalWrite(LED_STATUS, HIGH); delay(150);
        digitalWrite(LED_STATUS, LOW);  delay(150);
    }

    conectar();

    String msg = "Sistema iniciado! (LilyGO T-SIM7000G)\n";
    msg += "Operadora: " + modem.getOperator() + "\n";
    msg += "Sinal: " + String(modem.getSignalQuality()) + "/31\n";
    msg += "Monitorando caixa d'agua...";
    bot.sendMessage(CHAT_ID, msg, "");

    Serial.println("[SETUP] Pronto!");
}

// ─── LOOP ─────────────────────────────────────────────────────────────────────
void loop() {
    unsigned long agora = millis();

    // Reconecta se perder sinal
    if (!modem.isGprsConnected()) {
        Serial.println("[MODEM] Reconectando...");
        conectar();
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
        Serial.printf("[NIVEL] %.1fcm -> %d%%\n", dist, nivel);

        // Nivel baixo: alerta + liga bomba
        if (dist >= DIST_CAIXA_VAZIA && !alertaEnviado) {
            String msg = "ALERTA: Caixa d'agua baixa!\n";
            msg += "Nivel : " + String(nivel) + "%\n";
            msg += "Dist  : " + String(dist, 1) + "cm\n";
            msg += "Bomba ligada automaticamente.";
            bot.sendMessage(CHAT_ID, msg, "");
            ligarBomba();
            alertaEnviado = true;
            Serial.println("[ALERTA] Bomba ligada!");
        }

        // Nivel ok: desliga bomba
        if (dist <= DIST_CAIXA_CHEIA && alertaEnviado) {
            String msg = "Caixa d'agua abastecida!\n";
            msg += "Nivel : " + String(nivel) + "%\n";
            msg += "Bomba desligada.";
            bot.sendMessage(CHAT_ID, msg, "");
            desligarBomba();
            alertaEnviado = false;
            Serial.println("[OK] Bomba desligada.");
        }
    }

    // Status periodico
    if (agora - ultimoStatus >= INTERVALO_STATUS_MS) {
        ultimoStatus = agora;
        float dist  = medirMediaCm();
        int   nivel = distParaPorcento(dist);
        int   rssi  = modem.getSignalQuality();

        String msg = "--- Status do Sistema ---\n";
        msg += "Nivel : " + String(nivel) + "%\n";
        msg += "Bomba : " + String(bombaLigada ? "LIGADA" : "DESLIGADA") + "\n";
        msg += "Sinal : " + String(rssi) + "/31\n";
        msg += "Uptime: " + String(agora / 60000) + " min";
        bot.sendMessage(CHAT_ID, msg, "");
    }
}
