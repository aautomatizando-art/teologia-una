# Diagrama – ASCAEL ACDE 24/16 → ESP32 → Telegram

## Informações da placa (confirmadas na inspeção física)

- **Microcontrolador:** PIC18F4520-I/PQ (Microchip)
- **MAX232 (CI C16):** footprint presente na placa, **não instalado de fábrica**
- **Conector DB9:** footprint presente na placa (pads CN8), **não instalado de fábrica**
- **RS232 é opcional** — a Ascael fornece como acessório pago

---

## Passo 1 — Instalar o MAX232 na placa

Compre o chip **MAX232** (ou equivalente: MAX232A, ST232, ICL232) em
qualquer loja de eletrônica. Custo: R$ 5–10.

Encaixe no soquete **C16** da placa com o entalhe voltado para o lado
marcado na silkscreen.

> Os capacitores do circuito de carga (charge pump) já estão soldados
> na placa ao redor do C16 — só o chip mesmo que falta.

---

## Passo 2 — Soldar fios nos pads do DB9

O conector DB9 não está instalado, mas os **pads (furos) estão na placa**.
Solde fios diretamente nesses dois pads:

```
Pad DB9 nº 2  (TX da central)  ──► fio vermelho
Pad DB9 nº 5  (GND)            ──► fio preto
```

### Como identificar os pads do DB9

O DB9 tem 9 pinos em dois fileiras (5 em cima + 4 embaixo):

```
  1   2   3   4   5      ← fileira de cima
    6   7   8   9        ← fileira de baixo
```

- **Pino 2** = segundo furo da fileira de cima (TX — sinal da central)
- **Pino 5** = quinto furo da fileira de cima (GND)

---

## Passo 3 — Ligar ao ESP32 via MAX3232

Use o módulo **MAX3232** (conversor RS232 → 3.3V) para conectar ao ESP32:

```
┌──────────────────┐        ┌──────────┐        ┌──────────┐
│  ACDE 24/16      │        │ MAX3232  │        │  ESP32   │
│  (placa interna) │        │ módulo   │        │          │
│                  │        │          │        │          │
│  DB9 pad 2 (TX) ─┼──────►─┤ R1IN     │        │          │
│  DB9 pad 5 (GND)─┼──┐    │ R1OUT   ─┼───────►─┤ GPIO16   │
└──────────────────┘  │    │ VCC      ├── 3.3V ─┤ 3.3V    │
                      └───►─┤ GND      ├── GND ──┤ GND     │
                            └──────────┘        └──────────┘
```

### Tabela de conexões

| DB9 pad (placa) | MAX3232 | ESP32 |
|-----------------|---------|-------|
| Pad 2 – TX      | R1IN    | —     |
| Pad 5 – GND     | GND     | GND   |
| —               | R1OUT   | GPIO16 (RX2) |
| —               | VCC     | 3.3V  |
| —               | GND     | GND   |

> **ATENÇÃO:** Use MAX3232 (3.3V) para o módulo externo.
> O MAX232 interno à placa converte para RS232 (±12V).
> O MAX3232 externo converte de volta para 3.3V (para o ESP32).

---

## Passo 4 — Verificar o formato (RAW_MODE)

Com tudo ligado, grave o sketch com `RAW_MODE true` em `config.h`:

```cpp
#define BAUD_CENTRAL  9600
#define RAW_MODE      true
```

Abra o Monitor Serial (115200 baud), acione uma botoeira e observe:

```
[RAW] 01 ALARME BOTOEIRA PAV1 14:32:10
      HEX: 30 31 20 41 4C 41 52 4D 45 ...
```

Copie a saída para ajustar o parser e depois mude `RAW_MODE false`.

---

## Passo 5 — Configurar zonas (opcional)

Em `config.h`, preencha com os nomes dos seus dispositivos:

```cpp
const ZonaInfo ZONAS[] = {
    {  1, "BOTOEIRA PAV TERREO"   },
    {  2, "BOTOEIRA PAV SUPERIOR" },
    {  0, nullptr }
};
```

---

## Mensagens no Telegram

```
🔥 ALARME DE INCÊNDIO

📍 Zona: 01 — BOTOEIRA PAV TERREO
📋 01 ALARME BOTOEIRA 14:32:10
🕐 14:32:10

Central: ASCAEL ACDE 24/16
```

```
✅ NORMALIZADO

📍 Zona: 01 — BOTOEIRA PAV TERREO
📋 01 NORMAL 14:35:00

Central: ASCAEL ACDE 24/16
```

---

## Baud rates para testar (se RAW_MODE não mostrar nada)

| Tentativa | config.h |
|-----------|----------|
| 1ª | `#define BAUD_CENTRAL 9600` |
| 2ª | `#define BAUD_CENTRAL 4800` |
| 3ª | `#define BAUD_CENTRAL 19200` |
| 4ª | `#define BAUD_CENTRAL 2400` |

---

## Componentes necessários

| Item | Qtd | Observação |
|------|-----|------------|
| MAX232 / MAX232A | 1 | instalar no soquete C16 da placa da central |
| MAX3232 (módulo externo) | 1 | conversor RS232→3.3V para o ESP32 |
| ESP32 (DevKit) | 1 | qualquer modelo |
| Fios finos | 2 | soldar nos pads do DB9 (pinos 2 e 5) |

---

## Bibliotecas necessárias (Arduino IDE)

| Biblioteca | Instalação |
|------------|------------|
| ArduinoJson (Benoit Blanchon) | Gerenciar Bibliotecas |
| WiFi, WiFiClientSecure, HTTPClient | Incluso no core ESP32 |
