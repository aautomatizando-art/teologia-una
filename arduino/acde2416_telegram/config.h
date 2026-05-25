#pragma once

// ── WiFi ──────────────────────────────────────────
#define WIFI_SSID      "NomeDaSuaRede"
#define WIFI_PASSWORD  "SuaSenhaWiFi"

// ── Telegram ──────────────────────────────────────
#define TELEGRAM_TOKEN   "SEU_TOKEN_AQUI"
#define TELEGRAM_CHAT_ID "-1001234567890"

// ── Serial RS232 ──────────────────────────────────
#define BAUD_CENTRAL  9600     // baud padrão ASCAEL — tente também 2400, 4800, 19200
#define PIN_RX2       16       // ESP32 GPIO16 ← MAX3232 R1OUT
#define PIN_TX2       17       // ESP32 GPIO17 → MAX3232 T1IN (não usado, pode deixar)

// ── Cooldown e filtros ────────────────────────────
#define COOLDOWN_MS    60000UL  // pausa mínima entre msgs do mesmo evento (ms)
#define ENVIAR_ALARME  true     // notifica alarme de incêndio
#define ENVIAR_FALHA   true     // notifica falha
#define ENVIAR_NORMAL  true     // notifica normalização/retorno

// ── RAW MODE ─────────────────────────────────────
// Mude para TRUE para capturar e exibir tudo que a central envia via RS232.
// Use para descobrir o formato das mensagens antes de configurar o parser.
//
// Como usar:
//   1. Conecte o MAX3232 entre a RS232 DB9 da ACDE 24/16 e o ESP32
//   2. Ative RAW_MODE true e grave no ESP32
//   3. Abra o Monitor Serial (115200 baud)
//   4. Acione uma botoeira ou gere um alarme na central
//   5. Copie a saída exibida e informe para ajustar o parser
//   6. Após identificar o formato, defina RAW_MODE false e regrave
#define RAW_MODE  true

// ── Mapeamento de zonas (opcional) ───────────────
// Preencha com os nomes das zonas da sua instalação.
// Se a central enviar apenas o número da zona sem descrição, esses
// nomes serão adicionados automaticamente na mensagem do Telegram.
//
// Formato: { numero_da_zona, "descrição" }
// Deixe o array vazio ({ { 0, nullptr } }) para desativar.

struct ZonaInfo {
    int         numero;
    const char* descricao;
};

const ZonaInfo ZONAS[] = {
    {  1, "ZONA 1 – PAV TÉRREO"    },
    {  2, "ZONA 2 – PAV SUPERIOR"  },
    {  3, "ZONA 3 – SUBSOLO"       },
    // adicione quantas zonas precisar
    {  0, nullptr }   // sentinel – não remova
};
