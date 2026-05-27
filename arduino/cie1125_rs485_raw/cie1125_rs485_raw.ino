/*
 * CIE1125 RS485 Repetidora — Captura RAW
 *
 * Ligação:
 *   CIE D+ → MAX485 A (borne verde)
 *   CIE D- → MAX485 B (borne verde)
 *   MAX485 RO    → ESP32 GPIO16
 *   MAX485 RE+DE → GND  (recepção permanente)
 *   MAX485 VCC   → 3.3V
 *   MAX485 GND   → GND
 *
 * Testar baud rates se não aparecer dados:
 *   9600 → 19200 → 4800 → 38400 → 115200
 */

#define PIN_RX      16
#define PIN_TX      17
#define BAUD_RS485  9600   // ← mude aqui se não aparecer dados

// Pausa > GAP_MS entre bytes = separador de frame (nova linha)
#define GAP_MS  50

unsigned long tUltimoByte = 0;
int           bytesNaLinha = 0;

void setup() {
    Serial.begin(115200);
    Serial2.begin(BAUD_RS485, SERIAL_8N1, PIN_RX, PIN_TX);
    Serial.println("\n=== CIE1125 RS485 RAW CAPTURE ===");
    Serial.printf("Baud: %d  RX=GPIO%d\n", BAUD_RS485, PIN_RX);
    Serial.println("Acione uma botoeira e observe os bytes que mudam.\n");
}

void loop() {
    if (Serial2.available()) {
        uint8_t b = Serial2.read();
        unsigned long agora = millis();

        // Pausa longa = novo frame
        if (bytesNaLinha > 0 && agora - tUltimoByte > GAP_MS) {
            Serial.println();
            bytesNaLinha = 0;
        }

        Serial.printf("%02X ", b);
        bytesNaLinha++;

        // Quebra a cada 16 bytes para facilitar leitura
        if (bytesNaLinha >= 16) {
            Serial.println();
            bytesNaLinha = 0;
        }

        tUltimoByte = agora;
    }
}
