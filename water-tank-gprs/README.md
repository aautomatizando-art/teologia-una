# Controle de Caixa d'Agua via GPRS + Telegram

Projeto de automacao para monitoramento e controle de nivel de caixa d'agua
usando **ESP32 + SIM800L** com alertas via **Telegram**, sem necessidade de WiFi.

## Funcionalidades

- Medicao continua do nivel da caixa via sensor ultrassonico
- Alerta automatico no Telegram quando o nivel baixa
- Liga a bomba automaticamente ao detectar nivel baixo
- Desliga a bomba e notifica quando a caixa estiver cheia
- Relatorio de status periodico (nivel, bomba, sinal GSM)
- Reconexao automatica ao GPRS em caso de queda de sinal

## Hardware Necessario

| Componente             | Especificacao              | Qtd |
|------------------------|----------------------------|-----|
| Microcontrolador       | ESP32 DevKit               |  1  |
| Modem GSM              | SIM800L EVB                |  1  |
| Sensor nivel           | JSN-SR04T (impermeavel)    |  1  |
| Modulo rele            | Rele 5V 1 canal            |  1  |
| Fonte de alimentacao   | 5V 3A (minimo)             |  1  |
| Regulador de tensao    | LM2596 ajustado para 4.0V  |  1  |
| Chip SIM               | Qualquer operadora com GPRS|  1  |
| Antena GSM             | Antena SMA 2dBi            |  1  |

## Esquema de Ligacao

```
                    ┌──────────────────┐
                    │      ESP32       │
                    │                  │
JSN-SR04T TRIG ────>│ GPIO32           │
JSN-SR04T ECHO <────│ GPIO33           │
JSN-SR04T VCC  ────>│ 5V               │
JSN-SR04T GND  ────>│ GND              │
                    │                  │
Rele IN        ────>│ GPIO25           │
Rele VCC       ────>│ 5V               │
Rele GND       ────>│ GND              │
                    │                  │
SIM800L TXD    ────>│ GPIO16 (RX)      │
SIM800L RXD    <────│ GPIO17 (TX)      │
SIM800L GND    ────>│ GND              │
                    └──────────────────┘

   ┌─────────────┐
   │   Fonte 5V  │──────────────────────> ESP32 VIN
   │    3A min   │
   └──────┬──────┘
          │
   ┌──────▼──────┐
   │  LM2596     │ ajustado para 4.0V ──> SIM800L VCC
   │  Regulador  │                   ──> SIM800L GND (comum)
   └─────────────┘

ATENCAO: SIM800L NUNCA alimentar direto no 5V!
Pico de corrente: ~2A. Use fonte dedicada.
```

## Configuracao

### 1. Criar Bot no Telegram

1. Abra o Telegram e busque **@BotFather**
2. Envie `/newbot` e siga as instrucoes
3. Copie o **token** gerado
4. Busque **@userinfobot** para obter seu **Chat ID**

### 2. Editar config.h

```cpp
// APN da operadora (descomente a correta)
#define APN "claro.com.br"

// Telegram
#define BOT_TOKEN  "1234567890:AAxxxxxxxxxxxxxxxxxxxxxx"
#define CHAT_ID    "123456789"

// Calibracao da caixa (ajuste para sua instalacao)
#define DIST_CAIXA_VAZIA   90   // sensor a 90cm da agua = caixa vazia
#define DIST_CAIXA_CHEIA   15   // sensor a 15cm da agua = caixa cheia
```

### 3. Instalar Bibliotecas

Veja o arquivo `libraries.txt` para a lista completa.

### 4. Gravar no ESP32

- Placa: **ESP32 Dev Module**
- Upload Speed: **115200**
- Partition Scheme: **Default 4MB**

## APNs por Operadora (Brasil)

| Operadora | APN                        |
|-----------|----------------------------|
| Claro     | `claro.com.br`             |
| TIM       | `timbrasil.com.br`         |
| Vivo      | `zap.vivo.com.br`          |
| Oi        | `gprs.oi.com.br`           |

> **Dica rural:** A Claro geralmente tem a melhor cobertura 2G em areas rurais do Brasil.

## Calibracao do Sensor

O sensor e instalado na tampa da caixa apontando para baixo.
Meca as distancias reais da sua caixa:

```
[Sensor]
   |  <- 15cm  (DIST_CAIXA_CHEIA:  caixa cheia)
   |
   |  <- 90cm  (DIST_CAIXA_VAZIA:  caixa vazia, liga bomba)
   |
[Fundo]
```

## Mensagens Telegram

| Evento            | Mensagem enviada                        |
|-------------------|-----------------------------------------|
| Inicializacao     | "Sistema iniciado! Monitorando..."      |
| Nivel baixo       | "ALERTA: Caixa d'agua baixa! X%"        |
| Caixa abastecida  | "Caixa d'agua abastecida! Bomba desligada." |
| Status periodico  | Nivel, estado bomba, sinal GSM, uptime  |
