# Diagrama – CIE 1125 (Relé) → ESP32 → Telegram

## Visão geral

```
┌──────────────────┐  contato seco  ┌───────────┐  WiFi  ┌──────────┐
│    CIE 1125      │────────────────│  PC817    │───────►│  ESP32   │──► Telegram
│                  │                │ (optoacop.)│        │          │
│  Relé Alarme     │── NO + COM ───►│           │        │  GPIO 4  │
│  Relé Falha      │── NO + COM ───►│           │        │  GPIO 5  │
│  Relé Supervisão │── NO + COM ───►│           │        │  GPIO 18 │
└──────────────────┘                └───────────┘        └──────────┘
```

Sem cabo de rede entre a central e o ESP32 —
a ligação é somente por fio nos terminais de relé.

---

## Circuito por relé (repita para cada saída)

```
ESP32 3.3V ──┤ R 1kΩ ├──────────────────── PC817 pino 1 (Anodo  +)
                                            PC817 pino 2 (Catodo -)──── Relé NO
                                                                         Relé COM ──── GND
ESP32 GND ──────────────────────────────── PC817 pino 3 (Emissor)
ESP32 GPIOx (INPUT_PULLUP) ─────────────── PC817 pino 4 (Coletor)
```

### Pinagem do PC817 (DIP-4, visto de cima com o chanfro à esquerda)

```
  ┌────┐
1 │A  C│ 4      1 = Anodo  (+) do LED
2 │K  E│ 3      2 = Catodo (-) do LED
  └────┘         3 = Emissor (E) do transistor
                 4 = Coletor (C) do transistor
```

### Lógica de leitura

| Estado do relé | LED PC817 | Transistor | GPIO ESP32 |
|---------------|-----------|-----------|-----------|
| Fechado (alarme) | Aceso | Conduz | **LOW** |
| Aberto (normal) | Apagado | Corta | **HIGH** |

---

## Terminais de relé na CIE 1125

Consulte o manual físico para a posição exata dos bornes.
Tipicamente a central possui:

| Terminal | Relé | Condição de ativação |
|----------|------|---------------------|
| ALM-NO / ALM-COM | Alarme Geral | Qualquer detector/botoeira em alarme |
| FLT-NO / FLT-COM | Falha Geral | Qualquer falha de supervisão |
| SUP-NO / SUP-COM | Supervisão | Evento de supervisão |

> **Use sempre os terminais NO (Normalmente Aberto) e COM.**
> O terminal NC (Normalmente Fechado) tem lógica inversa.

---

## Ligação completa (3 relés)

```
CIE 1125                PC817 #1              ESP32
ALM-COM ────────────── Catodo              GPIO 4 (INPUT_PULLUP)
ALM-NO  ─── [1kΩ] ─── Anodo   Coletor ───────────────────────►┘
                               Emissor ──── GND

FLT-COM ────────────── Catodo              GPIO 5 (INPUT_PULLUP)
FLT-NO  ─── [1kΩ] ─── Anodo   Coletor ───────────────────────►┘
                               Emissor ──── GND

SUP-COM ────────────── Catodo              GPIO 18 (INPUT_PULLUP)
SUP-NO  ─── [1kΩ] ─── Anodo   Coletor ───────────────────────►┘
                               Emissor ──── GND

                ESP32 3.3V ──────────────── Todos os Anodos (via resistor 1kΩ cada)
                ESP32 GND  ──────────────── Todos os Emissores + COM dos relés
```

---

## Mensagens no Telegram

**Quando a botoeira é acionada:**
```
🔥 ALARME DE INCÊNDIO

📋 ALARME GERAL

Central: CIE 1125 | Intelbras
```

**Quando normaliza:**
```
✅ NORMALIZADO

📋 ALARME GERAL
⏱ Duração: 4m 32s

Central: CIE 1125 | Intelbras
```

**Falha detectada:**
```
⚠️ FALHA NA CENTRAL

📋 FALHA GERAL

Central: CIE 1125 | Intelbras
```

---

## Limitação desta abordagem

O relé informa apenas **se houve alarme**, não **qual dispositivo** específico
acionou. Para saber a botoeira exata, é necessário consultar o painel da
central ou usar a integração via Modbus (requer rede).

Se a central tiver relés de zona independentes (um por andar, por exemplo),
é possível adicionar mais GPIOs no ESP32 para identificar a zona:

```cpp
// Em config.h, adicione os relés de zona:
{ 19, "ZONA 1 – PAV TERREO", "ALARME" },
{ 21, "ZONA 2 – PAV SUPERIOR", "ALARME" },
```
