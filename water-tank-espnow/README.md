# Gestao Condominio - ESP32 + WiFi/ESP-NOW + WhatsApp + Dashboard (Supabase/Vercel)

Conjunto de firmwares ESP32 para monitorar a caixa d'agua (tanque superior e
inferior), o alarme de incendio (com foto), as bombas de incendio, as cancelas
da portaria e a agua da rua de um condominio. Cada modulo envia alertas para um
**grupo do WhatsApp** via **Evolution API** (VPS/Docker) e publica os dados no
**Supabase**, que alimenta a **dashboard web** (Vercel) em tempo real.

A dashboard fica em [`../water-tank-dashboard`](../water-tank-dashboard/).

## Visao geral dos nos

| # | Modulo                     | Placa            | Pasta                          | Tabela Supabase     | AP de configuracao (WiFiManager) |
|---|----------------------------|------------------|---------------------------------|----------------------|-------------------------------------|
| 1 | Tanque Superior - Sensor    | ESP32 DevKit     | `esp32_sensor/`                | `leituras` (via gateway) | -- (sem WiFi, fala via ESP-NOW) |
| 2 | Gateway (Tanque Superior + Tanque Inferior) | ESP32 DevKit | `esp32_gateway/` | `leituras` e `tanque_inferior` | `CaixaDagua-Setup` |
| 3 | Alarme de Incendio (com foto) | ESP32-CAM (AI-Thinker) | `esp32cam_alarme_incendio/` | `alarme_incendio`     | `AlarmeIncendio-Setup`             |
| 4 | Bombas de Incendio           | ESP32 DevKit     | `esp32_bombas_incendio/`        | `bombas_incendio`     | `BombaIncendio-Setup`              |
| 5 | Cancela da Portaria          | ESP32 DevKit     | `esp32_cancela_portaria/`       | `cancela_portaria`    | `CancelaPortaria-Setup`            |
| 6 | Agua da Rua                  | ESP32 DevKit     | `esp32_agua_rua/`                | `agua_rua`            | `AguaRua-Setup`                     |

Os modulos 2 a 6 sao conectados direto na rede WiFi do condominio (via
WiFiManager), enviam o status para o Supabase e disparam alertas no WhatsApp.
O **modulo 1** (Sensor do Tanque Superior, sem WiFi) fala via **ESP-NOW** com
o **modulo 2** (Gateway). Alem de receber o Tanque Superior via ESP-NOW, o
Gateway le diretamente pelos proprios GPIOs os sensores do **Tanque Inferior**
(nivel, 4 entradas da bomba, temperatura e vibracao) — um unico ESP32 cobre
os dois tanques.

Em todos os `config.h`, defina o **mesmo** `CONDOMINIO_NOME` para identificar
os nos do mesmo condominio na dashboard e nas mensagens do WhatsApp (veja
[Varios Condominios](#varios-condominios)).

---

## 1-2. Tanque Superior (ESP-NOW) + Tanque Inferior (no Gateway)

```
[Caixa d'agua - sem WiFi]        [100m - com WiFi]              [VPS Hostinger]

 ESP32 #1 (Sensor)                ESP32 #2 (Gateway)             Evolution API
 +--------------------+           +------------------+           +--------------+
 | JSN-SR04T          |  ESP-NOW  | WiFi + ESP-NOW   |   HTTP    | Docker :8080 |
 | (nivel da caixa)   | --------> | HTTPClient       | --------> | evolution_api|---> Grupo
 |                    |           | JSON p/ Evo API  |           | postgres     |     WhatsApp
 |                    |           |                  |           | redis        |
 |                    |           | HTTPS / JSON     |           +--------------+
 |                    |           | -------------------------> Supabase (REST) -> Dashboard
 |                    |           |                                              (Vercel)
 +--------------------+           +--------+---------+
                                            |
                                            | leitura local (GPIOs livres)
                                            v
                                   +------------------+
                                   | Tanque Inferior  |
                                   | JSN-SR04T (nivel)|
                                   | 4 entradas bomba |
                                   | DS18B20 (temp)   |
                                   | Vibracao (ADC)   |
                                   +------------------+
```

> O ESP32 #1 (Sensor) tem **somente** o sensor ultrassonico JSN-SR04T do
> Tanque Superior.
>
> O ESP32 #2 (Gateway) conecta na rede WiFi do condominio (via WiFiManager),
> recebe os dados do Sensor via ESP-NOW (o radio WiFi atende os dois ao mesmo
> tempo) e ainda le diretamente (pelos GPIOs livres) os sensores e entradas
> do **Tanque Inferior** (veja a subsecao abaixo) — um unico ESP32 cobre os
> dois tanques.

### Alertas no Grupo do WhatsApp (Tanque Superior)

| Evento                                       | Mensagem                                  |
|-----------------------------------------------|--------------------------------------------|
| Nivel <= `NIVEL_ALERTA`                        | "Nivel baixo na caixa d'agua!" + nivel/dist |
| Caixa abastecida (>= `NIVEL_OK`)               | "Caixa d'agua abastecida!"                |
| Sensor sem comunicacao (2 min)                 | "Sensor sem comunicacao!"                 |
| Sensor voltou a comunicar                      | "Sensor online novamente!"                |
| Leitura invalida do sensor                     | "Sensor com leitura invalida!"            |
| Gateway ligado/reiniciado                      | "Gateway reiniciado!" + nivel atual do Tanque Superior e do Tanque Inferior |
| WiFi do gateway caiu e reconectou sozinho      | "WiFi do gateway caiu e reconectou sozinho!" + tempo offline |

A cada leitura, o gateway tambem envia os dados (nivel e distancia) para o
Supabase (tabela `leituras`), que alimenta a dashboard em tempo real.

### Esquema de Ligacao (ESP32 #1 - Sensor)

```
ESP32 #1       JSN-SR04T
  GPIO32  ----> TRIG
  GPIO33  <---- ECHO
  5V      ----> VCC
  GND     ----> GND
```

### Calibracao do Sensor (Tanque Superior)

```
[JSN-SR04T na tampa da caixa]
   |  <- 15cm  DIST_CAIXA_CHEIA  -> caixa cheia (100%)
   |
   ~  nivel da agua
   |
   |  <- 90cm  DIST_CAIXA_VAZIA  -> caixa vazia (0%)
   |
[Fundo da caixa]
```

Ajuste `DIST_CAIXA_VAZIA` e `DIST_CAIXA_CHEIA` em `esp32_sensor/config.h`
conforme a altura real da sua caixa.

### Dicas de Alcance ESP-NOW (100m)

- Em campo aberto: 200-400m — seus 100m estao dentro do alcance
- Posicione o gateway em janela/area aberta voltada para a caixa
- Evite obstaculos metalicos na linha de visada
- Se precisar de mais alcance: ESP32 com antena externa (conector U.FL)

---

### Tanque Inferior (sensores ligados no proprio Gateway)

Os sensores e entradas do tanque inferior (reservatorio/cisterna proximo a
bomba) sao ligados diretamente nos GPIOs livres do **Gateway** — nao ha mais
um ESP32 dedicado para isso. O Gateway le esses sensores periodicamente (a
cada `INTERVALO_MEDICAO_MS`), publica no Supabase (tabela `tanque_inferior`)
e dispara os alertas no WhatsApp, alem de cuidar do Tanque Superior via
ESP-NOW.

### Sensores e Entradas

| Sinal                          | GPIO | Funcao                                     |
|---------------------------------|------|----------------------------------------------|
| JSN-SR04T TRIG                 | 32   | Disparo do sensor ultrassonico               |
| JSN-SR04T ECHO                 | 33   | Eco do sensor ultrassonico                   |
| ENTRADA 1                       | 27   | Bomba ligou                                 |
| ENTRADA 2                       | 14   | Bomba falhou                                |
| ENTRADA 3                       | 13   | Falha no inversor                            |
| ENTRADA 4                       | 4    | Painel sem energia (sem rede CA)            |
| Sensor de temperatura (DS18B20) | 15   | Temperatura da bomba (OneWire)               |
| Sensor de vibracao (analogico)  | 35   | Vibracao da bomba (ADC1)                     |
| LED onboard                     | 2    | Pisca apos cada envio                        |

### Esquema de Ligacao

```
ESP32 #2 (Gateway)        JSN-SR04T (Tanque Inferior)
  GPIO32  ----> TRIG
  GPIO33  <---- ECHO
  5V      ----> VCC
  GND     ----> GND

ESP32 #2 (Gateway)        Entradas (contato seco do quadro/inversor da bomba)
  GPIO27  <---- ENTRADA 1: Bomba ligou
  GPIO14  <---- ENTRADA 2: Bomba falhou
  GPIO13  <---- ENTRADA 3: Falha no inversor
  GPIO4   <---- ENTRADA 4: Painel sem energia
  GND     ----> Comum dos contatos (GND ativo, INPUT_PULLUP)

ESP32 #2 (Gateway)        DS18B20 (temperatura)
  GPIO15  <---> DQ (dados, com resistor de pull-up 4.7k para 3V3)
  3V3     ----> VCC
  GND     ----> GND

ESP32 #2 (Gateway)        Sensor de vibracao (saida analogica)
  GPIO35  <---- AOUT (ADC1, somente leitura)
  3V3/5V  ----> VCC (conforme o modulo)
  GND     ----> GND
```

### Calibracao

`config.h`:
```cpp
#define DIST_CAIXA_VAZIA  90   // cm com o tanque inferior vazio
#define DIST_CAIXA_CHEIA  15   // cm com o tanque inferior cheio
#define TI_NIVEL_ALERTA   20   // % - dispara alerta de nivel baixo
#define TI_NIVEL_OK       80   // % - considera "abastecido"
#define TEMP_ALERTA_C     60   // graus C - alerta de temperatura alta na bomba
#define VIBRACAO_LIMIAR   2500 // leitura ADC acima disso = vibracao excessiva
```

> O sensor de vibracao precisa ser calibrado no local: registre a leitura
> (`tiVibracaoValor`, exibida no Serial Monitor) com a bomba funcionando
> normalmente e ajuste `VIBRACAO_LIMIAR` acima desse valor.

### Alertas no Grupo do WhatsApp (Tanque Inferior)

| Evento                                  | Mensagem                                |
|-------------------------------------------|---------------------------------------------|
| Nivel <= `TI_NIVEL_ALERTA`                 | "Nivel baixo no Tanque Inferior!" (+ "Entrada 1 acionada: Bomba LIGADA" se a bomba estiver ligada) |
| Nivel >= `TI_NIVEL_OK`                     | "Tanque Inferior abastecido!"                |
| Entrada 2 acionada (Bomba falhou)          | "FALHA NA BOMBA!" + causas possiveis        |
| Entrada 2 normalizada                      | "Falha na bomba normalizada"                |
| Entrada 3 acionada (Falha no inversor)     | "FALHA NO INVERSOR!"                        |
| Entrada 3 normalizada                      | "Falha no inversor normalizada"             |
| Entrada 4 acionada (Painel sem energia)    | "PAINEL SEM ENERGIA!"                       |
| Entrada 4 normalizada                      | "Energia do painel restabelecida!"          |
| Temperatura > `TEMP_ALERTA_C`              | "Temperatura alta na bomba!"                 |
| Temperatura normalizada                    | "Temperatura da bomba normalizada"           |
| Vibracao > `VIBRACAO_LIMIAR`               | "Vibracao excessiva detectada na bomba!"     |
| Vibracao normalizada                       | "Vibracao da bomba normalizada"              |
| Leitura invalida do sensor ultrassonico    | "Sensor com leitura invalida!"               |

A cada leitura (a cada `INTERVALO_MEDICAO_MS`, padrao 30s), o Gateway envia os
dados (nivel, distancia, as 4 entradas, temperatura e vibracao) para o
Supabase (tabela `tanque_inferior`).

> Entrada 1 (Bomba ligou) e informativa: nao gera alerta proprio, mas seu
> estado e enviado ao Supabase e aparece junto com o alerta de nivel baixo.

---

## 3. Alarme de Incendio (ESP32-CAM, com foto)

**ESP32-CAM (AI-Thinker)**, com camera OV2640 onboard. Le 2 entradas digitais
da central de alarme de incendio e tira fotos da central que sao enviadas
para a dashboard e para o grupo do WhatsApp.

### Entradas

| Entrada    | GPIO | Funcao                              |
|------------|------|----------------------------------------|
| ENTRADA 1  | 13   | Alarme acionado (central disparou)     |
| ENTRADA 2  | 15   | Central com avaria/falha               |

> O modulo AI-Thinker ESP32-CAM usa quase todos os GPIOs para a camera, o
> cartao SD e pinos de boot/strapping. **So GPIO13 e GPIO15 estao livres** —
> nao reaproveite GPIO0, GPIO2, GPIO4, GPIO12, GPIO33 nem os pinos da camera
> (5,18,19,21,22,23,25,26,27,32,34,35,36,39).

### Comportamento

| Situacao                                  | Dashboard                          | WhatsApp                                  |
|----------------------------------------------|---------------------------------------|----------------------------------------------|
| Nenhuma entrada acionada                      | Status "Sistema Normal" + sinaleiro verde | "Alarme de incendio normalizado" / "Avaria normalizada" (na transicao) |
| Entrada 1 acionada (Alarme)                   | Status "alarme" + sirene + foto da central | "ALARME DE INCENDIO ACIONADO!" + foto      |
| Entrada 2 acionada (Avaria/Falha na central)  | Status "avaria" + sirene + foto da central | "Central de alarme com AVARIA/FALHA!" + foto |

- A foto e capturada (VGA/JPEG) e enviada para o Supabase Storage, bucket
  `fotos-incendio`, sempre no mesmo caminho (`incendio/<condominio>.jpg`) —
  a foto anterior e **sobrescrita automaticamente** (upload com
  `x-upsert: true`), sem precisar apagar manualmente.
- A URL publica da foto e gravada na tabela `alarme_incendio`
  (`foto_url` / `foto_atualizada_em`) para a dashboard exibir.
- Enquanto o status nao for "normal", uma nova foto e enviada a cada
  `FOTO_INTERVALO_MS` (padrao 1 hora) para manter a foto atualizada.
- A cada `INTERVALO_MEDICAO_MS` (padrao 30s) o status atual e enviado ao
  Supabase (heartbeat), mesmo sem mudanca.

### Esquema de Ligacao

```
ESP32-CAM (AI-Thinker)    Entradas (contato seco da central de incendio)
  GPIO13  <---- ENTRADA 1: Alarme acionado
  GPIO15  <---- ENTRADA 2: Central com avaria/falha
  GND     ----> Comum dos contatos (GND ativo, INPUT_PULLUP)
```

### Gravacao do ESP32-CAM

O ESP32-CAM nao tem porta USB — grave com um adaptador **FTDI USB-Serial**
(3.3V):

| FTDI | ESP32-CAM |
|------|-----------|
| 5V   | 5V        |
| GND  | GND       |
| TX   | U0R (RX)  |
| RX   | U0T (TX)  |

1. Ligue **GPIO0 ao GND** (modo de gravacao)
2. Na Arduino IDE: **Ferramentas > Placa > "AI Thinker ESP32-CAM"**, com
   **PSRAM: Enabled**
3. Faca o upload do `esp32cam_alarme_incendio.ino`
4. Apos o upload, **desligue GPIO0 do GND** e pressione o botao **RESET**
   para iniciar normalmente
5. Abra o Serial Monitor (115200) para acompanhar o boot, conexao WiFi e
   inicializacao da camera

---

## 4. Bombas de Incendio

ESP32 DevKit standalone (WiFi direto), monitora 3 entradas do quadro de
comando das bombas de incendio.

### Entradas

| Entrada    | GPIO | Funcao                                  |
|------------|------|--------------------------------------------|
| ENTRADA 1  | 27   | Bomba Principal de Incendio ligada         |
| ENTRADA 2  | 14   | Falha na Bomba de Incendio                  |
| ENTRADA 3  | 13   | Bomba Reserva de Incendio ligada           |

### Esquema de Ligacao

```
ESP32 (Bombas de Incendio)  Entradas (contato seco do quadro de comando)
  GPIO27  <---- ENTRADA 1: Bomba Principal de Incendio ligada
  GPIO14  <---- ENTRADA 2: Falha na Bomba de Incendio
  GPIO13  <---- ENTRADA 3: Bomba Reserva de Incendio ligada
  GND     ----> Comum dos contatos (GND ativo, INPUT_PULLUP)
```

### Alertas no Grupo do WhatsApp

| Evento                                      | Mensagem                                  |
|------------------------------------------------|----------------------------------------------|
| Entrada 1 acionada                              | "Bomba Principal de Incendio LIGADA!"        |
| Entrada 1 normalizada                           | "Bomba Principal de Incendio desligada."     |
| Entrada 2 acionada                              | "FALHA NA BOMBA DE INCENDIO!"                |
| Entrada 2 normalizada                           | "Falha na bomba de incendio normalizada."    |
| Entrada 3 acionada                              | "Bomba Reserva de Incendio LIGADA!"          |
| Entrada 3 normalizada                           | "Bomba Reserva de Incendio desligada."       |
| Inicializacao                                   | "Monitor Bombas de Incendio iniciado!"       |

Envia o status ao Supabase (tabela `bombas_incendio`) sempre que uma entrada
muda de estado, alem de um heartbeat a cada `INTERVALO_ENVIO_MS` (padrao 30s).

---

## 5. Cancela da Portaria

ESP32 DevKit standalone (WiFi direto), monitora 2 sinais de falha das
cancelas da portaria.

### Entradas

| Entrada    | GPIO | Funcao                  |
|------------|------|---------------------------|
| ENTRADA 1  | 27   | Cancela 1 em falha        |
| ENTRADA 2  | 14   | Cancela 2 em falha        |

### Esquema de Ligacao

```
ESP32 (Cancela da Portaria)  Entradas (contato seco do quadro das cancelas)
  GPIO27  <---- ENTRADA 1: Cancela 1 em falha
  GPIO14  <---- ENTRADA 2: Cancela 2 em falha
  GND     ----> Comum dos contatos (GND ativo, INPUT_PULLUP)
```

### Alertas no Grupo do WhatsApp

| Evento                          | Mensagem                                   |
|------------------------------------|------------------------------------------------|
| Entrada 1 acionada                  | "Cancela 1 da portaria em FALHA!"               |
| Entrada 1 normalizada               | "Cancela 1 da portaria normalizada."            |
| Entrada 2 acionada                  | "Cancela 2 da portaria em FALHA!"               |
| Entrada 2 normalizada               | "Cancela 2 da portaria normalizada."            |
| Inicializacao                       | "Monitor Cancela da Portaria iniciado!"         |

Envia o status ao Supabase (tabela `cancela_portaria`) sempre que uma
entrada muda de estado, alem de um heartbeat a cada `INTERVALO_ENVIO_MS`
(padrao 30s).

---

## 6. Agua da Rua

ESP32 DevKit standalone (WiFi direto), com sensor de fluxo por efeito Hall
(ex: YF-S201) instalado na entrada de agua da rua. Calcula a vazao (L/min) e
avisa se o fluxo parar por muito tempo.

### Sensor

| Sinal      | GPIO | Funcao                                          |
|------------|------|------------------------------------------------------|
| FLUXO_PIN  | 27   | Sensor de fluxo (hall effect, pulsos por interrupcao) |

### Esquema de Ligacao

```
ESP32 (Agua da Rua)   Sensor de Fluxo (ex: YF-S201)
  GPIO27  <---- Sinal (pulsos)
  5V      ----> VCC (vermelho)
  GND     ----> GND (preto)
```

### Calibracao

`config.h`:
```cpp
#define FLUXO_FATOR_CALIBRACAO  7.5    // pulsos/s por L/min - tipico do YF-S201, AJUSTAR no local
#define FLUXO_MINIMO_LPM        0.5    // abaixo disso = "sem fluxo"
#define INTERVALO_MEDICAO_MS    30000UL // calcula vazao e envia a cada 30s
#define FLUXO_TIMEOUT_MS        1800000UL // alerta apos 30 min sem fluxo
```

> A agua da rua costuma chegar de forma intermitente (em horarios
> determinados). Ajuste `FLUXO_TIMEOUT_MS` conforme o padrao de
> abastecimento do condominio para evitar falsos alarmes, e calibre
> `FLUXO_FATOR_CALIBRACAO` medindo um volume conhecido escoando em um tempo
> conhecido.

### Alertas no Grupo do WhatsApp

| Evento                                  | Mensagem                                       |
|---------------------------------------------|----------------------------------------------------|
| Sem fluxo por mais de `FLUXO_TIMEOUT_MS`     | "Agua da rua parou! Sem fluxo detectado ha mais de 30 minutos." |
| Fluxo normalizado                            | "Agua da rua normalizada — fluxo detectado novamente." |
| Inicializacao                                | "Monitor Agua da Rua iniciado!"                     |

Envia a vazao (`fluxo_lpm`) e o status (`fluxo_ativo`) ao Supabase (tabela
`agua_rua`) a cada `INTERVALO_MEDICAO_MS` (padrao 30s).

---

## Configuracao da Evolution API (VPS)

### 1. Descobrir o ID do grupo do WhatsApp

No terminal do VPS:

```bash
curl -H "apikey: SUA_APIKEY" \
  "http://localhost:8080/group/fetchAllGroups/SUA_INSTANCIA?getParticipants=false"
```

Procure o grupo pelo `subject` (nome) e copie o `id` — formato `120363xxxxxxxxxxx@g.us`.

> A API key e o valor de `AUTHENTICATION_API_KEY` no docker-compose/.env da Evolution API.
> A instancia e a mesma usada no seu projeto do leitor biometrico.

### 2. Liberar a porta 8080 no firewall do VPS (se ainda nao estiver)

Painel Hostinger > Regras de firewall > permitir TCP 8080
(ou restrinja a porta apenas ao IP da sua internet local, mais seguro).

### 3. Testar envio manual

```bash
curl -X POST "http://SEU_IP_VPS:8080/message/sendText/SUA_INSTANCIA" \
  -H "Content-Type: application/json" \
  -H "apikey: SUA_APIKEY" \
  -d '{"number": "120363xxxxxxxxxxx@g.us", "text": "Teste monitor caixa d agua"}'
```

Se a mensagem chegar no grupo, a API esta pronta.

## Ordem de Configuracao dos ESP32

### Passo 1 - Obter o MAC do Gateway (ESP32 #2)

1. Grave `get_mac_address/get_mac_address.ino` no **ESP32 #2**
2. Serial Monitor (115200) exibe:
```
MAC: AA:BB:CC:DD:EE:FF
Para o config.h: {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}
```

### Passo 2 - Configurar o Sensor (ESP32 #1)

`esp32_sensor/config.h`:
```cpp
uint8_t GATEWAY_MAC[] = {0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF}; // MAC do gateway
#define DIST_CAIXA_VAZIA  90   // calibre para sua caixa
#define DIST_CAIXA_CHEIA  15
```

### Passo 3 - Configurar o Gateway + Tanque Inferior (ESP32 #2)

`esp32_gateway/config.h`:
```cpp
#define CONDOMINIO_NOME "Condominio Principal"  // nome unico por condominio!
#define EVO_BASE_URL   "http://SEU_IP_VPS:8080"
#define EVO_INSTANCE   "sua_instancia"
#define EVO_APIKEY     "sua_apikey"
#define WHATS_GROUP_ID "120363xxxxxxxxxxx@g.us"

// Dashboard (Supabase) - veja water-tank-dashboard/README.md
#define SUPABASE_URL   "https://xxxxxxxx.supabase.co"
#define SUPABASE_KEY   "eyJ...sua_anon_key..."

// Tanque Superior (via ESP-NOW)
#define NIVEL_ALERTA  20
#define NIVEL_OK      80

// Tanque Inferior (sensores ligados neste mesmo ESP32)
#define DIST_CAIXA_VAZIA  90   // calibre para o tanque inferior
#define DIST_CAIXA_CHEIA  15
#define TI_NIVEL_ALERTA   20
#define TI_NIVEL_OK       80
#define TEMP_ALERTA_C     60
#define VIBRACAO_LIMIAR   2500
```

### Passo 4 - Gravar o Gateway e configurar o WiFi no local (WiFiManager)

1. No Arduino IDE, selecione a placa **ESP32 Dev Module**
2. Grave `esp32_gateway.ino` no ESP32 #2 e ligue a alimentacao
3. Na primeira ligacao (sem rede salva), o ESP32 cria o AP
   **`CaixaDagua-Setup`** (senha `12345678`). Conecte pelo celular — o portal
   abre sozinho (se nao abrir, acesse `192.168.4.1`). Toque em **Configure
   WiFi**, escolha a rede do condominio, digite a senha e salve
4. O Serial deve mostrar `[WiFi] Conectado! IP: ...`
5. Ligue os sensores e entradas do **Tanque Inferior** conforme o esquema da
   secao acima (JSN-SR04T, 4 entradas do quadro/inversor, DS18B20 e sensor
   de vibracao)
6. Deve chegar "Gateway reiniciado!" no grupo do WhatsApp, ja com o nivel
   atual do Tanque Superior (ou "aguardando sinal do sensor..." se o ESP32 #1
   ainda nao foi ligado) e do Tanque Inferior
7. Grave `esp32_sensor.ino` no ESP32 #1 — Serial deve mostrar "[ESP-NOW] Enviado com sucesso"
8. Acesse a dashboard ([`water-tank-dashboard`](../water-tank-dashboard/)) e selecione
   o condominio no seletor do topo

### Passo 5 - Configurar os demais nos (Alarme de Incendio, Bombas de
Incendio, Cancela da Portaria, Agua da Rua)

Para cada um dos 4 nos restantes, o processo e o mesmo:

1. Edite `config.h` da pasta correspondente:
   - `CONDOMINIO_NOME` — **igual ao do gateway** do mesmo condominio
   - `EVO_BASE_URL`, `EVO_INSTANCE`, `EVO_APIKEY`, `WHATS_GROUP_ID` — iguais
     aos do gateway
   - `SUPABASE_URL`, `SUPABASE_KEY` — iguais aos do gateway
   - Ajuste pinos/calibracao especificos do modulo (veja as secoes acima)
2. Grave o `.ino` no ESP32 (ou ESP32-CAM, no caso do Alarme de Incendio — veja
   as instrucoes de gravacao na secao 3)
3. Ligue o modulo. Se nao houver rede WiFi salva, ele cria o proprio AP de
   configuracao (veja a tabela em [Visao geral dos nos](#visao-geral-dos-nos)),
   senha `12345678`
4. Conecte pelo celular nessa rede — o portal de configuracao abre sozinho
   (se nao abrir, acesse `192.168.4.1` no navegador). Toque em **Configure
   WiFi**, escolha a rede do condominio, digite a senha e salve
5. Deve chegar a mensagem de "Monitor ... iniciado!" no grupo do WhatsApp
6. Na dashboard, o card correspondente passa a exibir os dados desse no

> Para trocar de rede WiFi depois (ex: mudou a senha do roteador), em
> qualquer no com WiFiManager (2 a 6): segure o ESP32 fora do alcance da
> rede antiga ou aguarde a falha de conexao — o portal de configuracao reabre
> automaticamente apos `PORTAL_TIMEOUT_S` (padrao 180s).

## Varios Condominios

Cada condominio recebe o conjunto completo de ESP32 que estiver instalando
(no minimo o Gateway). Em **todos** os `config.h` de um mesmo condominio,
defina o **mesmo** `CONDOMINIO_NOME` (ex: `"Residencial Sol"`,
`"Condominio Park"`) — todos enviam para o mesmo Supabase e aparecem juntos
no seletor da mesma dashboard. As mensagens do WhatsApp chegam prefixadas
com o nome do condominio (pode usar o mesmo grupo ou um `WHATS_GROUP_ID`
diferente por local).

Os AP de configuracao do WiFiManager ja tem nomes diferentes por tipo de
modulo (`CaixaDagua-Setup`, `AlarmeIncendio-Setup`, `BombaIncendio-Setup`,
`CancelaPortaria-Setup`, `AguaRua-Setup`), entao nao ha conflito mesmo com
varios condominios sendo configurados ao mesmo tempo.
