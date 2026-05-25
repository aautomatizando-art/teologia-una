# Diagrama de Ligação – CAE-500 XMAX → Arduino → LCD I2C

## Componentes

- Arduino Uno / Nano
- Módulo MAX232 (conversor RS232 ↔ TTL)
- Cabo DB9 fêmea (para conectar na saída RS232 da CAE-500)
- Display LCD 16x2 com módulo I2C (PCF8574)

## Diagrama de Blocos

```
┌─────────────────┐   RS232 ±12V   ┌──────────┐   TTL 5V   ┌─────────────────┐
│  CAE-500 XMAX   │───────────────►│  MAX232  │───────────►│  Arduino Uno    │
│                 │                │          │             │                 │
│  DB9 pino 2(TX) │───────────────►│ T1IN/R1OUT►──Pino 4 RX│                 │
│  DB9 pino 5(GND)│───────────────►│ GND      │   GND ─────│GND              │
└─────────────────┘                └──────────┘             │                 │
                                                            │  A4 (SDA) ──────┼──► LCD SDA
                                                            │  A5 (SCL) ──────┼──► LCD SCL
                                                            │  5V ────────────┼──► LCD VCC
                                                            │  GND ───────────┼──► LCD GND
                                                            └─────────────────┘
```

## Pinos do DB9 na CAE-500 XMAX

| Pino DB9 | Sinal | Conectar em |
|----------|-------|-------------|
| 2        | TX    | MAX232 entrada (T1IN) |
| 3        | RX    | MAX232 saída (R1OUT) — opcional, só se precisar enviar dados |
| 5        | GND   | GND do MAX232 / Arduino |

> **Atenção:** Nunca conecte o TX da central diretamente ao Arduino sem o MAX232 —
> a tensão RS232 (até +15V / -15V) danifica o microcontrolador.

## Módulo MAX232 — Capacitores Necessários

```
MAX232
  C1+ ──[1µF]── C1-
  C2+ ──[1µF]── C2-
  VS+ ──[1µF]── GND
  VS- ──[1µF]── GND
  VCC ── 5V
  GND ── GND
```

Muitos módulos prontos (com PCB) já incluem esses capacitores.

## Configuração Serial na CAE-500 XMAX

Acesse o menu de configuração da central e verifique/ajuste:

| Parâmetro | Valor padrão |
|-----------|-------------|
| Baud rate | 9600 bps    |
| Bits de dados | 8       |
| Paridade  | Nenhuma     |
| Stop bits | 1           |

Se não receber dados com 9600, tente **4800** ou **19200** no sketch
(linha `centralSerial.begin(9600)`).

## O que aparece no LCD

```
┌────────────────┐
│ALARME  End:42  │  ← tipo do evento + endereço decimal
│BOTOEIRA PAV 3  │  ← descrição cadastrada na central
└────────────────┘
```

```
┌────────────────┐
│FALHA   End:7   │
│DETECTOR SALA 1 │
└────────────────┘
```

## Bibliotecas Necessárias (Arduino IDE)

- **SoftwareSerial** — já inclusa no Arduino IDE
- **LiquidCrystal I2C** by Frank de Brabander — instalar via Gerenciar Bibliotecas

## Verificando o Endereço I2C do LCD

Se o display não ligar, abra o Serial Monitor (115200 baud) e carregue
o sketch `I2C_Scanner` para descobrir o endereço correto (0x27 ou 0x3F).
