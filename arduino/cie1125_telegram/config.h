#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID      "NomeDaSuaRede"
#define WIFI_PASSWORD  "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── CIE 1125 – Rede ───────────────────────────────
#define CIE_IP         "192.168.1.50"  // IP fixo configurado na central
#define CIE_PORT       502             // porta Modbus UDP (padrão Intelbras)
#define MODBUS_UNIT_ID 0x01            // ID escravo (padrão = 1)

// ── Polling ───────────────────────────────────────
#define POLL_INTERVAL_MS  2000         // consulta a cada 2 s
#define COOLDOWN_MS       60000UL      // pausa mín. entre msgs do mesmo evento

// ── Filtros ───────────────────────────────────────
#define ENVIAR_ALARME  true
#define ENVIAR_FALHA   true
#define ENVIAR_NORMAL  true

// ── MODO SCANNER ──────────────────────────────────
// Mude para TRUE para descobrir os registradores das botoeiras/detectores.
// Com SCANNER_MODE true:
//   1. Abra o Monitor Serial (115200 baud)
//   2. Acione uma botoeira na central
//   3. O ESP32 imprime qual registrador/bit mudou:
//      [SCANNER] Reg:62 Bit:4 Mask:0x10  → MUDOU 0→16
//   4. Anote esses valores e preencha o array DISPOSITIVOS[] abaixo
//   5. Mude SCANNER_MODE para false e regrave
#define SCANNER_MODE  false

// ── Mapa de dispositivos ──────────────────────────
// Preencha após rodar o SCANNER_MODE para descobrir os valores corretos.
//
// Fórmula confirmada no manual Intelbras CIE 1125 (Anexo A):
//   reg_addr  = codigo_evento / 8      (divisão inteira)
//   bit_mask  = 1 << (codigo_evento % 8)
//
// Exemplo confirmado: Falha Dispositivo 1 Loop 1 → reg 62, mask 0x10 (bit 4)
//   codigo_evento = 62*8 + 4 = 500
//
// Para alarme de incêndio o código de evento é diferente do de falha.
// Use o SCANNER_MODE para descobrir sem precisar do manual.

struct Dispositivo {
    uint16_t    regAddr;
    uint8_t     bitMask;
    const char* nome;
    const char* tipoEvento;   // "ALARME" ou "FALHA"
};

// Preencha com os valores descobertos pelo SCANNER_MODE:
const Dispositivo DISPOSITIVOS[] = {
    // { reg,  mask,   "nome",              "tipo"   }
    {  62, 0x10, "BOTOEIRA / DISP. 1",  "ALARME" },   // exemplo – ajuste!
    {   0, 0x00, nullptr,               nullptr  }    // sentinel
};
