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
    - Após registro bem-sucedido, Saída 2 é habilitada

  Dependências (Library Manager):
    - ArduinoJson  (v6+)  — Benoit Blanchon
*/

#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// ── Configurações ──────────────────────────────────────────────────────────
const char* SSID      = "SEU_WIFI";
const char* SENHA_WIFI = "SUA_SENHA_WIFI";
const char* BASE_URL   = "https://dashboard-producao-two.vercel.app";

// ── Pinos ──────────────────────────────────────────────────────────────────
#define PIN_SAIDA1  2    // LED/relé: WiFi conectado
#define PIN_SAIDA2  4    // LED/relé: habilitado após palete
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

  digitalWrite(PIN_SAIDA1, LOW);
  digitalWrite(PIN_SAIDA2, LOW);

  // Pisca Saída 1 enquanto conecta
  Serial.print("Conectando ao WiFi");
  WiFi.begin(SSID, SENHA_WIFI);
  while (WiFi.status() != WL_CONNECTED) {
    digitalWrite(PIN_SAIDA1, !digitalRead(PIN_SAIDA1));
    delay(400);
    Serial.print(".");
  }

  // Conectado: Saída 1 acende fixo
  digitalWrite(PIN_SAIDA1, HIGH);
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
  http.POST("{\"painel\":1,\"ip\":\"" + ip + "\"}");
  http.end();
}

// ── Registra 1 palete no Painel 1 (Linhas 1 e 2) ─────────────────────────
bool registrarPalete(const String& op) {
  if (WiFi.status() != WL_CONNECTED) return false;

  HTTPClient http;
  http.begin(String(BASE_URL) + "/api/producao");
  http.addHeader("Content-Type", "application/json");
  http.setTimeout(8000);

  // Painel 1 = Linhas 1 e 2
  String body = "{\"acao\":\"paletes\",\"op\":\"" + op + "\",\"paletes\":1,\"linhas\":\"1,2\"}";
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

  bool pressionado = (digitalRead(PIN_BOTAO) == LOW); // LOW = botão ativo

  if (pressionado) {
    if (inicioPressao == 0) inicioPressao = agora;

    if (!acionado && (agora - inicioPressao >= HOLD_MS)) {
      acionado = true; // trava para não re-disparar enquanto mantém pressionado

      Serial.println("Botão segurado 2 s — buscando OP ativa...");
      String op = buscarOPAtiva();

      if (op.isEmpty()) {
        Serial.println("Nenhuma OP ativa encontrada.");
      } else {
        Serial.printf("OP: %s — registrando palete no Painel 1...\n", op.c_str());
        if (registrarPalete(op)) {
          Serial.println("Palete registrado (Linhas 1+2)");
          digitalWrite(PIN_SAIDA2, HIGH); // habilita Saída 2
        } else {
          Serial.println("Falha ao registrar.");
        }
      }
    }
  } else {
    // Botão solto: reseta para próxima pressão
    inicioPressao = 0;
    acionado = false;
  }

  delay(10);
}
