#ifndef CONFIG_H
#define CONFIG_H

// ─── CONDOMINIO ──────────────────────────────────────────────────────────────────
// Identificador deste condominio (aparece na dashboard e nas mensagens do grupo).
// Use um nome diferente em cada gateway instalado!
#define CONDOMINIO_NOME  "Condominio Principal"

// ─── WIFI (configuracao no local via WiFiManager) ───────────────────────────────
// Nao precisa gravar SSID/senha no codigo: na primeira ligacao (ou se a rede
// salva nao for encontrada) o ESP32 cria o ponto de acesso abaixo. Conecte pelo
// celular, o portal abre sozinho, escolha a rede WiFi do condominio e digite a
// senha. Fica salvo na memoria flash do ESP32.
#define AP_CONFIG_NOME   "BombaIncendio-Setup"
#define AP_CONFIG_SENHA  "12345678"          // senha do AP de configuracao (min 8)
#define PORTAL_TIMEOUT_S 180                 // portal aberto por 3 min, depois reinicia

// ─── EVOLUTION API (WhatsApp no VPS Hostinger) ─────────────────────────────────
// IP do VPS (srv1745227.hstgr.cloud)
#define EVO_BASE_URL   "http://2.25.192.72:8080"

// Instancia conectada (verificada via /instance/fetchInstances)
#define EVO_INSTANCE   "escola-una-v2"

// API Key global da Evolution API (AUTHENTICATION_API_KEY)
// NAO comitar a chave real no GitHub — preencha apenas localmente!
#define EVO_APIKEY     "SUA_APIKEY_AQUI"

// Grupo do WhatsApp: "Gestao Condominio"
#define WHATS_GROUP_ID "120363407922496564@g.us"

// ─── SUPABASE (Dashboard web) ───────────────────────────────────────────────────
// URL do projeto: Supabase -> Settings -> API -> Project URL
#define SUPABASE_URL  "https://odnjbvsjqteqapppkkpc.supabase.co"

// Chave anon/public (chave publica, protegida por RLS — nunca use a service_role!)
#define SUPABASE_KEY  "eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJpc3MiOiJzdXBhYmFzZSIsInJlZiI6Im9kbmpidnNqcXRlcWFwcHBra3BjIiwicm9sZSI6ImFub24iLCJpYXQiOjE3ODA0Mjk4NDQsImV4cCI6MjA5NjAwNTg0NH0.YWEzDGBl-045TPfp66drFkFwo8eeILxEu6Ex8n3pO2M"

// ─── PINOS ESP32 BOMBAS DE INCENDIO ──────────────────────────────────
// Contatos secos (rele) ligados ao GND quando ativos -> usa INPUT_PULLUP
// (pino em HIGH = inativo, pino em LOW = ativo)
#define ENTRADA1_PIN    27   // ENTRADA 1: Bomba principal de incendio ligada
#define ENTRADA2_PIN    14   // ENTRADA 2: Falha na bomba de incendio
#define ENTRADA3_PIN    13   // ENTRADA 3: Bomba reserva de incendio ligada

// ─── INTERVALO DE ENVIO (heartbeat para a dashboard) ─────────────────
#define INTERVALO_ENVIO_MS  30000UL   // envia status a cada 30 segundos

#endif
