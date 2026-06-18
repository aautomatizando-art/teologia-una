/*
  Painel 1 — Registro de Palete por Botão
  Hardware: ESP32 Dev Board

  Pinos:
    GPIO 2  → Saída 1: LED/relé — acende ao conectar WiFi
    GPIO 4  → Saída 2: LED/relé — ativado após registrar palete
    GPIO 15 → Botão (GND no outro terminal, usa INPUT_PULLUP)

  Comportamento:
    - Ao conectar ao WiFi, Saída 1 acende
    - Ao segurar o botão por 2 s, busca a OP ativa no dashboard
      e registra 1 palete no Painel 1 (Linhas 1+2)
    - Após registro bem-sucedido, Saída 2 pulsa por 2 s

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
#define PIN_SAIDA1  2    // LED/relé: WiFi conectado  (ativo-LOW)
#define PIN_SAIDA2  4    // LED/relé: pulsa após palete (ativo-LOW)
#define PIN_BOTAO   15   // Botão (pullup interno, pressão = LOW)

// ── Parâmetros de tempo ────────────────────────────────────────────────────
const unsigned long HOLD_MS      = 2000; // pressão de 2 s para acionar
const unsigned long HEARTBEAT_MS = 5000; // intervalo de heartbeat para o dashboard

// ── Estado interno ─────────────────────────────────────────────────────────
unsigned long inicioPressao  = 0;
unsigned long ultimoHeartbeat = 0;
bool acionado = false; // evita re-disparo enquanto botão fica pressionado

// ── setup ──────────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);

  pinMode(PIN_SAIDA1, OUTPUT);
  pinMode(PIN_SAIDA2, OUTPUT);
  pinMode(PIN_BOTAO,  INPUT_PULLUP);

  digitalWrite(PIN_SAIDA1, HIGH); // relé desligado (ativo-LOW)
  digitalWrite(PIN_SAIDA2, HIGH);

  // Pisca Saída 1 enquanto conecta
  Serial.print("Conectando ao WiFi");
  WiFi.begin(SSID, SENHA_WIFI);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(PIN_SAIDA1, !digitalRead(PIN_SAIDA1));
    delay(400);
    Serial.print(".");
  }

  // Conectado: Saída 1 acende fixo (LOW = relé acionado)
  digitalWrite(PIN_SAIDA1, LOW);
  Serial.printf("\nConectado! IP: %s\n", WiFi.localIP().toString().c_str());
}

// ── Busca a OP ativa no dashboard ─────────────────────────────────────────
String buscarOPAtiva() {
  if (WiFi.status() != WL_CONNECTED) return "";

  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/producao");
  http.setTimeout(5000);

  int code = http.GET();
  if (code != 200) {
    Serial.printf("GET /api/producao → %d\n", code);
    http.end();
    return "";
  }

  String payload = http.getString();
  http.end();

  DynamicJsonDocument doc(2048);
  if (deserializeJson(doc, payload) != DeserializationError::Ok) return "";

  JsonArray ordens = doc["ordens"].as<JsonArray>();
  if (ordens.size() == 0) return "";
  return ordens[0]["numero"].as<String>();
}

// ── Envia heartbeat ao dashboard (a cada HEARTBEAT_MS) ────────────────────
void enviarHeartbeat() {
  if (WiFi.status() != WL_CONNECTED) return;

  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/esp32/ping");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(3000);

  String ip = WiFi.localIP().toString();
  String body = "{\"painel\":" + String(PAINEL_NUM) + ",\"ip\":\"" + ip + "\"}";
  http.POST(body);
  http.end();
}

// ── Registra 1 palete no painel configurado ───────────────────────────────
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

  // Heartbeat periódico independente do botão
  if (agora - ultimoHeartbeat >= HEARTBEAT_MS) {
    ultimoHeartbeat = agora;
    enviarHeartbeat();
  }

  bool pressionado = (digitalRead(PIN_BOTAO) == LOW);

  if (pressionado) {
    if (inicioPressao == 0) inicioPressao = agora;

    if (!acionado && (agora - inicioPressao >= HOLD_MS)) {
      acionado = true;

      Serial.println("Botão segurado 2 s — buscando OP ativa...");
      String op = buscarOPAtiva();

      if (op.isEmpty()) {
        Serial.println("Nenhuma OP ativa encontrada.");
      } else {
        Serial.printf("OP: %s — registrando palete no Painel %d...\n", op.c_str(), PAINEL_NUM);
        if (registrarPalete(op)) {
          Serial.println("Palete registrado (" LINHAS_STR ")");
          digitalWrite(PIN_SAIDA2, LOW);  // pulsa relé 2 s
          delay(2000);
          digitalWrite(PIN_SAIDA2, HIGH);
        } else {
          Serial.println("Falha ao registrar.");
        }
      }
    }
  } else {
    inicioPressao = 0;
    acionado = false;
  }

  delay(10);
}
