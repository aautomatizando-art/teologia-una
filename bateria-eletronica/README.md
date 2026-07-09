# Bateria Eletronica Profissional - ESP32

Bateria eletronica completa baseada em ESP32: pads piezo com deteccao de
velocidade (mesmo algoritmo dos modulos comerciais), motor de audio
polifonico que toca **amostras reais de bateria acustica** (multi-camadas
de velocidade, para dinamica realista) via I2S, chimbal com pedal
continuo (fechado / half-open / aberto + "chick"), e saida **MIDI DIN**
para tocar via modulo externo ou DAW/VST profissional (Superior
Drummer, EZdrummer, BFD, Addictive Drums, etc.) quando quiser o maximo
de realismo possivel.

## Por que soa "profissional" e nao um bipe de brinquedo

1. **Amostras reais, nao sintese** — o ESP32 toca `.wav` de uma bateria
   acustica de verdade (gravada ou de um pacote de amostras), nao um tom
   sintetizado.
2. **Multi-camadas de velocidade** — cada instrumento tem 4 arquivos
   diferentes (toque leve → forte). Um tambor tocado forte **soa
   diferente**, nao so "mais alto" — é assim que módulos reais (Roland
   TD, Alesis) funcionam.
3. **Algoritmo de deteccao com scan + mask** — a mesma logica usada em
   módulos comerciais: o firmware procura o verdadeiro pico do golpe
   (o piezo oscila por alguns ms) e depois ignora a vibracao residual
   por um tempo, evitando disparos fantasmas/duplicados.
4. **Polifonia total** — ate 16 sons tocando ao mesmo tempo (bumbo +
   caixa + prato + chimbal simultaneos, sem cortar um ao outro), com
   mixagem em software e limitador suave (sem "clip" digital feio).
5. **Chimbal de verdade** — pedal analogico com 3 zonas (fechado /
   half-open / aberto) que troca a amostra tocada, mais o som do pedal
   ("chick") e abafamento (choke) automatico do chimbal aberto quando
   o pedal fecha — igual a um chimbal acustico.
6. **Saida MIDI** — se quiser o som de um plugin de bateria profissional
   (VST) em vez do audio local do ESP32, ligue a saida MIDI DIN a uma
   interface MIDI-USB e use qualquer DAW.

---

## Lista de materiais (BOM)

| Item | Especificacao | Qtde |
|---|---|---|
| ESP32 **com PSRAM** | ESP32-WROVER (recomendado) ou DevKit com PSRAM | 1 |
| Pads piezo | disco piezo de 27-35mm (ou pads prontos de bateria eletronica) | 8 |
| Pedal de chimbal | potenciometro linear 10k (ou pedal de hihat com sensor) | 1 |
| DAC de audio | módulo **PCM5102A** (I2S, saida de linha) *ou* amplificador **MAX98357A** (I2S, direto num alto-falante) | 1 |
| Modulo microSD | leitor SPI (o comum de 6 pinos) | 1 |
| Cartao microSD | 4-16GB, formatado FAT32, classe 10 | 1 |
| Conector DIN 5 pinos | para saida MIDI (opcional, mas recomendado) | 1 |
| Resistores | 1MΩ (bleed dos piezos), 220Ω x2 (MIDI), 1kΩ (opcional protecao) | ver esquema |
| Diodos 1N4148 | protecao dos piezos (clamp) | 16 (2 por pad) |
| Alto-falante ou saida de linha | caixa de som ativa, fone ou mesa de som | 1 |
| Fonte 5V | alimentacao do ESP32 + amplificador | 1 |

---

## Pinagem (config.h)

| Sinal | GPIO | Observacao |
|---|---|---|
| Kick (bumbo) | 34 | ADC1, entrada apenas |
| Snare (caixa) | 35 | ADC1, entrada apenas |
| Tom 1 (agudo) | 32 | ADC1 |
| Tom 2 (medio) | 33 | ADC1 |
| Tom 3 / Surdo | 36 (VP) | ADC1, entrada apenas |
| Crash | 39 (VN) | ADC1, entrada apenas |
| Ride | 4 | ADC2 |
| Hi-Hat (pad) | 13 | ADC2 |
| Pedal do Hi-Hat (CV) | 14 | ADC2, posicao continua |
| I2S BCK | 25 | Bit clock do DAC |
| I2S WS (LRCK) | 26 | Word select do DAC |
| I2S DOUT | 27 | Dados de audio |
| SD SCK | 18 | SPI |
| SD MISO | 19 | SPI |
| SD MOSI | 23 | SPI |
| SD CS | 5 | SPI |
| MIDI TX | 17 | Serial2, saida DIN |
| LED status | 2 | Pisca a cada golpe |

Nenhum pino de boot/strapping (0, 2, 12, 15) e usado para entradas
criticas — evita problemas ao ligar o ESP32.

---

## Diagrama de ligacao

```
                         +-------------------------------+
  8x PIEZO PADS -------> |                                |
  (com circuito de       |            ESP32               | ---I2S---> PCM5102A ---> Caixa de som
   protecao, ver abaixo) |         (com PSRAM)             |                          / amplificador
                         |                                 |
  Pedal Hi-Hat (10k) --> |  ADC1/ADC2   I2S   SPI   UART2  | ---SPI---> Cartao microSD
                         |                                 |            (amostras .wav)
                         +-------------------------------+
                                          |
                                        UART2
                                          v
                                  Conector DIN MIDI
                                  (modulo externo / DAW)
```

### Circuito de protecao de cada piezo (repita para os 8 pads)

Um piezo sozinho gera picos de varias dezenas de volts e tensao
**negativa** — ligar direto no ESP32 pode danificar a entrada ADC.
Use este circuito simples em cada pad:

```
 Piezo (+) ----+-------------------+------[1N4148]---- 3.3V   (grampeia o topo)
               |                   |
               |                 [1MΩ]   (sangra a carga, define zero estavel)
               |                   |
               +-------------------+------[1N4148]---- GND    (grampeia negativo)
               |
               +------------------------> GPIO do ESP32 (entrada analogica)
 Piezo (-) --------------------------------------------- GND
```

Isso mantem o sinal sempre entre 0V e 3.3V, protegendo o ADC e dando
leituras consistentes. Pads comerciais de bateria eletronica ja trazem
esse circuito embutido (ligue-os direto).

### Pedal do chimbal

Um potenciometro linear 10k com o cursor no GPIO 14, uma ponta em 3.3V
e a outra em GND funciona bem como controlador continuo (fechado →
aberto). Pedais de hi-hat dedicados (com sensor de posicao) tambem
funcionam do mesmo jeito.

### DAC/amplificador I2S (PCM5102A)

| PCM5102A | ESP32 |
|---|---|
| VCC | 3.3V |
| GND | GND |
| BCK | GPIO 25 |
| LCK (WS) | GPIO 26 |
| DIN | GPIO 27 |
| SCK | GND (usa o clock interno do modulo) |
| FLT | GND |
| DEMP | GND |
| XSMT | 3.3V |

### Saida MIDI DIN (opcional)

```
ESP32 GPIO17 (TX) ---[220Ω]--- Pino 5 do DIN
3.3V ---------------[220Ω]--- Pino 4 do DIN
GND ------------------------- Pino 2 do DIN
```

---

## Estrutura de amostras no cartao SD

O firmware espera esta estrutura de pastas na **raiz** do cartao SD
(cada som tem 4 arquivos = 4 camadas de velocidade, exceto o pedal):

```
/kick/1.wav   /kick/2.wav   /kick/3.wav   /kick/4.wav
/snare/1.wav  /snare/2.wav  /snare/3.wav  /snare/4.wav
/tom1/1.wav   /tom1/2.wav   /tom1/3.wav   /tom1/4.wav
/tom2/1.wav   ...
/tom3/1.wav   ...
/crash/1.wav  ...
/ride/1.wav   ...
/hihat/closed/1.wav .. 4.wav
/hihat/open/1.wav   .. 4.wav
/hihat/pedal/1.wav               (som do "chick" do pedal, so 1 arquivo)
```

`1.wav` = toque mais leve, `4.wav` = golpe mais forte. Se faltar algum
arquivo, o firmware usa a camada mais proxima disponivel (nao trava).

### Formato exigido dos arquivos

- **WAV PCM 16 bits**, mono ou estereo (estereo e convertido para mono
  automaticamente), qualquer taxa de amostragem (recomendado 44100Hz).
- Prefira arquivos curtos (0.3-2s), sem silencio no inicio, para o
  golpe soar imediato ao bater no pad.

### De onde tirar amostras realistas

Para um som **profissional de verdade**, use amostras de uma bateria
acustica real:

1. **Grave sua propria bateria** (ou peça emprestada) com um microfone
   razoavel, um golpe por vez, em varias intensidades por peca.
2. **Pacotes de amostras gratuitos/CC0** de bateria acustica multi-velocity
   feitos para módulos de bateria eletronica (procure por "acoustic drum
   samples CC0" ou "free multisampled drum kit WAV" em bancos como
   freesound.org — confira sempre a licenca de cada pacote antes de usar).
3. **Extrair de um plugin VST** que voce possua a licenca (ex: renderizar
   cada nota do Superior Drummer/EZdrummer em varias velocidades e
   exportar como WAV) — so para uso pessoal, respeitando a licenca do
   plugin.

### Convertendo/preparando os arquivos (ffmpeg)

```bash
# Converter para PCM 16 bits, 44100Hz, mono:
ffmpeg -i golpe_forte.wav -ar 44100 -ac 1 -sample_fmt s16 kick/4.wav

# Normalizar o volume (deixa as 4 camadas com nivel relativo coerente):
ffmpeg -i entrada.wav -filter:a loudnorm -ar 44100 -ac 1 -sample_fmt s16 saida.wav

# Cortar silencio do inicio (resposta instantanea ao bater no pad):
ffmpeg -i entrada.wav -af silenceremove=start_periods=1:start_threshold=-40dB -ar 44100 -ac 1 -sample_fmt s16 saida.wav
```

---

## Como compilar e gravar

1. Instale a placa ESP32 na Arduino IDE (veja `libraries.txt`).
2. Selecione uma placa **com PSRAM** (ex: "ESP32 Wrover Module") e
   habilite `Tools > PSRAM > Enabled`.
3. Abra `firmware/esp32_bateria_eletronica/esp32_bateria_eletronica.ino`.
4. Grave o cartao SD com a estrutura de amostras acima e insira no modulo.
5. Compile e grave no ESP32.
6. Abra o Serial Monitor (115200 baud) para ver o carregamento das
   amostras e confirmar que tudo foi encontrado.

## Calibrando os pads

Antes de tocar "pra valer", grave `firmware/teste_calibracao_piezo/`
**separadamente** (sketch independente, sem dependencia de SD/PSRAM):

1. Bata em cada pad — leve, medio e forte — e anote os valores de
   **pico** mostrados no Serial Monitor.
2. No `config.h` do firmware principal, ajuste por pad:
   - `threshold`: um pouco abaixo do menor pico de um toque leve real
     (evita nao detectar toques fracos, mas acima do ruido de repouso).
   - `peakMax`: proximo do pico do golpe mais forte que voce vai dar
     (define o que conta como velocity 127).
3. Para o pedal do chimbal, solte (aberto) e pressione (fechado)
   olhando o valor impresso, e ajuste `HIHAT_PEDAL_CLOSED_MAX` /
   `HIHAT_PEDAL_OPEN_MIN` no `config.h`.
4. Grave o firmware principal novamente com os valores calibrados.

## Usando com um modulo/DAW externo via MIDI (som ainda mais realista)

A saida MIDI (DIN, `GPIO17`) envia Note On/Off com a velocidade real de
cada golpe, no canal 10 (padrao de bateria em General MIDI), com o
mapeamento de notas GM (bumbo=36, caixa=38, chimbal fechado=42, chimbal
aberto=46, pedal do chimbal=44, toms=45/47/50, crash=49, ride=51). Ligue
a saida DIN a uma interface MIDI-USB e use o ESP32 como **controlador**
de qualquer software de bateria (Superior Drummer, EZdrummer, BFD,
Addictive Drums, etc.) — o audio local do ESP32 continua funcionando ao
mesmo tempo, entao voce pode usar so o MIDI, so o audio local, ou os
dois juntos.

---

## Personalizando (`config.h`)

- `padConfig[]`: pino, sensibilidade (`threshold`), janela de
  deteccao do pico (`scanTimeMs`), tempo minimo entre golpes
  (`maskTimeMs`), pico maximo esperado (`peakMax`) e curva de
  dinamica (`curve`) — tudo ajustavel por pad.
- `NUM_VELOCITY_LAYERS`: numero de camadas de velocidade por som
  (padrao 4; pode reduzir para 2 se quiser economizar amostras/PSRAM).
- `AUDIO_MAX_VOICES`: polifonia (padrao 16 vozes simultaneas).
- `MASTER_VOLUME`: volume geral (0.0 a 1.0).
- `MIDI_ENABLED` / `MIDI_CHANNEL`: liga/desliga e muda o canal MIDI.

## Extensoes possiveis

- **Pads de duas zonas** (aro/pele na caixa) — usar um segundo piezo
  por pad em um pino ADC extra e comparar os dois sinais.
- **Display OLED + encoder rotativo** — para trocar de "kit" de sons
  (varias pastas no SD) e ajustar volume sem recompilar.
- **Potenciometro de volume fisico** — ler um ADC extra e aplicar em
  `MASTER_VOLUME` em tempo real.
- **USB-MIDI nativo** — trocando para uma placa ESP32-S3 (USB OTG
  nativo) e possivel expor a bateria como um dispositivo USB-MIDI
  classe padrao, sem precisar de interface MIDI-USB externa.

## Solucao de problemas

| Sintoma | Causa provavel |
|---|---|
| `[SD] ERRO: cartao nao encontrado` | Fiacao SPI errada, cartao nao formatado em FAT32, ou modulo SD com problema de alimentacao (use 3.3V, nao 5V, na maioria dos modulos) |
| `[AUDIO] ERRO: PSRAM nao encontrada` | Placa sem PSRAM, ou `PSRAM: Enabled` nao selecionado nas opcoes da Arduino IDE |
| Pad dispara sozinho / dobrado | `threshold` muito baixo ou `maskTimeMs` muito curto para aquele pad — recalibre |
| Pad nao dispara com toque leve | `threshold` muito alto — reduza e recalibre com `teste_calibracao_piezo` |
| Chimbal aberto nao aparece | Verifique a calibracao do pedal (`HIHAT_PEDAL_OPEN_MIN`) e se `/hihat/open/*.wav` existe no SD |
| Som cortado/estalando ao tocar rapido | Aumente `AUDIO_MAX_VOICES` (ate um limite razoavel de PSRAM/CPU) |
