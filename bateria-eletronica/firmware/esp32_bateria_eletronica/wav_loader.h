#ifndef WAV_LOADER_H
#define WAV_LOADER_H

#include <Arduino.h>

// Amostra de audio (mono, 16 bits) carregada inteira na PSRAM
struct WavSample {
    int16_t  *data   = nullptr;
    uint32_t  length = 0;   // numero de amostras (frames mono)
};

// Le um arquivo .wav (PCM 16 bits, mono ou estereo) do cartao SD e carrega
// na PSRAM. Se o arquivo for estereo, faz downmix para mono (media dos canais).
bool loadWavToPSRAM(const char *path, WavSample &out);

#endif
