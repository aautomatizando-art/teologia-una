# Controle de Caixa d'Agua via LTE-M/NB-IoT + Telegram

Monitoramento e controle automatico de nivel de caixa d'agua usando
**LilyGO T-SIM7000G** (ESP32 + modem integrado) com alertas via **Telegram**.
Sem necessidade de WiFi — funciona via rede celular LTE-M / NB-IoT.

> **Chip:** Algar Telecom M2M/IoT
> **Placa:** LilyGO T-SIM7000G (ESP32 + SIM7000G na mesma placa)

## Funcionalidades

- Medicao continua do nivel via sensor ultrassonico impermeavel (JSN-SR04T)
- Alerta no Telegram quando o nivel baixa
- Liga bomba automaticamente ao detectar nivel baixo
- Desliga bomba e notifica quando a caixa estiver cheia
- Relatorio de status a cada 5 minutos (nivel, bomba, sinal, uptime)
- Reconexao automatica em caso de queda de sinal
- Suporte a NB-IoT + LTE-M (melhor cobertura rural)

## Hardware Necessario

| Componente          | Especificacao                   | Qtd |
|---------------------|---------------------------------|-----|
| Placa principal     | LilyGO T-SIM7000G               |  1  |
| Sensor nivel        | JSN-SR04T (impermeavel)         |  1  |
| Modulo rele         | Rele 5V 1 canal                 |  1  |
| Fonte alimentacao   | 5V 2A (USB-C ou conector)       |  1  |
| Chip SIM            | Algar Telecom M2M/IoT (Micro)   |  1  |

> A LilyGO T-SIM7000G ja tem o ESP32 e o SIM7000G integrados.
> Nao e necessario regulador de tensao externo para o modem.

## Esquema de Ligacao

```
 LilyGO T-SIM7000G
 +----------------------+
 |  [ESP32 + SIM7000G]  |<---- Chip Algar M2M (slot Micro SIM na placa)
 |                      |
 |  GPIO32  ------------|----> JSN-SR04T  TRIG
 |  GPIO33  <-----------|----  JSN-SR04T  ECHO
 |  5V      ------------|----> JSN-SR04T  VCC
 |  GND     ------------|----> JSN-SR04T  GND
 |                      |
 |  GPIO25  ------------|----> Rele  IN
 |  5V      ------------|----> Rele  VCC
 |  GND     ------------|----> Rele  GND
 |                      |
 |  GPIO12  ------------|----> LED status (opcional)
 +----------------------+
        |
     USB-C
   Fonte 5V 2A

 Rele:
   COM  ----> Fio fase da bomba
   NO   ----> Fase da rede eletrica
   (Bomba liga quando GPIO25 = HIGH)
```

## Configuracao

### 1. Criar Bot no Telegram

1. Abra o Telegram e busque **@BotFather**
2. Envie `/newbot` e siga as instrucoes
3. Copie o **token** gerado
4. Busque **@userinfobot** para obter seu **Chat ID**

### 2. Editar `config.h`

```cpp
// Telegram
#define BOT_TOKEN  "1234567890:AAxxxxxxxxxxxxxxxxxxxxxx"
#define CHAT_ID    "123456789"

// Calibracao — meca na sua caixa (valores em cm):
#define DIST_CAIXA_VAZIA  90   // sensor a 90cm da agua = caixa vazia
#define DIST_CAIXA_CHEIA  15   // sensor a 15cm da agua = caixa cheia
```

### 3. Instalar Bibliotecas

Ver `libraries.txt`. Instalar via:
`Sketch > Incluir Biblioteca > Gerenciar Bibliotecas`

### 4. Gravar na placa

| Configuracao     | Valor              |
|------------------|--------------------|
| Placa            | ESP32 Dev Module   |
| Upload Speed     | 115200             |
| Partition Scheme | Default 4MB        |

## Calibracao do Sensor

O JSN-SR04T e instalado na tampa da caixa, apontando para baixo:

```
[JSN-SR04T na tampa]
   |  <- 15cm  DIST_CAIXA_CHEIA  -> caixa cheia, desliga bomba
   |
   ~  nivel da agua
   |
   |  <- 90cm  DIST_CAIXA_VAZIA  -> caixa vazia, liga bomba + alerta
   |
[Fundo da caixa]
```

## Mensagens no Telegram

| Evento            | Mensagem                                   |
|-------------------|--------------------------------------------|
| Inicializacao     | Sistema iniciado! Operadora, sinal...      |
| Nivel baixo       | ALERTA: Caixa d'agua baixa! X%            |
| Caixa cheia       | Caixa d'agua abastecida! Bomba desligada. |
| Status (5/5min)   | Nivel, bomba, sinal GSM, uptime           |

## Modo de Rede (config.h)

```cpp
#define NETWORK_MODE 51   // NB-IoT + LTE-M (recomendado)
// Opcoes:
// 2  = NB-IoT apenas      (maxima cobertura rural)
// 38 = LTE-M apenas       (mais rapido)
// 51 = NB-IoT + LTE-M     (automatico)
```

## Cobertura Algar Telecom

A Algar atua principalmente em:
- Triangulo Mineiro e Alto Paranaiba (MG)
- Noroeste de Sao Paulo
- Sul de Goias e norte do Parana

Verifique a cobertura NB-IoT/LTE-M no site da Algar antes de instalar.
