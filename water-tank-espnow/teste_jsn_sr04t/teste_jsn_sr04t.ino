/*
 * Teste isolado do sensor JSN-SR04T
 * Funcao: imprime a cada 1s a duracao do pulso (pulseIn) e a distancia
 *         calculada, sem WiFi/ESP-NOW. Use para testar o sensor em
 *         qualquer ESP32, mudando so os pinos abaixo se precisar.
 *
 * Ligacao:
 *   JSN-SR04T VCC  -> 5V (VIN)
 *   JSN-SR04T GND  -> GND
 *   JSN-SR04T TRIG -> GPIO 32
 *   JSN-SR04T ECHO -> GPIO 33 (use divisor de tensao 1k/2k para 3.3V)
 */

#define TRIG_PIN 32
#define ECHO_PIN 33

void setup() {
    Serial.begin(115200);
    pinMode(TRIG_PIN, OUTPUT);
    pinMode(ECHO_PIN, INPUT);
    digitalWrite(TRIG_PIN, LOW);
    delay(500);
    Serial.println("[TESTE] Pronto. TRIG=GPIO32, ECHO=GPIO33");
}

void loop() {
    digitalWrite(TRIG_PIN, LOW);
    delayMicroseconds(2);
    digitalWrite(TRIG_PIN, HIGH);
    delayMicroseconds(10);
    digitalWrite(TRIG_PIN, LOW);

    long dur = pulseIn(ECHO_PIN, HIGH, 30000);

    if (dur > 0) {
        float dist = dur * 0.034f / 2.0f;
        Serial.printf("[TESTE] pulso=%ldus -> distancia=%.1fcm\n", dur, dist);
    } else {
        Serial.println("[TESTE] pulso=0 (sem eco / timeout)");
    }

    delay(1000);
}
