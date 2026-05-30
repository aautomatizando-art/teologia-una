/*
 * ESP32 #2 - Gateway (proximo ao WiFi)
 * Funcao: recebe dados via ESP-NOW e envia alertas pelo Telegram
 *
 * Bibliotecas necessarias:
 *   - UniversalTelegramBot  by Brian Lough
 *   - ArduinoJson           by Benoit Blanchon
 */

#include <esp_now.h>
#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <UniversalTelegramBot.h>
#include <ArduinoJson.h>
#include "config.h"

// Estrutura deve ser identica a do esp32_sensor
typedef struct {
    float    distancia;
    int      nivel;
    bool     bombaLigada;
    unsigned long uptime;
    bool     leituraValida;
} DadosSensor;

DadosSensor dadosRecebidos;
volatile bool dadosNovos       = false;
bool alertaEnviado             = false;
bool alertaSemSinal            = false;
unsigned long ultimoStatus     = 0;
unsigned long ultimoRecebido   = 0;

WiFiClientSecure client;
UniversalTelegramBot bot(BOT_TOKEN, client);

// Callback ESP-NOW (executa em contexto de interrupcao - manter curto)
void onDataReceived(const esp_now_recv_info_t *info, const uint8_t *data, int len) {
    if (len == sizeof(DadosSensor)) {
        memcpy(&dadosRecebidos, data, sizeof(DadosSensor));
        dadosNovos     = true;
        ultimoRecebido = millis();
    }
}

void conectarWiFi() {
    Serial.print("[WiFi] Conectando");
    WiFi.mode(WIFI_STA);
    WiFi.begin(WIFI_SSID, WIFI_PASS);
    int tentativas = 0;
    while (WiFi.status() != WL_CONNECTED && tentativas < 20) {
        delay(500);
        Serial.print(".");
        tentativas++;
    }
    if (WiFi.status() == WL_CONNECTED) {
        Serial.println("\n[WiFi] Conectado! IP: " + WiFi.localIP().toString());
        Serial.println("[WiFi] Canal: " + String(WiFi.channel()));
    } else {
        Serial.println("\n[WiFi] Falha na conexao!");
    }
}

void setup() {
    Serial.begin(115200);

    // WiFi DEVE iniciar antes do ESP-NOW para sincronizar o canal
    conectarWiFi();
    client.setInsecure();

    Serial.print("[GATEWAY] MAC: ");
    Serial.println(WiFi.macAddress());  // <- informe este MAC no config.h do sensor!

    if (esp_now_init() != ESP_OK) {
        Serial.println("[ESP-NOW] ERRO na inicializacao!");
        return;
    }
    esp_now_register_recv_cb(onDataReceived);

    ultimoRecebido = millis();

    String msg = "Gateway iniciado!\n";
    msg += "MAC: " + WiFi.macAddress() + "\n";
    msg += "Canal WiFi: " + String(WiFi.channel()) + "\n";
    msg += "Aguardando dados do sensor...";
    bot.sendMessage(CHAT_ID, msg, "");

    Serial.println("[SETUP] Gateway pronto!");
}

void loop() {
    unsigned long agora = millis();

    // Reconecta WiFi se necessario
    if (WiFi.status() != WL_CONNECTED) {
        Serial.println("[WiFi] Reconectando...");
        conectarWiFi();
    }

    // Verifica timeout do sensor (sem dados por muito tempo)
    if (agora - ultimoRecebido >= TIMEOUT_SENSOR_MS && ultimoRecebido > 0) {
        if (!alertaSemSinal) {
            bot.sendMessage(CHAT_ID,
                "ATENCAO: Sensor sem comunicacao ha mais de 2 minutos!\n"
                "Verifique o ESP32 da caixa d'agua.", "");
            alertaSemSinal = true;
        }
    }

    // Processa dados novos recebidos via ESP-NOW
    if (dadosNovos) {
        dadosNovos     = false;
        alertaSemSinal = false;

        Serial.printf("[DADOS] Dist: %.1fcm | Nivel: %d%% | Bomba: %s | Uptime: %lus\n",
            dadosRecebidos.distancia,
            dadosRecebidos.nivel,
            dadosRecebidos.bombaLigada ? "LIGADA" : "DESLIGADA",
            dadosRecebidos.uptime
        );

        if (!dadosRecebidos.leituraValida) {
            bot.sendMessage(CHAT_ID, "ATENCAO: Sensor com leitura invalida!", "");
            return;
        }

        // Nivel baixo -> alerta Telegram
        if (dadosRecebidos.nivel <= NIVEL_ALERTA && !alertaEnviado) {
            String msg = "ALERTA: Caixa d'agua baixa!\n";
            msg += "Nivel   : " + String(dadosRecebidos.nivel) + "%\n";
            msg += "Distancia: " + String(dadosRecebidos.distancia, 1) + "cm\n";
            msg += "Bomba   : LIGADA automaticamente.";
            bot.sendMessage(CHAT_ID, msg, "");
            alertaEnviado = true;
        }

        // Nivel ok -> aviso Telegram
        if (dadosRecebidos.nivel >= NIVEL_OK && alertaEnviado) {
            String msg = "Caixa d'agua abastecida!\n";
            msg += "Nivel: " + String(dadosRecebidos.nivel) + "%\n";
            msg += "Bomba: DESLIGADA.";
            bot.sendMessage(CHAT_ID, msg, "");
            alertaEnviado = false;
        }
    }

    // Status periodico
    if (agora - ultimoStatus >= INTERVALO_STATUS_MS) {
        ultimoStatus = agora;
        String msg = "--- Status do Sistema ---\n";
        msg += "Nivel  : " + String(dadosRecebidos.nivel) + "%\n";
        msg += "Bomba  : " + String(dadosRecebidos.bombaLigada ? "LIGADA" : "DESLIGADA") + "\n";
        msg += "Sensor : " + String(dadosRecebidos.uptime / 60) + " min online\n";
        msg += "WiFi   : " + String(WiFi.RSSI()) + "dBm";
        bot.sendMessage(CHAT_ID, msg, "");
    }
}
