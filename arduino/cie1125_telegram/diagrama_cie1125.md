# Diagrama – Intelbras CIE 1125 → ESP32 → Telegram (via rede)

## Opção 1 — Mini roteador dedicado (sem mexer na rede existente)

Cria uma rede local isolada só para a integração:

```
┌─────────────┐  RJ45  ┌──────────────────────┐  WiFi  ┌──────────┐
│  CIE 1125   │───────►│  Mini roteador WiFi  │◄──────►│  ESP32   │──► Telegram
│  IP: fixo   │  LAN   │  (TP-Link, Intelbras) │        │          │
└─────────────┘        │  qualquer modelo ~R$60│        └──────────┘
                       └──────────────────────┘
```

**O roteador não precisa ter internet** — só cria a rede local entre os dois.

### Configuração do mini roteador
1. Ligue o roteador normalmente
2. Conecte o cabo RJ45 da CIE 1125 numa porta LAN (não WAN)
3. Conecte o ESP32 ao WiFi do roteador (SSID e senha no `config.h`)
4. Anote o IP que o roteador atribuiu à central (veja na tabela DHCP do roteador)
5. Configure IP fixo na central com esse mesmo IP

---

## Opção 2 — Rede existente (só um cabo)

```
┌─────────────┐  RJ45  ┌──────────────────────┐  WiFi  ┌──────────┐
│  CIE 1125   │───────►│  Switch / Roteador   │◄──────►│  ESP32   │──► Telegram
│  IP: fixo   │        │  da instalação       │        │          │
└─────────────┘        └──────────────────────┘        └──────────┘
```

Adicione um cabo de rede Cat5e/Cat6 entre a CIE 1125 e o switch mais próximo.

---

## Configuração na central CIE 1125 (ambas as opções)

### 1. Habilitar Modbus UDP
```
Painel da central → Menu → Rede → Protocolo → Modbus UDP → Habilitar
```

### 2. Configurar IP fixo
```
Painel da central → Menu → Rede → Modo → IP Fixo

  IP da central : 192.168.1.50    ← coloque este mesmo valor no config.h
  Máscara       : 255.255.255.0
  Gateway       : 192.168.1.1     ← IP do roteador
```

---

## Passo a passo para descobrir os registradores (SCANNER_MODE)

Como o Anexo A do manual Intelbras está em PDF bloqueado, use o modo scanner
para descobrir os registradores das suas botoeiras sem precisar do manual.

### 1. Ative o modo scanner em `config.h`
```cpp
#define SCANNER_MODE  true
```

### 2. Grave no ESP32 e abra o Monitor Serial (115200 baud)

### 3. Acione uma botoeira na central e observe a saída:
```
[SCANNER] Reg:62  Bit:4  Mask:0x10  codigo=500  0 → 1
```

### 4. Anote os valores e preencha o `DISPOSITIVOS[]` em `config.h`
```cpp
#define SCANNER_MODE  false   // ← desative após descobrir

const Dispositivo DISPOSITIVOS[] = {
    { 62, 0x10, "BOTOEIRA PAV 1", "ALARME" },   // ← valores do scanner
    { 70, 0x01, "BOTOEIRA PAV 2", "ALARME" },
    {  0, 0x00, nullptr, nullptr }
};
```

### 5. Grave novamente — o sistema já notifica pelo Telegram

---

## Fórmula dos registradores (confirmada pelo manual)

```
reg_addr = codigo_evento / 8      (divisão inteira)
bit_mask = 1 << (codigo_evento % 8)
```

| Exemplo confirmado | Reg | Mask | Bit | Código |
|-------------------|-----|------|-----|--------|
| Falha Disp.1 Loop1 | 62 | 0x10 | 4 | 500 |
| Evento código 19  | 2  | 0x08 | 3 | 19  |
| Evento código 942 | 117| 0x40 | 6 | 942 |

---

## Mensagens no Telegram

```
🔥 ALARME DE INCÊNDIO

📋 BOTOEIRA PAV 1
🔧 Tipo: ALARME
📍 Reg: 62  Bit: 4

Central: CIE 1125 | Intelbras
```

```
✅ NORMALIZADO

📋 BOTOEIRA PAV 1
🔧 Tipo: ALARME
📍 Reg: 62  Bit: 4

Central: CIE 1125 | Intelbras
```

---

## Componentes necessários

| Item | Opção 1 | Opção 2 |
|------|---------|---------|
| Mini roteador WiFi | ✅ Necessário (~R$ 60-80) | ❌ Não precisa |
| Cabo de rede RJ45 | ✅ Incluso no roteador | ✅ 1 cabo Cat5e |
| ESP32 | ✅ | ✅ |
| MAX3232 / fios extras | ❌ Não precisa | ❌ Não precisa |

---

## Bibliotecas necessárias (Arduino IDE)

| Biblioteca | Onde instalar |
|------------|---------------|
| ArduinoJson (Benoit Blanchon) | Gerenciar Bibliotecas |
| WiFi, WiFiUDP, HTTPClient | Incluso no core ESP32 |
