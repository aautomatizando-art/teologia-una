#include "midi_out.h"
#include "config.h"

void midiInit() {
    if (!MIDI_ENABLED) return;
    Serial2.begin(31250, SERIAL_8N1, MIDI_RX_PIN, MIDI_TX_PIN);
}

void midiSendNote(uint8_t note, uint8_t velocity) {
    if (!MIDI_ENABLED) return;

    uint8_t status = 0x90 | ((MIDI_CHANNEL - 1) & 0x0F);   // Note On, canal configurado

    Serial2.write(status);
    Serial2.write(note & 0x7F);
    Serial2.write(velocity & 0x7F);

    // Note Off imediato: golpe de bateria e um disparo unico, nao uma nota
    // sustentada. A maioria dos modulos/plugins de bateria trata isso como
    // um "one-shot" normal.
    Serial2.write(status);
    Serial2.write(note & 0x7F);
    Serial2.write((uint8_t)0);
}
