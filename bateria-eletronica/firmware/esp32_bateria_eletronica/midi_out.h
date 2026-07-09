#ifndef MIDI_OUT_H
#define MIDI_OUT_H

#include <Arduino.h>

// Inicializa a porta serial MIDI (31250 baud, padrao MIDI DIN).
void midiInit();

// Envia Note On seguido de Note Off imediato (golpe de bateria = disparo
// unico, sem "tecla segurada"). Permite ligar o ESP32 como controlador
// MIDI a um modulo de bateria ou DAW/VST profissional para som ainda mais
// realista, alem (ou no lugar) do audio tocado localmente pelo ESP32.
void midiSendNote(uint8_t note, uint8_t velocity);

#endif
