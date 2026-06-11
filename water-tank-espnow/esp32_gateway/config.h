#ifndef CONFIG_H
#define CONFIG_H

// ─── WIFI ────────────────────────────────────────────────────────────────────────
#define WIFI_SSID  "SUA_REDE_WIFI"
#define WIFI_PASS  "SUA_SENHA_WIFI"

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
