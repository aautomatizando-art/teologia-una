/*
 * Utilitario: exibe o MAC address do ESP32 no Serial Monitor
 *
 * INSTRUCOES:
 * 1. Grave este sketch no ESP32 #2 (Gateway)
 * 2. Abra o Serial Monitor (115200 baud)
 * 3. Copie o MAC exibido
 * 4. Cole em GATEWAY_MAC no config.h do esp32_sensor
 *
 * Exemplo de saida:
 *   MAC: AA:BB:CC:DD:EE:FF
 *   Para o config.h: {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
 */

#include <WiFi.h>

void setup() {
    Serial.begin(115200);
    WiFi.mode(WIFI_STA);
    delay(1000);

    String mac = WiFi.macAddress();
    Serial.println("\n===================================");
    Serial.println("  MAC Address do ESP32 Gateway");
    Serial.println("===================================");
    Serial.println("MAC: " + mac);

    // Formata automaticamente para o config.h
    Serial.print("Para o config.h: {");
    for (int i = 0; i < 6; i++) {
        Serial.print("0x");
        String byte_str = mac.substring(i * 3, i * 3 + 2);
        byte_str.toUpperCase();
        Serial.print(byte_str);
        if (i < 5) Serial.print(", ");
    }
    Serial.println("}");
    Serial.println("===================================");
}

void loop() {
    // nada
}
