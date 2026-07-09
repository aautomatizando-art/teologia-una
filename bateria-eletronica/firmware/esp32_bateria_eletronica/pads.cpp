#include "pads.h"
#include "audio_engine.h"
#include "midi_out.h"
#include <math.h>

// Mesmo algoritmo de deteccao usado em modulos de bateria eletronica
// comerciais: SCAN (procura o pico real do golpe) + MASK (ignora a
// vibracao residual do piezo por um tempo, evitando disparos duplicados).
enum PadState { PAD_IDLE, PAD_SCANNING, PAD_MASKING };

struct PadRuntime {
    PadState state      = PAD_IDLE;
    uint16_t peak        = 0;
    uint32_t stateStart  = 0;
};

static PadRuntime padRT[NUM_PADS];

static HiHatZone hihatZone     = HIHAT_CLOSED;
static uint32_t  ledOffAt      = 0;

static void flashLed() {
    digitalWrite(LED_PIN, HIGH);
    ledOffAt = millis() + 4;
}

static uint8_t peakToVelocity(uint16_t peak, const PadConfig &cfg) {
    int32_t range    = (int32_t)peak - (int32_t)cfg.threshold;
    int32_t maxRange = (int32_t)cfg.peakMax - (int32_t)cfg.threshold;
    if (maxRange < 1) maxRange = 1;
    if (range < 0) range = 0;

    float norm   = constrain((float)range / (float)maxRange, 0.0f, 1.0f);
    float shaped = powf(norm, 1.0f / cfg.curve);
    uint8_t vel  = (uint8_t)(shaped * 126.0f) + 1;   // 1..127 (nunca 0 -> sempre audivel)
    return vel;
}

static void onPadHit(PadId id, uint8_t velocity) {
    flashLed();
    const PadConfig &cfg = padConfig[id];

    if (id == PAD_HIHAT) {
        bool fechado = (hihatZone != HIHAT_OPEN);
        SoundId snd  = fechado ? SND_HIHAT_CLOSED : SND_HIHAT_OPEN;
        if (fechado) audioChokeGroup(SND_HIHAT_OPEN);   // abafa o chimbal aberto tocando
        audioTriggerSound(snd, velocity);
        midiSendNote(fechado ? GM_CLOSED_HIHAT : GM_OPEN_HIHAT, velocity);
    } else {
        audioTriggerSound(padToSound(id), velocity);
        midiSendNote(cfg.midiNote, velocity);
    }
}

static void processPad(PadId id) {
    const PadConfig &cfg = padConfig[id];
    PadRuntime &rt        = padRT[id];
    uint16_t v            = analogRead(cfg.pin);
    uint32_t now          = millis();

    switch (rt.state) {
        case PAD_IDLE:
            if (v > cfg.threshold) {
                rt.state      = PAD_SCANNING;
                rt.peak       = v;
                rt.stateStart = now;
            }
            break;

        case PAD_SCANNING:
            if (v > rt.peak) rt.peak = v;
            if (now - rt.stateStart >= cfg.scanTimeMs) {
                onPadHit(id, peakToVelocity(rt.peak, cfg));
                rt.state      = PAD_MASKING;
                rt.stateStart = now;
            }
            break;

        case PAD_MASKING:
            if (now - rt.stateStart >= cfg.maskTimeMs) {
                rt.state = PAD_IDLE;
            }
            break;
    }
}

// Le a posicao continua do pedal do chimbal e dispara o som de "chick"
// (pedal batendo) quando o pedal fecha rapidamente vindo de aberto/half-open.
static void scanHiHatPedal() {
    int v = analogRead(PIN_HIHAT_PEDAL);
    HiHatZone nova;
    if (v < HIHAT_PEDAL_CLOSED_MAX)      nova = HIHAT_CLOSED;
    else if (v > HIHAT_PEDAL_OPEN_MIN)   nova = HIHAT_OPEN;
    else                                  nova = HIHAT_HALF;

    if (hihatZone != HIHAT_CLOSED && nova == HIHAT_CLOSED) {
        audioTriggerSound(SND_HIHAT_PEDAL, 90);
        audioChokeGroup(SND_HIHAT_OPEN);
        midiSendNote(GM_PEDAL_HIHAT, 90);
    }
    hihatZone = nova;
}

void padsInit() {
    analogReadResolution(12);
    analogSetAttenuation(ADC_11db);   // faixa completa 0-3.3V

    for (int i = 0; i < NUM_PADS; i++) {
        pinMode(padConfig[i].pin, INPUT);
    }
    pinMode(PIN_HIHAT_PEDAL, INPUT);
    pinMode(LED_PIN, OUTPUT);
}

void padsTask(void *param) {
    for (;;) {
        scanHiHatPedal();
        for (int i = 0; i < NUM_PADS; i++) {
            processPad((PadId)i);
        }
        if (ledOffAt && millis() >= ledOffAt) {
            digitalWrite(LED_PIN, LOW);
            ledOffAt = 0;
        }
        vTaskDelay(1 / portTICK_PERIOD_MS);   // varredura a ~1kHz
    }
}
