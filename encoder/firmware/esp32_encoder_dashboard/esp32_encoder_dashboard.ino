/*
 * ═══════════════════════════════════════════════════════════
 *  ESP32 Encoder Dashboard — Firmware v2.1
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
 *  ENTRADAS DIGITAIS  (sinal PNP: 1 = ativo)
 *    DIN1  Produzindo → GPIO 34  (input-only, sem pull-up)
 *    DIN2  Setup      → GPIO 35  (input-only, sem pull-up)
 *    DIN3  Falha      → GPIO 36  (input-only, sem pull-up)
 *
 *  SAÍDAS DIGITAIS
 *    DOUT1 Supervisor → GPIO 25
 *    DOUT2 Geral      → GPIO 26
 *
 *  COMUNICAÇÃO
 *    HTTP REST  → http://<IP>/data    porta 80
 *    WebSocket  → ws://<IP>:81/ws     porta 81
 *    Broadcast JSON a cada 200 ms
 *
 *  BIBLIOTECAS NECESSÁRIAS  (instale via Library Manager)
 *    • WebSockets  by Markus Sattler   (WebSocketsServer)
 *    • ArduinoJson by Benoit Blanchon  (versão 6.x)
 *    WiFi e WebServer já vêm no ESP32 Arduino core
 *
 *  COMANDO WebSocket para controlar saídas (dashboard → ESP32):
 *    {"cmd":"setout","pin":1,"val":1}  → DOUT1 LIGADO
 *    {"cmd":"setout","pin":1,"val":0}  → DOUT1 DESLIGADO
 *    {"cmd":"setout","pin":2,"val":1}  → DOUT2 LIGADO
 *    {"cmd":"setout","pin":2,"val":0}  → DOUT2 DESLIGADO
 *
 *  JSON broadcast (ESP32 → dashboard a cada 200 ms):
 *    {
 *      "pulses":1234,     "direction":1,
 *      "position_m":2.15, "speed_mpm":12.4,
 *      "din1":0,          "din2":0,  "din3":0,
 *      "dout1":0,         "dout2":0,
 *      "timestamp":12345
 *    }
 * ═══════════════════════════════════════════════════════════
 */

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <WebSocketsServer.h>
#include <ArduinoJson.h>

// ─── CONFIGURAÇÃO DE REDE ───────────────────────────────────
const char* SSID     = "SUA_REDE_WIFI";   // ← altere aqui
const char* PASSWORD = "SUA_SENHA_WIFI";  // ← altere aqui

// ─── POLIA / ENCODER ───────────────────────────────────────
#define PULLEY_DIAM_MM  200.0f   // ← diâmetro da polia em mm
#define PPR             360      // ← pulsos por revolução do encoder

// metros por pulso (calculado em tempo de compilação)
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

// Cálculo de velocidade
unsigned long lastSpeedTs  = 0;
long          lastSpeedPulses = 0;
float         speedMpm     = 0.0f;   // metros por minuto

WebServer        server(80);
WebSocketsServer wsServer(81);

// ─── INTERRUPÇÃO DO ENCODER ────────────────────────────────
void IRAM_ATTR onEncoderA() {
  bool a = digitalRead(PIN_A);
  bool b = digitalRead(PIN_B);
  direction   = (a == b) ? 1 : -1;
  pulseCount += direction;
}

// ─── HANDLER WEBSOCKET ─────────────────────────────────────
void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type == WStype_TEXT) {
    StaticJsonDocument<128> cmd;
    DeserializationError err = deserializeJson(cmd, payload, length);
    if (err == DeserializationError::Ok) {
      const char* c = cmd["cmd"];
      if (c && strcmp(c, "setout") == 0) {
        int pin = cmd["pin"] | 0;
        int val = cmd["val"] | 0;
        if (pin == 1) {
          dout1State = (val != 0);
          digitalWrite(DOUT1, dout1State ? HIGH : LOW);
          Serial.printf("DOUT1 = %d\n", dout1State ? 1 : 0);
        }
        if (pin == 2) {
          dout2State = (val != 0);
          digitalWrite(DOUT2, dout2State ? HIGH : LOW);
          Serial.printf("DOUT2 = %d\n", dout2State ? 1 : 0);
        }
      }
    }
  }
}

// ─── CALCULAR VELOCIDADE (m/min) ───────────────────────────
void updateSpeed() {
  unsigned long now = millis();
  unsigned long dt  = now - lastSpeedTs;
  if (dt >= 500) {                              // atualiza a cada 500 ms
    long dp = pulseCount - lastSpeedPulses;
    speedMpm = (abs(dp) * M_PER_PULSE) / (dt / 60000.0f);
    lastSpeedPulses = pulseCount;
    lastSpeedTs     = now;
  }
}

// ─── MONTAR JSON ───────────────────────────────────────────
String buildJson() {
  StaticJsonDocument<320> doc;
  doc["pulses"]     = pulseCount;
  doc["direction"]  = direction;
  doc["position_m"] = serialized(String(pulseCount * M_PER_PULSE, 4));
  doc["speed_mpm"]  = serialized(String(speedMpm, 2));
  doc["din1"]       = digitalRead(DIN1);
  doc["din2"]       = digitalRead(DIN2);
  doc["din3"]       = digitalRead(DIN3);
  doc["dout1"]      = dout1State ? 1 : 0;
  doc["dout2"]      = dout2State ? 1 : 0;
  doc["timestamp"]  = millis();
  String out;
  serializeJson(doc, out);
  return out;
}

// ─── SETUP ─────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n=== ESP32 Encoder Dashboard v2.1 ===");
  Serial.printf("Polia: %.0f mm | PPR: %d | %.6f m/pulso\n",
                PULLEY_DIAM_MM, PPR, M_PER_PULSE);

  // Encoder
  pinMode(PIN_A, INPUT_PULLUP);
  pinMode(PIN_B, INPUT_PULLUP);
  attachInterrupt(digitalPinToInterrupt(PIN_A), onEncoderA, CHANGE);

  // Entradas digitais (GPIO 34/35/36 = input-only, sem pull-up interno)
  pinMode(DIN1, INPUT);
  pinMode(DIN2, INPUT);
  pinMode(DIN3, INPUT);

  // Saídas digitais (iniciam desligadas)
  pinMode(DOUT1, OUTPUT); digitalWrite(DOUT1, LOW);
  pinMode(DOUT2, OUTPUT); digitalWrite(DOUT2, LOW);

  // Conectar WiFi
  Serial.print("Conectando WiFi");
  WiFi.begin(SSID, PASSWORD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(300);
    Serial.print(".");
  }
  Serial.println();
  Serial.print("IP: ");
  Serial.println(WiFi.localIP());

  // Endpoint HTTP /data
  server.on("/data", HTTP_GET, []() {
    server.sendHeader("Access-Control-Allow-Origin", "*");
    server.send(200, "application/json", buildJson());
  });

  server.begin();
  wsServer.begin();
  wsServer.onEvent(onWsEvent);

  lastSpeedTs = millis();

  Serial.println("HTTP  :80  → http://" + WiFi.localIP().toString() + "/data");
  Serial.println("WS    :81  → ws://"  + WiFi.localIP().toString() + ":81/ws");
  Serial.println("=====================================");
}

// ─── LOOP ──────────────────────────────────────────────────
unsigned long lastBcast = 0;

void loop() {
  server.handleClient();
  wsServer.loop();
  updateSpeed();

  if (millis() - lastBcast >= 200) {
    lastBcast = millis();
    wsServer.broadcastTXT(buildJson());
  }
}
