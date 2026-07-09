#include "wav_loader.h"
#include <SD.h>
#include <string.h>
#include <stdio.h>

// Parser simplificado de WAV/RIFF (cobre o caso comum: PCM 16 bits,
// exportado por Audacity/ffmpeg/sox). Nao suporta WAV com metadados
// extras complexos (LIST, cue, etc.) alem do fmt/data basico, mas esses
// chunks sao simplesmente pulados.
bool loadWavToPSRAM(const char *path, WavSample &out) {
    File f = SD.open(path);
    if (!f) {
        Serial.printf("[SD] Arquivo nao encontrado: %s\n", path);
        return false;
    }

    char tag[4];
    uint32_t chunkSize32;

    f.read((uint8_t *)tag, 4);
    if (strncmp(tag, "RIFF", 4) != 0) {
        Serial.printf("[SD] %s nao e RIFF/WAV valido\n", path);
        f.close();
        return false;
    }
    f.read((uint8_t *)&chunkSize32, 4);   // tamanho total, ignorado
    f.read((uint8_t *)tag, 4);
    if (strncmp(tag, "WAVE", 4) != 0) {
        Serial.printf("[SD] %s nao e WAVE valido\n", path);
        f.close();
        return false;
    }

    uint16_t numChannels   = 1;
    uint16_t bitsPerSample = 16;
    bool     dataFound     = false;

    while (f.available() >= 8) {
        char chunkId[4];
        uint32_t chunkSize;
        f.read((uint8_t *)chunkId, 4);
        f.read((uint8_t *)&chunkSize, 4);

        if (strncmp(chunkId, "fmt ", 4) == 0) {
            uint16_t audioFormat;
            uint32_t sampleRate, byteRate;
            uint16_t blockAlign;
            f.read((uint8_t *)&audioFormat, 2);
            f.read((uint8_t *)&numChannels, 2);
            f.read((uint8_t *)&sampleRate, 4);
            f.read((uint8_t *)&byteRate, 4);
            f.read((uint8_t *)&blockAlign, 2);
            f.read((uint8_t *)&bitsPerSample, 2);
            long resto = (long)chunkSize - 16;
            if (resto > 0) f.seek(f.position() + resto);

            if (audioFormat != 1 || bitsPerSample != 16) {
                Serial.printf("[SD] %s: apenas PCM 16 bits e suportado (formato=%d, bits=%d)\n",
                              path, audioFormat, bitsPerSample);
                f.close();
                return false;
            }
        } else if (strncmp(chunkId, "data", 4) == 0) {
            uint32_t bytesPerFrame  = (uint32_t)numChannels * (bitsPerSample / 8);
            uint32_t numFrames      = chunkSize / bytesPerFrame;

            int16_t *buf = (int16_t *)ps_malloc((size_t)numFrames * sizeof(int16_t));
            if (!buf) {
                Serial.printf("[PSRAM] Falha ao alocar %u amostras para %s\n", numFrames, path);
                f.close();
                return false;
            }

            int16_t frame[2];
            for (uint32_t i = 0; i < numFrames; i++) {
                f.read((uint8_t *)frame, bytesPerFrame);
                if (numChannels == 1) {
                    buf[i] = frame[0];
                } else {
                    buf[i] = (int16_t)(((int32_t)frame[0] + (int32_t)frame[1]) / 2);
                }
            }

            out.data   = buf;
            out.length = numFrames;
            dataFound  = true;
            break;
        } else {
            // chunk desconhecido (ex: "LIST"), pula
            f.seek(f.position() + chunkSize + (chunkSize % 2));
        }
    }

    f.close();
    if (!dataFound) {
        Serial.printf("[SD] %s: chunk 'data' nao encontrado\n", path);
    }
    return dataFound;
}
