# Diagrama – Intelbras CIE 1125 → ESP32 → Telegram

## Diferença fundamental em relação à CAE-500 XMAX

| | CAE-500 XMAX | CIE 1125 |
|---|---|---|
| Interface | RS232 (DB9) | **Ethernet RJ45** |
| Protocolo | Texto ASCII serial | **Modbus UDP** |
| Conversor necessário | MAX3232 | **Nenhum** |
| Ligação ao ESP32 | Serial2 (GPIO16) | **WiFi (mesma rede)** |

---

## Diagrama de blocos

```
┌──────────────┐  RJ45  ┌────────────────┐  WiFi  ┌─────────────┐
│  CIE 1125    │───────►│ Switch/Roteador │◄──────►│    ESP32    │
│  Intelbras   │        │                │        │             │
│  IP: 192.168 │        └────────────────┘        │  Telegram ──┼──► Grupo
│  .1.50       │                                  └─────────────┘
│  UDP :502    │
└──────────────┘
```

Nenhum cabo vai direto entre CIE 1125 e ESP32 —
ambos se comunicam pela **rede local**.

---

## Pré-requisitos na central CIE 1125

### 1. Habilitar Modbus UDP
```
Menu da central → Rede → Protocolo → Modbus UDP → Habilitar
```

### 2. Definir IP fixo (recomendado)
```
Menu da central → Rede → Modo → IP Fixo
                         IP: 192.168.1.50
                      Máscara: 255.255.255.0
                      Gateway: 192.168.1.1
```
> Sem IP fixo, o IP pode mudar após reiniciar o roteador,
> quebrando a comunicação.

---

## Como descobrir os registradores dos seus dispositivos

O manual da CIE 1125 (Anexo A) contém a tabela completa de
códigos de evento. Use a fórmula:

```
registrador = codigo_evento / 8   (divisão inteira)
mascara_bit = 1 << (codigo_evento % 8)
```

### Exemplos confirmados no manual Intelbras

| Código de evento | Registrador | Máscara | Descrição |
|-----------------|-------------|---------|-----------|
| 19              | 2           | 0x08    | —         |
| 942             | 117         | 0x40    | —         |
| 1500            | 187         | 0x10    | —         |

### Registradores de resumo (sem máscara)

| Registrador | Tipo    | Descrição                  |
|-------------|---------|----------------------------|
| 200         | uint16  | Quantidade de alarmes ativos |
| 202         | uint16  | Quantidade de falhas ativas  |

---

## Preenchendo o config.h

Após obter os códigos do Anexo A, preencha o array `DISPOSITIVOS[]`:

```cpp
const Dispositivo DISPOSITIVOS[] = {
    // { reg,  mask,  "nome",             "tipo"   }
    {   2,  0x08, "BOTOEIRA PAV 1",    "ALARME" },
    {   2,  0x10, "BOTOEIRA PAV 2",    "ALARME" },
    {   3,  0x01, "DETECTOR SALA 101", "ALARME" },
    { 117,  0x40, "MODULO SAIDA 1",    "FALHA"  },
    {   0,  0x00, nullptr, nullptr }  // não remover
};
```

---

## Mensagens no Telegram

```
🔥 ALARME DE INCÊNDIO

📋 BOTOEIRA PAV 1
🔧 Tipo: ALARME
📍 Reg: 2  Bit: 3
🕐 Uptime: 00:05:32

Central: CIE 1125 | Intelbras
```

```
✅ NORMALIZADO

📋 BOTOEIRA PAV 1
🔧 Tipo: ALARME
📍 Reg: 2  Bit: 3
🕐 Uptime: 00:08:15

Central: CIE 1125 | Intelbras
```

---

## Bibliotecas necessárias (Arduino IDE)

| Biblioteca | Autor | Onde instalar |
|------------|-------|---------------|
| ArduinoJson | Benoit Blanchon | Gerenciar Bibliotecas |
| WiFi, WiFiUDP, HTTPClient | — | Incluso no core ESP32 |

---

## Obtendo o manual com o Anexo A

- Site oficial: [intelbras.com/pt-br/central-de-alarme-de-incendio-cie-1125](https://www.intelbras.com/pt-br/central-de-alarme-de-incendio-cie-1125)
- Seção: **Ajuda e Downloads → Manuais**
- Arquivo: `manual-do-usuario-CIE-1125-CIE-1250-e-CIE-2500.pdf`
- Consulte o **Anexo A** para a tabela completa de códigos de evento
