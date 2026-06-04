/*
 * ═══════════════════════════════════════════════════════════
 *  Quiz Bíblico Pentecostal — Firmware ESP32  v1.0
 * ═══════════════════════════════════════════════════════════
 *
 *  FUNÇÃO
 *    • 5 botões de resposta (1 a 5)  → o jogador sorteado escolhe a alternativa
 *    • 8 saídas de jogador (1 a 8)   → o sorteio aciona a saída do jogador
 *                                       (LED / relé / sinaleiro na bancada)
 *    • 1 buzzer                      → som de acerto / erro
 *
 *  BOTÕES DE RESPOSTA  (INPUT_PULLUP — ligar botão entre o pino e o GND)
 *    BTN1 → GPIO 13
 *    BTN2 → GPIO 12
 *    BTN3 → GPIO 14
 *    BTN4 → GPIO 27
 *    BTN5 → GPIO 15
 *
 *  SAÍDAS DE JOGADOR  (nível alto = acionado)
 *    OUT1 → GPIO 16    OUT5 → GPIO 21
 *    OUT2 → GPIO 17    OUT6 → GPIO 22
 *    OUT3 → GPIO 18    OUT7 → GPIO 23
 *    OUT4 → GPIO 19    OUT8 → GPIO 4
 *
 *  BUZZER          → GPIO 25
 *  LED de status   → GPIO 2  (onboard)
 *
 *  COMUNICAÇÃO
 *    HTTP REST  → http://<IP>/data    porta 80
 *    WebSocket  → ws://<IP>:81/ws     porta 81
 *
 *  COMANDOS  (dashboard → ESP32)
 *    {"cmd":"sorteio","player":3}   → liga SÓ a saída do jogador 3
 *    {"cmd":"alloff"}               → desliga todas as saídas de jogador
 *    {"cmd":"resultado","ok":true}  → toca som de ACERTO (parabéns)
 *    {"cmd":"resultado","ok":false} → toca som de ERRO (castigo)
 *    {"cmd":"buzzer","ms":200,"freq":880}
 *
 *  EVENTOS  (ESP32 → dashboard)
 *    {"event":"button","btn":2,"ts":12345}   ao pressionar um botão
 *    {"buttons":[0,1,0,0,0],"player":3,"timestamp":...}  estado a cada 200 ms
 *
 *  BIBLIOTECAS  (Library Manager)
 *    • WebSockets  by Markus Sattler
 *    • ArduinoJson by Benoit Blanchon (6.x)
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

// ─── PINOS ──────────────────────────────────────────────────
const uint8_t BTN[5]    = {13, 12, 14, 27, 15};
const uint8_t PLAYER[8] = {16, 17, 18, 19, 21, 22, 23, 4};
#define BUZZER  25
#define LED_ST  2

// ─── ESTADO ─────────────────────────────────────────────────
bool          btnState[5]   = {false,false,false,false,false};
bool          btnLast[5]    = {false,false,false,false,false};
unsigned long btnDebounce[5]= {0,0,0,0,0};
int           activePlayer  = 0;          // 0 = nenhum
const unsigned long DEBOUNCE_MS = 40;

WebServer        server(80);
WebSocketsServer wsServer(81);

// ─── BUZZER ─────────────────────────────────────────────────
void beep(int freq, int ms) {
  tone(BUZZER, freq, ms);
  delay(ms);
}
void somAcerto() {                 // arpejo alegre (parabéns)
  beep(523, 120); beep(659, 120); beep(784, 120); beep(1047, 220);
  noTone(BUZZER);
}
void somErro() {                   // descida grave (castigo)
  beep(330, 180); beep(262, 180); beep(196, 350);
  noTone(BUZZER);
}

// ─── SAÍDAS DE JOGADOR ──────────────────────────────────────
void allPlayersOff() {
  for (int i = 0; i < 8; i++) digitalWrite(PLAYER[i], LOW);
  activePlayer = 0;
}
void setPlayer(int p) {            // p = 1..8
  allPlayersOff();
  if (p >= 1 && p <= 8) {
    digitalWrite(PLAYER[p - 1], HIGH);
    activePlayer = p;
  }
}

// ─── BROADCAST DE ESTADO ────────────────────────────────────
void sendState() {
  StaticJsonDocument<256> doc;
  JsonArray b = doc.createNestedArray("buttons");
  for (int i = 0; i < 5; i++) b.add(btnState[i] ? 1 : 0);
  doc["player"]    = activePlayer;
  doc["timestamp"] = millis();
  char buf[256];
  size_t n = serializeJson(doc, buf);
  wsServer.broadcastTXT(buf, n);
}

void sendButtonEvent(int btn) {
  StaticJsonDocument<96> doc;
  doc["event"] = "button";
  doc["btn"]   = btn;             // 1..5
  doc["ts"]    = millis();
  char buf[96];
  size_t n = serializeJson(doc, buf);
  wsServer.broadcastTXT(buf, n);
}

// ─── HANDLER WEBSOCKET (comandos do dashboard) ──────────────
void onWsEvent(uint8_t num, WStype_t type, uint8_t* payload, size_t length) {
  if (type != WStype_TEXT) return;
  StaticJsonDocument<160> cmd;
  if (deserializeJson(cmd, payload, length) != DeserializationError::Ok) return;

  const char* c = cmd["cmd"];
  if (!c) return;

  if (strcmp(c, "sorteio") == 0) {
    int p = cmd["player"] | 0;
    setPlayer(p);
    Serial.printf("Sorteio → jogador %d\n", p);
    sendState();
  }
  else if (strcmp(c, "alloff") == 0) {
    allPlayersOff();
    sendState();
  }
  else if (strcmp(c, "resultado") == 0) {
    bool ok = cmd["ok"] | false;
    if (ok) somAcerto(); else somErro();
  }
  else if (strcmp(c, "buzzer") == 0) {
    int ms   = cmd["ms"]   | 200;
    int freq = cmd["freq"] | 880;
    beep(freq, ms);
    noTone(BUZZER);
  }
}

// ─── HTTP /data (fallback de polling) ───────────────────────
void handleData() {
  StaticJsonDocument<256> doc;
  JsonArray b = doc.createNestedArray("buttons");
  for (int i = 0; i < 5; i++) b.add(btnState[i] ? 1 : 0);
  doc["player"]    = activePlayer;
  doc["timestamp"] = millis();
  String out;
  serializeJson(doc, out);
  server.sendHeader("Access-Control-Allow-Origin", "*");
  server.send(200, "application/json", out);
}

// ─── SETUP ──────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 5; i++) pinMode(BTN[i], INPUT_PULLUP);
  for (int i = 0; i < 8; i++) { pinMode(PLAYER[i], OUTPUT); digitalWrite(PLAYER[i], LOW); }
  pinMode(BUZZER, OUTPUT);
  pinMode(LED_ST, OUTPUT);

  WiFi.begin(SSID, PASSWORD);
  Serial.printf("\nConectando a %s", SSID);
  while (WiFi.status() != WL_CONNECTED) { delay(400); Serial.print("."); digitalWrite(LED_ST, !digitalRead(LED_ST)); }
  digitalWrite(LED_ST, HIGH);
  Serial.printf("\n✓ WiFi conectado — IP: %s\n", WiFi.localIP().toString().c_str());

  server.on("/data", handleData);
  server.begin();
  wsServer.begin();
  wsServer.onEvent(onWsEvent);

  beep(880, 100); noTone(BUZZER);   // bip de boot
}

// ─── LOOP ───────────────────────────────────────────────────
unsigned long lastBroadcast = 0;

void loop() {
  server.handleClient();
  wsServer.loop();

  // leitura dos botões com debounce (INPUT_PULLUP → pressionado = LOW)
  for (int i = 0; i < 5; i++) {
    bool pressed = (digitalRead(BTN[i]) == LOW);
    if (pressed != btnLast[i] && (millis() - btnDebounce[i]) > DEBOUNCE_MS) {
      btnDebounce[i] = millis();
      btnLast[i]     = pressed;
      btnState[i]    = pressed;
      if (pressed) {
        sendButtonEvent(i + 1);
        Serial.printf("Botão %d\n", i + 1);
      }
    }
  }

  // broadcast de estado a cada 200 ms
  if (millis() - lastBroadcast > 200) {
    lastBroadcast = millis();
    sendState();
  }
}
