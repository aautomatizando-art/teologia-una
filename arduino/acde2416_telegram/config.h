#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID      "NomeDaSuaRede"
#define WIFI_PASSWORD  "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── Serial (pino 39 do PIC18F4520) ───────────────
#define BAUD_CENTRAL  4800    // confirmado
#define PIN_RX2       16      // ESP32 GPIO16 ← pino 39 do PIC (via resistor 1kΩ)
#define PIN_TX2       17      // não usado

// ── Cooldown e filtros ────────────────────────────
#define COOLDOWN_MS    60000UL
#define ENVIAR_NORMAL  true   // envia "normalizado" quando alarme cessa

// ── RAW MODE ─────────────────────────────────────
// true  → imprime cada byte no Monitor Serial (diagnóstico)
// false → parser ativo, envia notificações ao Telegram
#define RAW_MODE  false

// ── Mapa de dispositivos ──────────────────────────
// Como descobrir o endereço de cada botoeira:
//   1. Grave com RAW_MODE false
//   2. Acione cada botoeira
//   3. O Telegram mostra "Endereço XX" — anote esse número
//   4. Preencha abaixo com o endereço e o nome do local
//
// O endereço depende das chaves DIP da botoeira:
//   Chave 1 ligada = bit 0 → end. 1  (byte 0xFE)
//   Chave 2 ligada = bit 1 → end. 2  (byte 0xFD)
//   Chave 3 ligada = bit 2 → end. 3  (byte 0xFB)
//   Chave 1+2+3    = bits 0,1,2 → end. 7  (byte 0xBF)

struct Dispositivo {
    int         endereco;   // 1–16
    const char* nome;
};

const Dispositivo DISPOSITIVOS[] = {
    //  { endereco, "nome do local"               }
    {  7, "BOTOEIRA PAV T\xC3\x89RREO"           },  // exemplo — ajuste!
    {  0, nullptr }   // sentinel — não remova
};
