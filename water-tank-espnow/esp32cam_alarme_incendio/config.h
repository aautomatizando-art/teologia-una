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
#define AP_CONFIG_NOME   "AlarmeIncendio-Setup"
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

// ─── PINOS ESP32-CAM (AI-Thinker) — ENTRADAS DIGITAIS ───────────────────────────
// O modulo AI-Thinker ESP32-CAM usa quase todos os GPIOs para a camera OV2640,
// o cartao SD e/ou pinos de boot/strapping. So sobram 2 GPIOs realmente livres
// para uso geral: GPIO13 e GPIO15.
//
// NAO USE para entradas digitais:
//   - GPIO0  e GPIO2  -> pinos de modo de boot (GPIO0 tambem e XCLK da camera)
//   - GPIO4  e GPIO33 -> LEDs onboard (flash branco e LED vermelho indicador)
//   - GPIO12          -> strapping de tensao da flash (MTDI); se ficar em nivel
//                        errado no boot, a flash pode nao ser detectada
//   - GPIO5,18,19,21,22,23,25,26,27,32,34,35,36,39 -> todos usados pela camera
//     (SIOD/SIOC/VSYNC/HREF/PCLK/Y2-Y9/PWDN), GPIO34/35/36/39 sao so leitura (ADC)
//
// Por isso, usamos GPIO13 e GPIO15 (ambos com INPUT_PULLUP, contato seco para
// GND = ativo), iguais ao padrao das "entradas" dos demais ESP32 do projeto.
#define ENTRADA1_PIN    13   // ENTRADA 1: Alarme acionado (central de incendio)
#define ENTRADA2_PIN    15   // ENTRADA 2: Central com avaria/falha

// ─── CAMERA (OV2640 onboard, modulo AI-Thinker ESP32-CAM) ───────────────────────
// Pinagem fixa do modulo — nao alterar. Definida em esp32cam_alarme_incendio.ino.

// ─── INTERVALOS ──────────────────────────────────────────────────────────────────
// Heartbeat: envia o status atual ao Supabase mesmo sem mudanca de estado,
// para a dashboard nao mostrar "sem comunicacao".
#define INTERVALO_MEDICAO_MS  30000UL    // 30 segundos

// Enquanto o status NAO estiver "normal" (alarme ou avaria), tira uma nova
// foto da central periodicamente para manter a foto da dashboard atualizada.
#define FOTO_INTERVALO_MS  3600000UL     // 1 hora

#endif
