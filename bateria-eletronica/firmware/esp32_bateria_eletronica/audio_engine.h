#ifndef AUDIO_ENGINE_H
#define AUDIO_ENGINE_H

#include <Arduino.h>
#include "config.h"

// Sons disponiveis (cada um com ate NUM_VELOCITY_LAYERS amostras .wav)
enum SoundId {
    SND_KICK = 0,
    SND_SNARE,
    SND_TOM1,
    SND_TOM2,
    SND_TOM3,
    SND_CRASH,
    SND_RIDE,
    SND_HIHAT_CLOSED,
    SND_HIHAT_OPEN,
    SND_HIHAT_PEDAL,   // "chick" do pedal (usa so a camada 0)
    NUM_SOUNDS
};

void audioInitI2S();
void audioLoadSamples();
void audioTask(void *param);

// Dispara um som com a velocidade do golpe (1-127). Escolhe automaticamente
// a camada de velocidade mais proxima disponivel.
void audioTriggerSound(SoundId snd, uint8_t velocity);

// Interrompe (com fade rapido, sem "clique") todas as vozes tocando o som
// indicado -- usado para o abafamento do chimbal aberto quando o pedal fecha.
void audioChokeGroup(SoundId snd);

SoundId padToSound(PadId id);

#endif
