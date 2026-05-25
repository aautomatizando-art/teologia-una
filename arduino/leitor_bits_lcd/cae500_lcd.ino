/*
 * Leitor de Eventos – Ilumac CAE-500 XMAX → LCD I2C 16x2
 *
 * A central envia eventos em texto ASCII pela porta RS232 (DB9).
 * Um módulo MAX232 converte os níveis ±12V para TTL 5V do Arduino.
 *
 * Ligações:
 *   CAE-500 DB9 pino 2 (TX) → MAX232 → Arduino pino 4 (RX software)
 *   CAE-500 DB9 pino 5 (GND) → GND Arduino
 *
 *   LCD I2C → Arduino
 *   SDA → A4  |  SCL → A5  |  VCC → 5V  |  GND → GND
 *
 * Parâmetros seriais da CAE-500 XMAX:
 *   9600 bps, 8 bits, sem paridade, 1 stop bit (8N1)
 *   (Se não receber dados, tente 4800 ou 19200 – verificar no menu da central)
 *
 * Formato típico de mensagem da central:
 *   "DD/MM/AAAA HH:MM:SS  ALARME  END:042  BOTOEIRA PAV 3\r\n"
 *   "DD/MM/AAAA HH:MM:SS  FALHA   END:007  DETECTOR SALA 1\r\n"
 *   "DD/MM/AAAA HH:MM:SS  NORMAL  END:042  BOTOEIRA PAV 3\r\n"
 */

#include <SoftwareSerial.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// Pino 4 = RX (recebe da central via MAX232)
// Pino 5 = TX (não usado, mas obrigatório no SoftwareSerial)
SoftwareSerial centralSerial(4, 5);

// Endereço I2C do LCD: 0x27 (tente 0x3F se não funcionar)
LiquidCrystal_I2C lcd(0x27, 16, 2);

String bufferLinha = "";

// Extrai o número após "END:" na linha recebida
// Retorna -1 se não encontrar
int extrairEndereco(const String& linha) {
    int idx = linha.indexOf("END:");
    if (idx == -1) return -1;

    String numStr = "";
    idx += 4;  // avança para depois de "END:"
    while (idx < (int)linha.length() && isDigit(linha[idx])) {
        numStr += linha[idx];
        idx++;
    }
    return numStr.length() > 0 ? numStr.toInt() : -1;
}

// Extrai o tipo de evento (ALARME, FALHA, NORMAL, etc.)
String extrairTipo(const String& linha) {
    if (linha.indexOf("ALARME")  != -1) return "ALARME ";
    if (linha.indexOf("FALHA")   != -1) return "FALHA  ";
    if (linha.indexOf("NORMAL")  != -1) return "NORMAL ";
    if (linha.indexOf("SUPERV")  != -1) return "SUPERV.";
    return "EVENTO ";
}

void exibirNoLCD(const String& tipo, int endereco, const String& linha) {
    lcd.clear();

    // Linha 0: tipo do evento + endereço decimal
    lcd.setCursor(0, 0);
    lcd.print(tipo);
    lcd.print(" End:");
    lcd.print(endereco);

    // Linha 1: descrição (caracteres após o endereço, até 16 chars)
    int idxDesc = linha.indexOf("END:");
    if (idxDesc != -1) {
        // Avança além de "END:NNN  "
        int i = idxDesc + 4;
        while (i < (int)linha.length() && (isDigit(linha[i]) || linha[i] == ' ')) i++;

        String desc = linha.substring(i);
        desc.trim();
        if (desc.length() > 16) desc = desc.substring(0, 16);

        lcd.setCursor(0, 1);
        lcd.print(desc);
    }
}

void setup() {
    Serial.begin(115200);           // monitor serial para debug
    centralSerial.begin(9600);      // comunicação com a CAE-500 XMAX

    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("CAE-500 XMAX");
    lcd.setCursor(0, 1);
    lcd.print("Aguardando...");

    Serial.println("Sistema iniciado. Aguardando eventos da CAE-500 XMAX...");
}

void loop() {
    while (centralSerial.available()) {
        char c = centralSerial.read();

        if (c == '\n' || c == '\r') {
            bufferLinha.trim();

            if (bufferLinha.length() > 10) {
                Serial.println(">> " + bufferLinha);   // debug no monitor serial

                int endereco = extrairEndereco(bufferLinha);
                if (endereco >= 0) {
                    String tipo = extrairTipo(bufferLinha);
                    exibirNoLCD(tipo, endereco, bufferLinha);

                    Serial.print("   Tipo: ");
                    Serial.print(tipo);
                    Serial.print(" | Endereço: ");
                    Serial.println(endereco);
                }
            }

            bufferLinha = "";
        } else {
            if (bufferLinha.length() < 128) {   // limite de segurança
                bufferLinha += c;
            }
        }
    }
}
