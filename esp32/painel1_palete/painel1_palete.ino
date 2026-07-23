/*
  Painel 1 — Registro de Palete por Botão
  Hardware: ESP32 Dev Board

  Pinos:
    GPIO 2  → Saída 1: Luz Vermelha — acende ao ligar (sem WiFi)
    GPIO 4  → Saída 2: Luz Amarela  — acende ao conectar WiFi (estado idle)
    GPIO 5  → Saída 3: Sirene       — pulsa 2 s após registrar palete
    GPIO 18 → Saída 4: Luz Verde    — pulsa 2 s após registrar palete
    GPIO 15 → Botão (GND no outro terminal, usa INPUT_PULLUP)

  Comportamento:
    1. Energizou      → Saída 1 (vermelho) ON fixo, demais OFF
    2. WiFi ok        → Saída 1 OFF, Saída 2 (amarelo) ON
    3. Perda de WiFi  → Saída 2 OFF, Saída 1 (vermelho) ON; reconecta e volta ao passo 2
    4. Botão 2 s      → Registra palete, Saída 2 OFF,
                        Saída 3 (sirene) + Saída 4 (verde) ON por 2 s,
                        depois desligam e Saída 2 (amarelo) volta ON

  Relés ativo-LOW: LOW = relé acionado / ligado
                   HIGH = relé desligado

  Dependências (Library Manager):
    - ArduinoJson  (v6+)  — Benoit Blanchon
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ── Configurações ──────────────────────────────────────────────────────────
const char* SSID       = "iPhone de Alexsandro";
const char* SENHA_WIFI = "galaction";
const char* BASE_URL   = "https://dashboard-producao-two.vercel.app";

// ── Identidade deste painel ─────────────────────────────────────────────────
//    Para clonar para outro painel, altere apenas estas duas linhas:
#define PAINEL_NUM  1          // 1, 2 ou 3
#define LINHAS_STR  "1,2"      // "1,2" | "3,4" | "5,6"

// ── Pinos ──────────────────────────────────────────────────────────────────
#define PIN_SAIDA1  2    // Luz Vermelha — energizado sem WiFi
#define PIN_SAIDA2  4    // Luz Amarela  — conectado/idle
#define PIN_SAIDA3  5    // Sirene       — confirmação de palete
#define PIN_SAIDA4  18   // Luz Verde    — confirmação de palete
#define PIN_BOTAO   15   // Botão (pullup interno, pressão = LOW)

// ── Parâmetros de tempo ────────────────────────────────────────────────────
const unsigned long HOLD_MS         = 2000; // segurar 2 s para acionar
const unsigned long HEARTBEAT_MS    = 5000; // intervalo heartbeat dashboard
const unsigned long CONFIRMACAO_MS  = 3000; // verde fica 3 s após confirmação

// ── Helpers: relé ativo-LOW ────────────────────────────────────────────────
#define RELE_ON(pin)  digitalWrite(pin, LOW)
#define RELE_OFF(pin) digitalWrite(pin, HIGH)

// ── Estado interno ─────────────────────────────────────────────────────────
unsigned long inicioPressao   = 0;
unsigned long ultimoHeartbeat = 0;
bool acionado    = false;
bool wifiOnline  = false; // rastreia estado anterior da conexão

// ── setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(PIN_SAIDA1, OUTPUT);
  pinMode(PIN_SAIDA2, OUTPUT);
  pinMode(PIN_SAIDA3, OUTPUT);
  pinMode(PIN_SAIDA4, OUTPUT);
  pinMode(PIN_BOTAO,  INPUT_PULLUP);

  // Estado inicial: só vermelho aceso
  RELE_ON(PIN_SAIDA1);
  RELE_OFF(PIN_SAIDA2);
  RELE_OFF(PIN_SAIDA3);
  RELE_OFF(PIN_SAIDA4);

  // Aguarda conexão WiFi — vermelho pisca a cada 1 s
  Serial.print("Conectando ao WiFi");
  WiFi.begin(SSID, SENHA_WIFI);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(PIN_SAIDA1, !digitalRead(PIN_SAIDA1)); // blink 1 s
    delay(1000);
    Serial.print(".");
  }

  // Conectado: vermelho OFF → amarelo ON
  RELE_OFF(PIN_SAIDA1);
  RELE_ON(PIN_SAIDA2);
  wifiOnline = true;
  Serial.printf("\nConectado! IP: %s\n", WiFi.localIP().toString().c_str());
}

// ── Busca OP ativa ─────────────────────────────────────────────────────────
String buscarOPAtiva() {
  if (WiFi.status() != WL_CONNECTED) return "";

  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/producao");
  http.setTimeout(5000);
  int code = http.GET();
  if (code != 200) { http.end(); return ""; }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, payload) != DeserializationError::Ok) return "";
  JsonArray ordens = doc["ordens"].as<JsonArray>();
  if (ordens.size() == 0) return "";
  return ordens[0]["numero"].as<String>();
}

// ── Heartbeat ao dashboard ─────────────────────────────────────────────────
void enviarHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/esp32/ping");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  String ip   = WiFi.localIP().toString();
  String body = "{\"painel\":" + String(PAINEL_NUM) + ",\"ip\":\"" + ip + "\"}";
  http.POST(body);
  http.end();
}

// ── Registra 1 palete ──────────────────────────────────────────────────────
bool registrarPalete(const String& op) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/producao");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);

  String body = "{\"acao\":\"paletes\",\"op\":\"" + op + "\",\"paletes\":1,\"linhas\":\"" LINHAS_STR "\"}";
  int code = http.POST(body);
  Serial.printf("POST /api/producao → %d\n", code);
  http.end();
  return (code == 200);
}

// ── loop ───────────────────────────────────────────────────────────────────
void loop() {
  unsigned long agora = millis();
  bool conectado = (WiFi.status() == WL_CONNECTED);

  // Detecta mudança de estado do WiFi
  if (conectado && !wifiOnline) {
    // Reconectou: vermelho OFF → amarelo ON
    RELE_OFF(PIN_SAIDA1);
    RELE_ON(PIN_SAIDA2);
    wifiOnline = true;
    Serial.printf("WiFi reconectado! IP: %s\n", WiFi.localIP().toString().c_str());
  } else if (!conectado && wifiOnline) {
    // Perdeu conexão: amarelo OFF → vermelho ON
    RELE_OFF(PIN_SAIDA2);
    RELE_OFF(PIN_SAIDA3);
    RELE_OFF(PIN_SAIDA4);
    RELE_ON(PIN_SAIDA1);
    wifiOnline = false;
    Serial.println("WiFi desconectado — aguardando reconexão...");
    WiFi.reconnect();
  }

  // Heartbeat periódico
  if (agora - ultimoHeartbeat >= HEARTBEAT_MS) {
    ultimoHeartbeat = agora;
    enviarHeartbeat();
  }

  bool pressionado = (digitalRead(PIN_BOTAO) == LOW);

  if (pressionado) {
    if (inicioPressao == 0) inicioPressao = agora;

    if (!acionado && (agora - inicioPressao >= HOLD_MS)) {
      acionado = true;

      Serial.println("Botão 2 s — buscando OP ativa...");
      String op = buscarOPAtiva();

      if (op.isEmpty()) {
        Serial.println("Nenhuma OP ativa.");
      } else {
        Serial.printf("OP: %s — registrando palete no Painel %d...\n", op.c_str(), PAINEL_NUM);

        // Amarelo OFF + sirene ON imediatamente ao acionar
        RELE_OFF(PIN_SAIDA2);
        RELE_ON(PIN_SAIDA3);

        if (registrarPalete(op)) {
          // Registrado: sirene OFF → verde ON por 3 s → amarelo volta
          Serial.println("Palete registrado (" LINHAS_STR ")");
          RELE_OFF(PIN_SAIDA3);
          RELE_ON(PIN_SAIDA4);
          delay(CONFIRMACAO_MS);
          RELE_OFF(PIN_SAIDA4);
          RELE_ON(PIN_SAIDA2);
        } else {
          // Falha: sirene OFF → amarelo volta
          Serial.println("Falha ao registrar.");
          RELE_OFF(PIN_SAIDA3);
          RELE_ON(PIN_SAIDA2);
        }
      }
    }
  } else {
    inicioPressao = 0;
    acionado = false;
  }

  delay(10);
}
