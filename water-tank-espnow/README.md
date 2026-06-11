# Controle de Caixa d'Agua via ESP-NOW + WiFi + WhatsApp + Dashboard (Supabase/Vercel)

Sistema de dois ESP32 para monitorar o nivel de uma caixa d'agua e o painel/quadro
de comando da bomba. Comunicacao local via **ESP-NOW** (sem WiFi no ponto da caixa),
alertas em um **grupo do WhatsApp** atraves da **Evolution API** (VPS/Docker) e uma
**dashboard web** (Vercel + Supabase) com o nivel animado e o status das 4 entradas.

## Arquitetura

```
[Caixa d'agua - sem WiFi]        [100m - com WiFi]              [VPS Hostinger]

 ESP32 #1 (Sensor)                ESP32 #2 (Gateway)             Evolution API
 +--------------------+           +------------------+           +--------------+
 | JSN-SR04T          |           | WiFi             |   HTTP    | Docker :8080 |
 | ENTRADA 1: Bomba   |  ESP-NOW  | HTTPClient       | --------> | evolution_api|---> Grupo
 |   ligou            | --------> | JSON p/ Evo API  |           | postgres     |     WhatsApp
 | ENTRADA 2: Bomba   |           |                  |           | redis        |
 |   falhou           |           | HTTPS / JSON     |           +--------------+
 | ENTRADA 3: Falha   |           | -------------------------> Supabase (REST) -> Dashboard
 |   no inversor      |           |                                              (Vercel)
 | ENTRADA 4: Painel  |           |
 |   sem energia      |           |
 +--------------------+           +------------------+
```

A dashboard fica em [`../water-tank-dashboard`](../water-tank-dashboard/).

## Entradas do ESP32 Sensor

Sao 4 entradas digitais (contato seco, ligadas ao GND quando ativas,
`INPUT_PULLUP`) vindas do quadro de comando / inversor da bomba:

| Entrada    | GPIO | Funcao                          |
|------------|------|----------------------------------|
| ENTRADA 1  | 27   | Bomba ligou                     |
| ENTRADA 2  | 14   | Bomba falhou                    |
| ENTRADA 3  | 13   | Falha no inversor                |
| ENTRADA 4  | 4    | Painel sem energia (sem rede CA) |

## Alertas no Grupo do WhatsApp

| Evento                                       | Mensagem                                  |
|-----------------------------------------------|--------------------------------------------|
| Nivel baixo + Entrada 1 acionada (Bomba ligou) | "Nivel baixo! Bomba LIGADA" + nivel/dist  |
| Caixa abastecida (>= 80%)                      | "Caixa abastecida! Bomba DESLIGADA"       |
| Entrada 2 acionada (Bomba falhou)              | "FALHA NA BOMBA!" + causas possiveis      |
| Entrada 2 normalizada                          | "Falha na bomba normalizada"              |
| Entrada 3 acionada (Falha no inversor)         | "FALHA NO INVERSOR!"                      |
| Entrada 3 normalizada                          | "Falha no inversor normalizada"           |
| Entrada 4 acionada (Painel sem energia)        | "PAINEL SEM ENERGIA!"                     |
| Entrada 4 normalizada                          | "Energia do painel restabelecida!"        |
| Sensor sem comunicacao (2 min)                 | "Sensor sem comunicacao!"                 |
| Sensor voltou a comunicar                      | "Sensor online novamente!"                |
| Leitura invalida do sensor                     | "Sensor com leitura invalida!"            |
| Gateway ligado                                 | "Monitor iniciado!"                       |

A cada leitura, o gateway tambem envia os dados (nivel, distancia e as 4
entradas) para o Supabase, que alimenta a dashboard em tempo real.

## Hardware

| Item               | Qtd | Uso                                  |
|--------------------|-----|----------------------------------------|
| ESP32 DevKit       | 2   | Sensor + Gateway                       |
| JSN-SR04T          | 1   | Sensor de nivel                        |
| Sinais do quadro / inversor | 4 | Contatos secos -> ENTRADA 1-4 (GND ativo) |
| Fonte 5V 1A        | 2   | Uma para cada ESP32                    |

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

### Passo 3 - Configurar o Gateway (ESP32 #2)

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
```

> O WiFi **nao** vai no codigo: configura-se no local (proximo passo).

### Passo 4 - Gravar e configurar o WiFi no local (WiFiManager)

1. Grave `esp32_gateway.ino` no ESP32 #2 e ligue-o no condominio
2. Se nao houver rede salva, ele cria o ponto de acesso **CaixaDagua-Setup**
   (senha `12345678`)
3. Conecte pelo celular nessa rede — o portal de configuracao abre sozinho
   (se nao abrir, acesse `192.168.4.1` no navegador)
4. Toque em **Configure WiFi**, escolha a rede do condominio, digite a senha
   e salve. O ESP32 reinicia ja conectado (a rede fica salva na memoria flash)
5. Deve chegar "Monitor iniciado!" no grupo do WhatsApp
6. Grave `esp32_sensor.ino` no ESP32 #1 — Serial deve mostrar "[ESP-NOW] Enviado com sucesso"
7. Acesse a dashboard ([`water-tank-dashboard`](../water-tank-dashboard/)) e selecione
   o condominio no seletor do topo

> Para trocar de rede WiFi depois (ex: mudou a senha do roteador): segure o
> ESP32 fora do alcance da rede antiga ou aguarde a falha de conexao — o portal
> reabre automaticamente.

## Varios Condominios

Cada condominio recebe um **par de ESP32** (sensor + gateway). No gateway de
cada local, defina um `CONDOMINIO_NOME` diferente — todos enviam para o mesmo
Supabase e aparecem no seletor da mesma dashboard. As mensagens do WhatsApp
chegam prefixadas com o nome do condominio (pode usar o mesmo grupo ou um
`WHATS_GROUP_ID` diferente por local).

## Esquema de Ligacao (ESP32 #1 - Sensor)

```
ESP32 #1       JSN-SR04T
  GPIO32  ----> TRIG
  GPIO33  <---- ECHO
  5V      ----> VCC
  GND     ----> GND

ESP32 #1       Entradas (contato seco do quadro/inversor da bomba)
  GPIO27  <---- ENTRADA 1: Bomba ligou
  GPIO14  <---- ENTRADA 2: Bomba falhou
  GPIO13  <---- ENTRADA 3: Falha no inversor
  GPIO4   <---- ENTRADA 4: Painel sem energia
  GND     ----> Comum dos contatos (GND ativo, INPUT_PULLUP)
```

## Calibracao do Sensor

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

## Dicas de Alcance ESP-NOW (100m)

- Em campo aberto: 200-400m — seus 100m estao dentro do alcance
- Posicione o gateway em janela/area aberta voltada para a caixa
- Evite obstaculos metalicos na linha de visada
- Se precisar de mais alcance: ESP32 com antena externa (conector U.FL)
