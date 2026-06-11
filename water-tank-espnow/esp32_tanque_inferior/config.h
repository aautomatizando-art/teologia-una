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
#define AP_CONFIG_NOME   "TanqueInferior-Setup"
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

// ─── PINOS ESP32 TANQUE INFERIOR ─────────────────────────────────────
#define TRIG_PIN        32   // JSN-SR04T TRIG
#define ECHO_PIN        33   // JSN-SR04T ECHO
#define LED_PIN          2   // LED onboard (pisca apos cada envio)

// ─── ENTRADAS DIGITAIS (sinais do quadro/inversor da bomba) ──────────
// Contatos secos (rele) ligados ao GND quando ativos -> usa INPUT_PULLUP
// (pino em HIGH = inativo, pino em LOW = ativo)
// OBS: este no NAO possui "Entrada 1" (Bomba ligou) — esse sinal so existe
// no sensor do tanque superior.
#define ENTRADA2_PIN    14   // ENTRADA 2: Bomba falhou
#define ENTRADA3_PIN    13   // ENTRADA 3: Falha no inversor
#define ENTRADA4_PIN     4   // ENTRADA 4: Painel sem energia (sem rede CA)

// ─── TEMPERATURA (DS18B20 via OneWire) ───────────────────────────────
#define TEMP_PIN        15   // barramento OneWire do sensor DS18B20

// ─── VIBRACAO (sensor analogico) ─────────────────────────────────────
#define VIBRACAO_PIN    35   // ADC1 (pino somente leitura) — saida analogica do sensor

// ─── CALIBRACAO DA CAIXA ─────────────────────────────────────────────
// Distancia (cm) do sensor ate a superficie da agua:
//
//  [Sensor na tampa]
//     |  <- DIST_CAIXA_CHEIA (15cm) => caixa cheia (100%)
//     |
//     ~  nivel da agua
//     |
//     |  <- DIST_CAIXA_VAZIA (90cm) => caixa vazia (0%)
//     |
//  [Fundo]
//
#define DIST_CAIXA_VAZIA  90   // cm: nivel baixo (caixa vazia)
#define DIST_CAIXA_CHEIA  15   // cm: nivel cheio (caixa cheia)

// ─── LIMITES DE ALERTA ──────────────────────────────────────────────────────────
#define NIVEL_ALERTA  20   // % abaixo disto -> alerta de nivel baixo
#define NIVEL_OK      80   // % acima disto  -> aviso de caixa abastecida

#define TEMP_ALERTA_C   60     // alerta se temperatura da bomba ultrapassar este valor (°C)
#define VIBRACAO_LIMIAR 2500   // leitura ADC (0-4095) acima disto = vibracao excessiva

// ─── INTERVALO DE MEDICAO ─────────────────────────────────────────────
#define INTERVALO_MEDICAO_MS  30000UL   // mede e envia a cada 30 segundos

#endif
