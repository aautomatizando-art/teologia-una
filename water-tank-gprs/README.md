# Controle de Caixa d'Agua via GPRS + Telegram

Projeto de automacao para monitoramento e controle de nivel de caixa d'agua
usando **ESP32 + SIM800L** com alertas via **Telegram**, sem necessidade de WiFi.

> **Chip utilizado:** Algar Telecom M2M/IoT

## Funcionalidades

- Medicao continua do nivel da caixa via sensor ultrassonico impermeavel
- Alerta automatico no Telegram quando o nivel baixa
- Liga a bomba automaticamente ao detectar nivel baixo
- Desliga a bomba e notifica quando a caixa estiver cheia
- Relatorio de status periodico (nivel, bomba, sinal GSM, uptime)
- Reconexao automatica ao GPRS em caso de queda de sinal

## Hardware Necessario

| Componente           | Especificacao                      | Qtd |
|----------------------|------------------------------------|-----|
| Microcontrolador     | ESP32 DevKit                       |  1  |
| Modem GSM            | SIM800L EVB (com antena SMA)       |  1  |
| Sensor nivel         | JSN-SR04T impermeavel              |  1  |
| Modulo rele          | Rele 5V 1 canal                    |  1  |
| Fonte alimentacao    | 5V 3A minimo                       |  1  |
| Regulador de tensao  | LM2596 ajustado para 4.0V          |  1  |
| Chip SIM             | **Algar Telecom M2M/IoT**          |  1  |

> **ATENCAO:** O chip Algar M2M/IoT vem em formato triplo (Mini/Micro/Nano).
> Use o **Micro SIM** (tamanho do meio) no SIM800L.

## Esquema de Ligacao

```
                    +------------------+
                    |      ESP32       |
                    |                  |
JSN-SR04T TRIG ---->| GPIO32           |
JSN-SR04T ECHO <----| GPIO33           |
JSN-SR04T VCC  ---->| 5V               |
JSN-SR04T GND  ---->| GND              |
                    |                  |
Rele IN        ---->| GPIO25           |
Rele VCC       ---->| 5V               |
Rele GND       ---->| GND              |
                    |                  |
SIM800L TXD    ---->| GPIO16 (RX)      |
SIM800L RXD    <----| GPIO17 (TX)      |
SIM800L GND    ---->| GND (comum)      |
                    +------------------+

   +--------------+
   |  Fonte 5V 3A |------------------------------> ESP32 VIN
   |              |
   +------+-------+
          |
   +------v-------+
   |   LM2596     | ajustado 4.0V -------------> SIM800L VCC
   |  Regulador   |               -------------> SIM800L GND (comum)
   +--------------+

ATENCAO: SIM800L NUNCA alimentar direto no 5V!
Pico de corrente: ~2A. Use regulador dedicado.
```

## Configuracao

### 1. Criar Bot no Telegram

1. Abra o Telegram e busque **@BotFather**
2. Envie `/newbot` e siga as instrucoes
3. Copie o **token** gerado
4. Busque **@userinfobot** para obter seu **Chat ID**

### 2. Editar `config.h`

```cpp
// APN Algar M2M (ja configurado)
#define APN "m2m.algar.com.br"

// Telegram — preencha com seus dados
#define BOT_TOKEN  "1234567890:AAxxxxxxxxxxxxxxxxxxxxxx"
#define CHAT_ID    "123456789"

// Calibracao — meca na sua caixa
#define DIST_CAIXA_VAZIA  90   // sensor a 90cm da agua = caixa vazia
#define DIST_CAIXA_CHEIA  15   // sensor a 15cm da agua = caixa cheia
```

### 3. Instalar Bibliotecas

Ver arquivo `libraries.txt`. Instalar via Arduino IDE:
`Sketch > Incluir Biblioteca > Gerenciar Bibliotecas`

### 4. Gravar no ESP32

| Configuracao      | Valor              |
|-------------------|--------------------|
| Placa             | ESP32 Dev Module   |
| Upload Speed      | 115200             |
| Partition Scheme  | Default 4MB        |

## Calibracao do Sensor

O sensor e instalado na tampa da caixa, apontando para baixo:

```
[Sensor JSN-SR04T]
   |  <- 15cm  (DIST_CAIXA_CHEIA: caixa cheia, desliga bomba)
   |
   ~  nivel da agua
   |
   |  <- 90cm  (DIST_CAIXA_VAZIA: caixa vazia, liga bomba + alerta)
   |
[Fundo da caixa]
```

## Mensagens no Telegram

| Evento              | Mensagem                                    |
|---------------------|---------------------------------------------|
| Inicializacao       | Sistema iniciado! Monitorando...            |
| Nivel baixo         | ALERTA: Caixa d'agua baixa! X%             |
| Caixa abastecida    | Caixa d'agua abastecida! Bomba desligada.  |
| Status (5 em 5 min) | Nivel, bomba, sinal GSM, uptime            |

## Cobertura Algar Telecom

A Algar Telecom atua principalmente em:
- Triangulo Mineiro e Alto Paranaiba (MG)
- Noroeste de Sao Paulo
- Sul de Goias e norte do Parana

Verifique a cobertura no site da Algar antes de instalar.
