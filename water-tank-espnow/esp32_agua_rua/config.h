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
#define AP_CONFIG_NOME   "AguaRua-Setup"
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

// ─── PINOS ESP32 AGUA DA RUA ──────────────────────────────────────────
// Sensor de fluxo por efeito Hall (ex: YF-S201) ligado a um pino capaz de
// interrupcao. O sensor gera pulsos a uma frequencia proporcional a vazao.
#define FLUXO_PIN    27   // Sensor de fluxo (hall effect, ex: YF-S201)

// ─── CALIBRACAO DO SENSOR DE FLUXO ────────────────────────────────────
// Pulsos por segundo, por litro/minuto de vazao. Depende do MODELO do
// sensor (diametro, fabricante etc.) — o valor abaixo (7.5) e o tipico
// do YF-S201 (1/2"), mas DEVE SER CALIBRADO no local: meca um volume
// conhecido escoando em um tempo conhecido e ajuste este fator ate o
// fluxo_lpm calculado bater com a vazao real.
#define FLUXO_FATOR_CALIBRACAO  7.5

// Vazao minima (L/min) para considerar que ha fluxo de agua ativo.
// Abaixo disto, fluxo_ativo = false.
#define FLUXO_MINIMO_LPM  0.5

// ─── INTERVALO DE MEDICAO / ENVIO (heartbeat para a dashboard) ────────
// A cada este intervalo: calcula a vazao (L/min) com base nos pulsos
// acumulados, zera o contador e envia o status para o Supabase.
#define INTERVALO_MEDICAO_MS  30000UL   // 30 segundos

// ─── TIMEOUT SEM FLUXO ─────────────────────────────────────────────────
// Tempo continuo sem fluxo (ms) para disparar o alerta de "agua da rua
// parou". A agua da rua costuma chegar de forma intermitente (em alguns
// horarios do dia/noite), entao este valor deve ser AJUSTADO conforme o
// padrao de uso/abastecimento de cada condominio para evitar falsos
// alarmes (ex: aumentar se a concessionaria so abastece a noite).
#define FLUXO_TIMEOUT_MS  1800000UL   // 30 minutos

#endif
