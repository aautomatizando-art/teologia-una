#ifndef CONFIG_H
#define CONFIG_H

// ─── WIFI ────────────────────────────────────────────────────────────────────────
#define WIFI_SSID  "SUA_REDE_WIFI"
#define WIFI_PASS  "SUA_SENHA_WIFI"

// ─── EVOLUTION API (WhatsApp no seu VPS Hostinger) ────────────────────────────
// Base URL: IP do seu VPS + porta da Evolution API
#define EVO_BASE_URL   "http://SEU_IP_VPS:8080"

// Nome da instancia criada na Evolution API
// (a mesma usada no projeto do leitor biometrico)
#define EVO_INSTANCE   "SUA_INSTANCIA"

// API Key global da Evolution API
// (variavel AUTHENTICATION_API_KEY do docker-compose / .env)
#define EVO_APIKEY     "SUA_APIKEY"

// ID do GRUPO do WhatsApp que recebera os alertas
// Formato: 120363xxxxxxxxxxx@g.us
// Para descobrir, rode no terminal do VPS:
//   curl -H "apikey: SUA_APIKEY" \
//     "http://localhost:8080/group/fetchAllGroups/SUA_INSTANCIA?getParticipants=false"
#define WHATS_GROUP_ID "120363xxxxxxxxxxx@g.us"

// ─── LIMITES DE ALERTA ──────────────────────────────────────────────────────────
#define NIVEL_ALERTA  20   // % abaixo disto -> alerta de nivel baixo
#define NIVEL_OK      80   // % acima disto  -> aviso de caixa cheia

// ─── STATUS PERIODICO ───────────────────────────────────────────────────────────
// 0 = desativado (so envia alertas). Ex: 3600000UL = status a cada 1 hora
#define INTERVALO_STATUS_MS  0UL

// ─── TIMEOUT SEM DADOS ──────────────────────────────────────────────────────────
// Alerta se o sensor ficar mais que X ms sem enviar dados
#define TIMEOUT_SENSOR_MS  120000UL   // 2 minutos sem dados = alerta

#endif
