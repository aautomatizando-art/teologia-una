/*
 * ═══════════════════════════════════════════════════════════
 *  ESP32 Encoder Dashboard — Firmware v2.4
 * ═══════════════════════════════════════════════════════════
 *
 *  ENCODER PNP 360ppr
 *    Canal A    → GPIO 18  (INPUT_PULLUP)
 *    Canal B    → GPIO 19  (INPUT_PULLUP)
 *    Alimentação→ 5V / GND
 *
 *  POLIA
 *    Diâmetro   → 200 mm  (altere PULLEY_DIAM_MM se necessário)
 *    PPR        → 360     (altere PPR se o encoder for diferente)
 *    m/pulso    → π × 200 / (360 × 1000) ≈ 0.001745 m
 *
 *  ENTRADAS DIGITAIS  (botões físicos PNP: pressionado=1, solto=0)
 *    DIN1  Produzindo → GPIO 34  (input-only, sem pull-up)
 *    DIN2  Setup      → GPIO 35  (input-only, sem pull-up)
 *    DIN3  Falha      → GPIO 36  (input-only, sem pull-up)
 *
 *  SAÍDAS DIGITAIS
 *    DOUT1 Supervisor → GPIO 25
 *    DOUT2 Geral      → GPIO 26
 *
 *  COMUNICAÇÃO LOCAL
 *    HTTP REST  → http://<IP>/data    porta 80
 *    WebSocket  → ws://<IP>:81/ws     porta 81
 *    Broadcast JSON a cada 200 ms
 *
 *  SUPABASE (acesso de qualquer dispositivo/rede via Vercel)
 *    Preencha SUPA_URL e SUPA_KEY com os dados do projeto Supabase.
 *    O ESP32 faz HTTP POST a cada 500 ms atualizando a linha 'main'
 *    na tabela esp32_telemetry. O dashboard assina via Realtime.
 *    Deixe SUPA_URL vazio ("") para desabilitar.
 *
 *    Canal de comando reverso (dashboard → ESP32 via Supabase):
 *    o dashboard grava dout1_cmd/dout2_cmd na linha 'main'. O ESP32
 *    busca esses campos (GET) a cada 700 ms, aciona a saída física
 *    e limpa o campo (PATCH para null) para não reaplicar o comando.
 *    Necessário para acionar DOUT pelo celular/HTTPS, onde não há
 *    WebSocket direto na LAN.
 *
 *  BIBLIOTECAS NECESSÁRIAS  (instale via Library Manager)
 *    • WebSockets  by Markus Sattler   (WebSocketsServer)
 *    • ArduinoJson by Benoit Blanchon  (versão 6.x)
 *    WiFi, WebServer, HTTPClient, WiFiClientSecure já vêm no core
 *
 *  COMANDO WebSocket para controlar saídas (dashboard → ESP32, LAN):
 *    {"cmd":"setout","pin":1,"val":1}  → DOUT1 LIGADO
 *    {"cmd":"setout","pin":2,"val":0}  → DOUT2 DESLIGADO
 *
 *  JSON broadcast / POST (ESP32 → dashboard a cada 200/500 ms):
 *    {
 *      "id":"main",
 *      "pulses":1234,     "direction":1,
 *      "position_m":2.15, "speed_mpm":12.4,
 *      "din1":0,          "din2":0,  "din3":0,
 *      "dout1":0,         "dout2":0,
 *      "esp32_ts":12345
 *    }
 * ═══════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

// ─── CONFIGURAÇÃO DE REDE ───────────────────────────────────
const char* SSID     = "SUA_REDE_WIFI";   // ← altere aqui
const char* PASSWORD = "SUA_SENHA_WIFI";  // ← altere aqui

// ─── SUPABASE (deixe vazio para desabilitar) ────────────────
// Exemplo: "https://zcevjbtzucwpmioiqtis.supabase.co"
#define SUPA_URL  ""   // ← cole aqui o URL do projeto Supabase
#define SUPA_KEY  ""   // ← cole aqui a Anon Key do Supabase

// ─── POLIA / ENCODER ───────────────────────────────────────
#define PULLEY_DIAM_MM  200.0f   // ← diâmetro da polia em mm
#define PPR             360      // ← pulsos por revolução do encoder

#define M_PER_PULSE  (PI * PULLEY_DIAM_MM / (PPR * 1000.0f))

// ─── PINOS ─────────────────────────────────────────────────
#define PIN_A  18
#define PIN_B  19
#define DIN1   34
#define DIN2   35
#define DIN3   36
#define DOUT1  25
#define DOUT2  26

// ─── VARIÁVEIS GLOBAIS ─────────────────────────────────────
volatile long pulseCount = 0;
volatile int  direction  = 1;
bool dout1State = false;
bool dout2State = false;

unsigned long lastSpeedTs     = 0;
long          lastSpeedPulses = 0;
float         speedMpm        = 0.0f;

bool supaEnabled = false;
unsigned long lastSupaPost = 0;
unsigned long lastSupaPoll = 0;

WebServer        server(80);
WebSocketsServer wsServer(81);

// ─── INTERRUPÇÃO DO ENCODER ────────────────────────────────
void IRAM_ATTR onEncoderA() {
  bool a = digitalRead(PIN_A);
  bool b = digitalRead(PIN_B);
  direction   = (a == b) ? 1 : -1;
  pulseCount += direction;
}

// ─── ACIONAR SAÍDA FÍSICA ──────────────────────────────────
void applyOut(int pin, int val) {
  if (pin == 1) { dout1State = (val != 0); digitalWrite(DOUT1, dout1State ? HIGH : LOW); Serial.printf("DOUT1=%d\n", dout1State); }
  if (pin == 2) { dout2State = (val != 0); digitalWrite(DOUT2, dout2State ? HIGH : LOW); Serial.printf("DOUT2=%d\n", dout2State); }
}

// ─── PROCESSAR COMANDO SETOUT (WebSocket LAN) ──────────────
void processSetout(uint8_t* payload, size_t length) {
  StaticJsonDocument<128> cmd;
  if (deserializeJson(cmd, payload, length) != DeserializationError::Ok) return;
  const char* c = cmd["cmd"];
  if (!c || strcmp(c, "setout") != 0) return;
  applyOut(cmd["pin"] | 0, cmd["val"] | 0);
}

// ─── HANDLER WEBSOCKET LOCAL ───────────────────────────────
void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) processSetout(payload, length);
}

// ─── CALCULAR VELOCIDADE (m/min) ───────────────────────────
void updateSpeed() {
  unsigned long now = millis();
  unsigned long dt  = now - lastSpeedTs;
  if (dt >= 500) {
    long dp = pulseCount - lastSpeedPulses;
    speedMpm = (abs(dp) * M_PER_PULSE) / (dt / 60000.0f);
    lastSpeedPulses = pulseCount;
    lastSpeedTs     = now;
  }
}

// ─── MONTAR JSON ───────────────────────────────────────────
String buildJson(bool includeId) {
  StaticJsonDocument<384> doc;
  if (includeId) doc["id"] = "main";   // necessário para upsert no Supabase
  doc["pulses"]     = pulseCount;
  doc["direction"]  = direction;
  doc["position_m"] = (float)(pulseCount * M_PER_PULSE);
  doc["speed_mpm"]  = speedMpm;
  doc["din1"]       = digitalRead(DIN1);
  doc["din2"]       = digitalRead(DIN2);
  doc["din3"]       = digitalRead(DIN3);
  doc["dout1"]      = dout1State ? 1 : 0;
  doc["dout2"]      = dout2State ? 1 : 0;
  doc["esp32_ts"]   = millis();
  String out;
  serializeJson(doc, out);
  return out;
}

// ─── POST PARA SUPABASE ────────────────────────────────────
void postToSupabase() {
  WiFiClientSecure client;
  client.setInsecure();                   // Supabase já tem cert válido; setInsecure evita bundle de CA

  HTTPClient http;
  String url = String(SUPA_URL) + "/rest/v1/esp32_telemetry";
  if (!http.begin(client, url)) { http.end(); return; }

  http.addHeader("Content-Type",  "application/json");
  http.addHeader("apikey",        SUPA_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPA_KEY);
  http.addHeader("Prefer",        "resolution=merge-duplicates,return=minimal");

  String body = buildJson(true);
  int code = http.POST(body);
  http.end();

  if (code > 0) Serial.printf("[Supa] POST %d\n", code);
  else          Serial.printf("[Supa] Erro %d\n", code);
}

// ─── LIMPAR COMANDOS APLICADOS (dout*_cmd → null) ──────────
void clearSupabaseCommands(bool clear1, bool clear2) {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = String(SUPA_URL) + "/rest/v1/esp32_telemetry?id=eq.main";
  if (!http.begin(client, url)) { http.end(); return; }

  http.addHeader("Content-Type",  "application/json");
  http.addHeader("apikey",        SUPA_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPA_KEY);
  http.addHeader("Prefer",        "return=minimal");

  StaticJsonDocument<64> doc;
  if (clear1) doc["dout1_cmd"] = nullptr;
  if (clear2) doc["dout2_cmd"] = nullptr;
  String body;
  serializeJson(doc, body);

  http.PATCH(body);
  http.end();
}

// ─── BUSCAR COMANDOS DE SAÍDA NO SUPABASE ──────────────────
// Permite acionar DOUT1/DOUT2 pelo dashboard quando acessado via
// HTTPS/celular (sem WebSocket direto na LAN).
void pollSupabaseCommands() {
  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  String url = String(SUPA_URL) + "/rest/v1/esp32_telemetry?id=eq.main&select=dout1_cmd,dout2_cmd";
  if (!http.begin(client, url)) { http.end(); return; }

  http.addHeader("apikey",        SUPA_KEY);
  http.addHeader("Authorization", String("Bearer ") + SUPA_KEY);

  int code = http.GET();
  if (code == 200) {
    String body = http.getString();
    http.end();

    StaticJsonDocument<256> doc;
    if (deserializeJson(doc, body) == DeserializationError::Ok && doc.is<JsonArray>() && doc.size() > 0) {
      JsonObject row = doc[0];
      bool clear1 = !row["dout1_cmd"].isNull();
      bool clear2 = !row["dout2_cmd"].isNull();
      if (clear1) applyOut(1, row["dout1_cmd"].as<int>());
      if (clear2) applyOut(2, row["dout2_cmd"].as<int>());
      if (clear1 || clear2) clearSupabaseCommands(clear1, clear2);
    }
  } else {
    http.end();
  }
}

// ─── SETUP ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32 Encoder Dashboard v2.4 ===");
  Serial.printf("Polia: %.0f mm | PPR: %d | %.6f m/pulso\n",
                PULLEY_DIAM_MM, PPR, M_PER_PULSE);

  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_A), onEncoderA, CHANGE);

  pinMode(DIN1, INPUT);
  pinMode(DIN2, INPUT);
  pinMode(DIN3, INPUT);

  pinMode(DOUT1, OUTPUT); digitalWrite(DOUT1, LOW);
  pinMode(DOUT2, OUTPUT); digitalWrite(DOUT2, LOW);

  Serial.print("Conectando WiFi");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) { delay(300); Serial.print("."); }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  server.on("/data", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", buildJson(false));
  });
  server.begin();

  wsServer.begin();
  wsServer.onEvent(onWsEvent);

  supaEnabled = (strlen(SUPA_URL) > 0 && strlen(SUPA_KEY) > 0);
  if (supaEnabled) Serial.println("[Supa] Habilitado — POST 500ms / comandos DOUT 700ms");
  else             Serial.println("[Supa] Desabilitado (SUPA_URL vazio)");

  lastSpeedTs  = millis();
  lastSupaPost = millis();
  lastSupaPoll = millis();

  Serial.println("HTTP  :80  → http://" + WiFi.localIP().toString() + "/data");
  Serial.println("WS    :81  → ws://"   + WiFi.localIP().toString() + ":81/ws");
  Serial.println("=====================================");
}

// ─── LOOP ──────────────────────────────────────────────────
unsigned long lastBcast = 0;

void loop() {
  server.handleClient();
  wsServer.loop();
  updateSpeed();

  // Broadcast local (LAN WebSocket) a cada 200 ms
  if (millis() - lastBcast >= 200) {
    lastBcast = millis();
    String jsonLocal = buildJson(false);
    wsServer.broadcastTXT(jsonLocal);
  }

  // POST para Supabase a cada 500 ms (HTTPS é mais lento que WS local)
  if (supaEnabled && millis() - lastSupaPost >= 500) {
    lastSupaPost = millis();
    postToSupabase();
  }

  // Buscar comandos de saída (dout*_cmd) a cada 700 ms
  if (supaEnabled && millis() - lastSupaPoll >= 700) {
    lastSupaPoll = millis();
    pollSupabaseCommands();
  }
}
