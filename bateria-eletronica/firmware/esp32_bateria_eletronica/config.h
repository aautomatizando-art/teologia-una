#ifndef CONFIG_H
#define CONFIG_H

#include <Arduino.h>

// ─── PADS FISICOS (piezo) ──────────────────────────────────────────────
enum PadId {
    PAD_KICK = 0,   // Bumbo
    PAD_SNARE,      // Caixa
    PAD_TOM1,       // Tom agudo
    PAD_TOM2,       // Tom medio
    PAD_TOM3,       // Surdo / Tom de piso
    PAD_CRASH,      // Prato de ataque
    PAD_RIDE,       // Prato de condução
    PAD_HIHAT,      // Chimbal (o par aberto/fechado é escolhido pelo pedal)
    NUM_PADS
};

// ─── PINOS DOS PIEZOS (entrada analogica, 0-3.3V apos o circuito de protecao) ──
#define PIN_KICK   34
#define PIN_SNARE  35
#define PIN_TOM1   32
#define PIN_TOM2   33
#define PIN_TOM3   36   // "VP"
#define PIN_CRASH  39   // "VN"
#define PIN_RIDE    4
#define PIN_HIHAT  13

// Pedal do chimbal: posicao continua (fechado / meio-aberto / aberto)
#define PIN_HIHAT_PEDAL  14

// ─── SAIDA DE AUDIO (DAC I2S, ex: PCM5102A ou amplificador MAX98357A) ──────
#define I2S_BCK_PIN   25   // bit clock
#define I2S_WS_PIN    26   // word select / LRCK
#define I2S_DOUT_PIN  27   // dados de audio

// ─── CARTAO SD (SPI, onde ficam as amostras .wav) ──────────────────────────
#define SD_SCK_PIN   18
#define SD_MISO_PIN  19
#define SD_MOSI_PIN  23
#define SD_CS_PIN     5

// ─── SAIDA MIDI (conector DIN 5 pinos, para usar com modulo/DAW externo) ───
#define MIDI_ENABLED   true
#define MIDI_TX_PIN    17
#define MIDI_RX_PIN    16   // nao usado (so MIDI OUT), exigido pelo Serial2.begin()
#define MIDI_CHANNEL   10   // canal 10 = bateria (padrao General MIDI)

// LED de status (pisca a cada golpe detectado)
#define LED_PIN 2

// ─── AUDIO ──────────────────────────────────────────────────────────────
#define AUDIO_SAMPLE_RATE   44100
#define AUDIO_MAX_VOICES    16     // polifonia: golpes simultaneos que tocam ao mesmo tempo
#define AUDIO_BUFFER_LEN    256    // amostras por bloco enviado ao I2S
#define MASTER_VOLUME       0.9f   // volume geral (0.0 - 1.0)

// Numero de camadas de velocidade por som (ex.: 1=toque leve ... 4=forte)
// Cada camada e um arquivo .wav diferente -> e o que da a dinamica realista
// (um tambor tocado forte nao soa so "mais alto", soa diferente).
#define NUM_VELOCITY_LAYERS 4

// ─── NOTAS MIDI (General MIDI - canal de bateria) ──────────────────────────
#define GM_BASS_DRUM      36
#define GM_SNARE          38
#define GM_CLOSED_HIHAT   42
#define GM_PEDAL_HIHAT    44
#define GM_OPEN_HIHAT     46
#define GM_LOW_TOM        45
#define GM_MID_TOM        47
#define GM_HIGH_TOM       50
#define GM_CRASH          49
#define GM_RIDE           51

// ─── ALGORITMO DE DETECCAO DO GOLPE (mesma logica dos modulos comerciais) ──
// threshold  : nivel minimo de ADC (0-4095) para considerar que houve uma batida
// scanTimeMs : janela de tempo em que o firmware procura o verdadeiro PICO do
//              golpe (o piezo oscila, o pico real pode vir alguns ms depois
//              do primeiro estouro do limiar)
// maskTimeMs : tempo minimo entre dois golpes no mesmo pad (evita "metralhar"
//              disparos por causa da vibracao residual do piezo)
// peakMax    : valor de ADC equivalente a velocity=127 (golpe mais forte
//              possivel) -- AJUSTE isso com o sketch teste_calibracao_piezo
// curve      : curva de resposta a velocidade (1.0=linear, >1=mais dinamica,
//              dá mais "corpo" aos golpes fracos e reserva o topo p/ fortes)
struct PadConfig {
    uint8_t     pin;
    uint16_t    threshold;
    uint8_t     scanTimeMs;
    uint16_t    maskTimeMs;
    uint16_t    peakMax;
    float       curve;
    uint8_t     midiNote;
};

static PadConfig padConfig[NUM_PADS] = {
    // pino       thr  scan mask  peakMax curve  nota MIDI
    { PIN_KICK,   250,   6,   60,  3600,  1.6f, GM_BASS_DRUM    },
    { PIN_SNARE,  200,   5,   55,  3400,  1.8f, GM_SNARE        },
    { PIN_TOM1,   200,   5,   55,  3400,  1.7f, GM_HIGH_TOM     },
    { PIN_TOM2,   200,   5,   55,  3400,  1.7f, GM_MID_TOM      },
    { PIN_TOM3,   200,   5,   55,  3400,  1.7f, GM_LOW_TOM      },
    { PIN_CRASH,  180,   8,  120,  3600,  1.5f, GM_CRASH        },
    { PIN_RIDE,   180,   8,  100,  3600,  1.5f, GM_RIDE         },
    { PIN_HIHAT,  180,   6,   70,  3400,  1.6f, GM_CLOSED_HIHAT },
};

// ─── PEDAL DO CHIMBAL ───────────────────────────────────────────────────
// Valores de ADC (0-4095) que definem cada zona do pedal. CALIBRE com o
// teste_calibracao_piezo: solte o pedal (aberto) e pressione (fechado) e
// anote os valores lidos no pino PIN_HIHAT_PEDAL.
#define HIHAT_PEDAL_CLOSED_MAX   1200   // abaixo disso = fechado
#define HIHAT_PEDAL_OPEN_MIN     2800   // acima disso  = aberto (entre = half-open)

#endif
