/*
 * Leitor de Bits com Display LCD I2C 16x2
 *
 * Protocolo: pino DATA + pino CLOCK
 *   - A cada borda de subida do CLOCK, um bit é lido do DATA
 *   - Após 8 bits (MSB primeiro), o byte é exibido em decimal no LCD
 *
 * Ligações:
 *   LCD I2C  -> Arduino
 *   VCC      -> 5V
 *   GND      -> GND
 *   SDA      -> A4  (Uno/Nano) | 20 (Mega)
 *   SCL      -> A5  (Uno/Nano) | 21 (Mega)
 *
 *   Fonte de bits -> Arduino
 *   DATA     -> Pino 4
 *   CLOCK    -> Pino 2  (INT0 – suporta interrupção)
 *   GND      -> GND
 */

#include <Wire.h>
#include <LiquidCrystal_I2C.h>

#define CLOCK_PIN  2   // INT0
#define DATA_PIN   4

// Endereço I2C mais comum: 0x27. Se não funcionar, tente 0x3F.
LiquidCrystal_I2C lcd(0x27, 16, 2);

volatile uint8_t buffer     = 0;   // byte sendo montado
volatile uint8_t bitCount   = 0;   // bits já recebidos neste byte
volatile bool    novoValor  = false;
volatile uint8_t valorFinal = 0;

// ISR: chamada a cada borda de subida do CLOCK
void IRAM_ATTR onClock() {
    uint8_t bit = digitalRead(DATA_PIN);

    // MSB primeiro: desloca para a esquerda e insere o novo bit
    buffer = (buffer << 1) | (bit & 0x01);
    bitCount++;

    if (bitCount == 8) {
        valorFinal = buffer;
        novoValor  = true;
        buffer     = 0;
        bitCount   = 0;
    }
}

void setup() {
    pinMode(CLOCK_PIN, INPUT);
    pinMode(DATA_PIN,  INPUT);

    attachInterrupt(digitalPinToInterrupt(CLOCK_PIN), onClock, RISING);

    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Leitor de Bits");
    lcd.setCursor(0, 1);
    lcd.print("Aguardando...");
}

void loop() {
    if (novoValor) {
        novoValor = false;

        uint8_t val = valorFinal;  // cópia local (variável volátil lida uma vez)

        lcd.clear();

        // Linha 0: valor decimal
        lcd.setCursor(0, 0);
        lcd.print("Dec: ");
        lcd.print(val);

        // Linha 1: valor binário formatado (8 dígitos)
        lcd.setCursor(0, 1);
        lcd.print("Bin: ");
        for (int i = 7; i >= 0; i--) {
            lcd.print((val >> i) & 0x01);
        }
    }
}
