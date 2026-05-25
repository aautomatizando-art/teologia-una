#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID      "NomeDaSuaRede"
#define WIFI_PASSWORD  "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── CIE 1125 – Rede ───────────────────────────────
// Configure IP fixo na central (Menu → Rede → Modo Fixo)
// Padrão de fábrica: DHCP (descubra o IP no roteador após conectar)
#define CIE_IP    "192.168.1.50"   // ajuste para o IP da sua central
#define CIE_PORT  502              // porta Modbus UDP (padrão)
#define MODBUS_UNIT_ID  0x01       // ID escravo da central (padrão = 1)

// ── Polling ───────────────────────────────────────
#define POLL_INTERVAL_MS  2000    // consulta a central a cada 2 s
#define COOLDOWN_MS       60000UL // pausa entre mensagens do mesmo evento

// ── Filtros de evento ─────────────────────────────
#define ENVIAR_ALARME    true
#define ENVIAR_FALHA     true
#define ENVIAR_NORMAL    true   // normalização (evento limpo)

// ── Mapa de dispositivos ──────────────────────────
// Cada dispositivo é identificado por: registrador Modbus + máscara de bit
//
// FÓRMULA (do manual CIE 1125 – Anexo A):
//   reg_addr = codigo_evento / 8
//   bit_mask = 1 << (codigo_evento % 8)
//
// Exemplos verificados no manual Intelbras (Anexo A):
//   Código 19  → reg 2,   máscara 0x08
//   Código 942 → reg 117, máscara 0x40
//   Código 1500→ reg 187, máscara 0x10
//
// Registradores de resumo (sem máscara):
//   reg 200 = total de alarmes ativos (uint16)
//   reg 202 = total de falhas ativas  (uint16)
//
// CONSULTE O ANEXO A DO MANUAL para obter os códigos exatos
// de cada botoeira/detector da sua instalação e preencha abaixo.

struct Dispositivo {
    uint16_t    regAddr;   // endereço do registrador
    uint8_t     bitMask;   // máscara do bit (ex: 0x01, 0x02, 0x04...)
    const char* nome;      // descrição livre
    const char* tipoEvento;// "ALARME" ou "FALHA"
};

// Preencha com os dispositivos da sua instalação.
// Deixe a última linha com nullptr para indicar fim da lista.
const Dispositivo DISPOSITIVOS[] = {
    // { reg, mask, "descrição",   "tipo"   }
    // Substitua pelos valores do Anexo A do seu manual:
    { 2,   0x08, "BOTOEIRA PAV 1",     "ALARME" },
    { 2,   0x10, "BOTOEIRA PAV 2",     "ALARME" },
    { 3,   0x01, "DETECTOR SALA 101",  "ALARME" },
    { 3,   0x02, "DETECTOR CORREDOR",  "ALARME" },
    { 117, 0x40, "MODULO SAIDA 1",     "FALHA"  },
    { 0,   0x00, nullptr,              nullptr  }   // sentinel – não remover
};
