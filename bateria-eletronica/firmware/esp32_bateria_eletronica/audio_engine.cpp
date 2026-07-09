#include "audio_engine.h"
#include "wav_loader.h"
#include <driver/i2s.h>
#include <math.h>
#include <string.h>
#include <stdio.h>

// ─── Banco de amostras (carregado do SD para a PSRAM no boot) ─────────────
struct SoundGroup {
    const char *folder;
    WavSample   layers[NUM_VELOCITY_LAYERS];
};

// Estrutura de pastas esperada no cartao SD (veja README.md do projeto):
//   /kick/1.wav .. /kick/4.wav   (1=toque leve ... 4=golpe forte)
//   /snare/1.wav .. 4.wav
//   /tom1/1.wav .. 4.wav   /tom2/...   /tom3/...
//   /crash/1.wav .. 4.wav
//   /ride/1.wav .. 4.wav
//   /hihat/closed/1.wav .. 4.wav
//   /hihat/open/1.wav .. 4.wav
//   /hihat/pedal/1.wav            (som do pedal "chick", uma unica amostra)
static SoundGroup soundBank[NUM_SOUNDS] = {
    { "/kick" },
    { "/snare" },
    { "/tom1" },
    { "/tom2" },
    { "/tom3" },
    { "/crash" },
    { "/ride" },
    { "/hihat/closed" },
    { "/hihat/open" },
    { "/hihat/pedal" },
};

// ─── Vozes (polifonia) ──────────────────────────────────────────────────
#define RELEASE_FADE_SAMPLES 220   // ~5ms em 44.1kHz -- evita "clique" ao abafar

struct Voice {
    bool     active         = false;
    bool     releasing      = false;
    int32_t  releaseLeft    = 0;
    const int16_t *data     = nullptr;
    uint32_t length         = 0;
    uint32_t pos            = 0;
    float    gain           = 1.0f;
    SoundId  sound          = SND_KICK;
};

static Voice voices[AUDIO_MAX_VOICES];
static portMUX_TYPE voiceMux = portMUX_INITIALIZER_UNLOCKED;

SoundId padToSound(PadId id) {
    switch (id) {
        case PAD_KICK:  return SND_KICK;
        case PAD_SNARE: return SND_SNARE;
        case PAD_TOM1:  return SND_TOM1;
        case PAD_TOM2:  return SND_TOM2;
        case PAD_TOM3:  return SND_TOM3;
        case PAD_CRASH: return SND_CRASH;
        case PAD_RIDE:  return SND_RIDE;
        default:        return SND_KICK;   // PAD_HIHAT e tratado a parte (pads.cpp)
    }
}

void audioInitI2S() {
    i2s_config_t cfg = {
        .mode                 = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_TX),
        .sample_rate          = AUDIO_SAMPLE_RATE,
        .bits_per_sample      = I2S_BITS_PER_SAMPLE_16BIT,
        .channel_format       = I2S_CHANNEL_FMT_RIGHT_LEFT,
        .communication_format = I2S_COMM_FORMAT_STAND_I2S,
        .intr_alloc_flags     = ESP_INTR_FLAG_LEVEL1,
        .dma_buf_count        = 6,
        .dma_buf_len          = AUDIO_BUFFER_LEN,
        .use_apll             = true,
        .tx_desc_auto_clear   = true,
    };
    i2s_pin_config_t pins = {
        .bck_io_num   = I2S_BCK_PIN,
        .ws_io_num    = I2S_WS_PIN,
        .data_out_num = I2S_DOUT_PIN,
        .data_in_num  = I2S_PIN_NO_CHANGE,
    };
    i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL);
    i2s_set_pin(I2S_NUM_0, &pins);
}

void audioLoadSamples() {
    if (!psramFound()) {
        Serial.println("[AUDIO] ERRO: PSRAM nao encontrada! Habilite PSRAM nas");
        Serial.println("        opcoes da placa (Tools > PSRAM > Enabled) e use");
        Serial.println("        uma placa ESP32 com PSRAM (ex: ESP32-WROVER).");
    }

    for (int s = 0; s < NUM_SOUNDS; s++) {
        int camadas = (s == SND_HIHAT_PEDAL) ? 1 : NUM_VELOCITY_LAYERS;
        for (int l = 0; l < camadas; l++) {
            char path[80];
            snprintf(path, sizeof(path), "%s/%d.wav", soundBank[s].folder, l + 1);
            if (loadWavToPSRAM(path, soundBank[s].layers[l])) {
                Serial.printf("[AUDIO] Carregado: %s (%u amostras)\n", path, soundBank[s].layers[l].length);
            } else {
                Serial.printf("[AUDIO] Aviso: %s ausente (som ficara em silencio nessa camada)\n", path);
            }
        }
    }
}

// Encontra a camada de velocidade carregada mais proxima da pedida
static int8_t resolveLayer(SoundId snd, uint8_t velocity, int numLayers) {
    int layer = (velocity * numLayers) / 128;
    if (layer >= numLayers) layer = numLayers - 1;

    for (int offset = 0; offset < numLayers; offset++) {
        int tryLayer = layer - offset;
        if (tryLayer >= 0 && soundBank[snd].layers[tryLayer].data) return tryLayer;
        tryLayer = layer + offset;
        if (tryLayer < numLayers && soundBank[snd].layers[tryLayer].data) return tryLayer;
    }
    return -1;
}

static int findFreeVoice() {
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (!voices[i].active) return i;
    }
    // sem voz livre: rouba a voz mais antiga (round-robin simples)
    static int steal = 0;
    int v = steal;
    steal = (steal + 1) % AUDIO_MAX_VOICES;
    return v;
}

void audioTriggerSound(SoundId snd, uint8_t velocity) {
    int numLayers = (snd == SND_HIHAT_PEDAL) ? 1 : NUM_VELOCITY_LAYERS;
    int8_t layer = resolveLayer(snd, velocity, numLayers);
    if (layer < 0) return;   // nenhuma amostra carregada para esse som

    WavSample &s = soundBank[snd].layers[layer];

    // Curva perceptual de ganho: golpes fracos nao ficam quietos demais,
    // mas o topo da dinamica fica reservado para os golpes fortes.
    float gain = powf(velocity / 127.0f, 0.6f);

    portENTER_CRITICAL(&voiceMux);
    int v = findFreeVoice();
    voices[v].active    = true;
    voices[v].releasing = false;
    voices[v].data      = s.data;
    voices[v].length    = s.length;
    voices[v].pos       = 0;
    voices[v].gain      = gain;
    voices[v].sound     = snd;
    portEXIT_CRITICAL(&voiceMux);
}

void audioChokeGroup(SoundId snd) {
    portENTER_CRITICAL(&voiceMux);
    for (int i = 0; i < AUDIO_MAX_VOICES; i++) {
        if (voices[i].active && voices[i].sound == snd && !voices[i].releasing) {
            voices[i].releasing   = true;
            voices[i].releaseLeft = RELEASE_FADE_SAMPLES;
        }
    }
    portEXIT_CRITICAL(&voiceMux);
}

// Limitador suave: evita o "clip" duro/digital quando varios sons somam
// acima da faixa de 16 bits (ex: bumbo + caixa + prato ao mesmo tempo).
static inline int16_t softClip(int32_t x) {
    const float threshold = 28000.0f;
    const float ceiling   = 32700.0f;
    float xf = (float)x;
    float sign = xf < 0 ? -1.0f : 1.0f;
    float mag  = fabsf(xf);
    if (mag > threshold) {
        mag = threshold + (ceiling - threshold) * tanhf((mag - threshold) / (ceiling - threshold));
    }
    if (mag > 32767.0f) mag = 32767.0f;
    return (int16_t)(sign * mag);
}

void audioTask(void *param) {
    static int32_t mixBuf[AUDIO_BUFFER_LEN];
    static int16_t outBuf[AUDIO_BUFFER_LEN * 2];   // estereo intercalado

    for (;;) {
        memset(mixBuf, 0, sizeof(mixBuf));

        portENTER_CRITICAL(&voiceMux);
        for (int v = 0; v < AUDIO_MAX_VOICES; v++) {
            if (!voices[v].active) continue;
            Voice &voice = voices[v];

            for (int i = 0; i < AUDIO_BUFFER_LEN; i++) {
                if (voice.pos >= voice.length) {
                    voice.active = false;
                    break;
                }
                float g = voice.gain;
                if (voice.releasing) {
                    g *= (float)voice.releaseLeft / (float)RELEASE_FADE_SAMPLES;
                    if (--voice.releaseLeft <= 0) {
                        voice.active = false;
                    }
                }
                mixBuf[i] += (int32_t)((float)voice.data[voice.pos++] * g);
                if (!voice.active) break;
            }
        }
        portEXIT_CRITICAL(&voiceMux);

        for (int i = 0; i < AUDIO_BUFFER_LEN; i++) {
            int16_t s = softClip((int32_t)(mixBuf[i] * MASTER_VOLUME));
            outBuf[2 * i]     = s;   // canal esquerdo
            outBuf[2 * i + 1] = s;   // canal direito (mono duplicado)
        }

        size_t written;
        i2s_write(I2S_NUM_0, outBuf, sizeof(outBuf), &written, portMAX_DELAY);
    }
}
