/*
 * ═══════════════════════════════════════════════════════════════════════════
 * MONITOR DE AMBIENTES — ESP32-S3
 *
 * No sensor para salas de aula e escritorios. Mede continuamente os
 * parametros que as normas brasileiras cobram e publica no Supabase, que
 * compara com os limites normativos e dispara WhatsApp quando algo sai da
 * faixa.
 *
 * O QUE CADA SENSOR ATENDE:
 *   VEML7700  -> iluminancia (lx)          ABNT NBR ISO/CIE 8995-1:2013
 *   AS7341    -> temperatura de cor (K)    ABNT NBR ISO/CIE 8995-1:2013
 *   Fotodiodo -> modulacao de flicker (%)  IEEE 1789-2015 / 8995-1 §4.10
 *   SCD41     -> CO2 (ppm)                 ABNT NBR 17037:2023
 *   SHT45     -> temperatura / umidade     ABNT NBR 16401-2:2008 / NR-17
 *   SPS30     -> MP1 / MP2,5 / MP4 / MP10  ABNT NBR 17037:2023 / OMS
 *   SGP41     -> indice de COV e NOx       ISO 16000-29
 *   SFA30     -> formaldeido (ug/m3)       OMS AQG
 *   FS3000    -> velocidade do ar (m/s)    ABNT NBR 16401-2:2008
 *   ICS-43434 -> LAeq dB(A)                ABNT NBR 10152:2017
 *   LD2410C   -> presenca                  contexto de ocupacao
 *
 * NAO E MEDIDO POR SENSOR (vem do projeto luminotecnico):
 *   UGR e Ra. Nao existe sensor de campo para eles — sao calculados no
 *   DIALux/Relux e lidos do datasheet da luminaria. Ficam no banco como
 *   parametros de origem 'projeto'.
 *
 * BIBLIOTECAS (Gerenciador de Bibliotecas da IDE Arduino):
 *   WiFiManager                      (tzapu)
 *   ArduinoJson                      (Benoit Blanchon)
 *   Adafruit VEML7700 Library        (+ Adafruit BusIO)
 *   Adafruit AS7341
 *   Sensirion I2C SCD4x
 *   Sensirion I2C SHT4x
 *   Sensirion I2C SGP41
 *   Sensirion Gas Index Algorithm
 *   Sensirion I2C SPS30              (arduino-i2c-sps30)
 *   SparkFun FS3000 Arduino Library
 *   ld2410                           (ncmreynolds)
 *
 * PLACA: ESP32-S3 Dev Module. Arduino-ESP32 core 2.0.14+ ou 3.x.
 * ═══════════════════════════════════════════════════════════════════════════
 */

#include <WiFi.h>
#include <WiFiManager.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <Wire.h>
#include <math.h>
#include "config.h"

#ifdef USA_VEML7700
  #include <Adafruit_VEML7700.h>
  Adafruit_VEML7700 veml;
#endif
#ifdef USA_AS7341
  #include <Adafruit_AS7341.h>
  Adafruit_AS7341 as7341;
#endif
#ifdef USA_SCD41
  #include <SensirionI2CScd4x.h>
  SensirionI2CScd4x scd4x;
#endif
#ifdef USA_SHT45
  #include <SensirionI2CSht4x.h>
  SensirionI2CSht4x sht4x;
#endif
#ifdef USA_SPS30
  #include <SensirionI2CSps30.h>
  SensirionI2CSps30 sps30;
#endif
#ifdef USA_SGP41
  #include <SensirionI2CSgp41.h>
  #include <VOCGasIndexAlgorithm.h>
  #include <NOxGasIndexAlgorithm.h>
  SensirionI2CSgp41 sgp41;
  VOCGasIndexAlgorithm vocAlg;
  NOxGasIndexAlgorithm noxAlg;
  uint16_t sgpCondicionamento = 10;   // 10 s de conditioning exigidos no boot
#endif
#ifdef USA_SFA30
  #include <SensirionI2CSfa3x.h>
  SensirionI2CSfa3x sfa3x;
#endif
#ifdef USA_FS3000
  #include <SparkFun_FS3000_Arduino_Library.h>
  FS3000 fs3000;
#endif
#ifdef USA_MIC
  #include <driver/i2s.h>
#endif
#ifdef USA_LD2410
  #include <ld2410.h>
  ld2410 radar;
#endif

// ═══ ESTADO DAS MEDIDAS ════════════════════════════════════════════════════
// NAN sinaliza "sem leitura" — o campo e omitido do JSON e o backend nao
// avalia o que nao recebeu.
struct Medidas {
  float lux      = NAN;
  float tcc      = NAN;
  float flicker  = NAN;
  float co2      = NAN;
  float temp     = NAN;
  float ur       = NAN;
  float pm1      = NAN;
  float pm25     = NAN;
  float pm4      = NAN;
  float pm10     = NAN;
  float voc      = NAN;
  float nox      = NAN;
  float hcho     = NAN;
  float vel_ar   = NAN;
  float ruido    = NAN;
  float ruido_max= NAN;
  float ar_ext   = NAN;
  bool  presenca = false;
  bool  temPresenca = false;
} m;

bool sensorOk[12] = {false};

unsigned long ultimoEnvio   = 0;
unsigned long ultimaLeitura = 0;

// ═══ ACUSTICA — LAeq com ponderacao A ══════════════════════════════════════
#ifdef USA_MIC

#define TAXA_AMOSTRAGEM  48000
#define BLOCO_AMOSTRAS   512

// Filtro de ponderacao A para 48 kHz, em 3 secoes bi-quadraticas (SOS).
// Fonte: implementacao de referencia esp32-i2s-slm (ikostoski), derivada da
// IEC 61672-1. O ganho absoluto e corrigido por MIC_OFFSET_DB na calibracao
// contra um decibelimetro classe 2 — sem essa calibracao o valor NAO tem
// validade metrologica.
struct SOS { float b1, b2, a1, a2; };
static const float A_GANHO = 0.169994948147430f;
static const SOS A_SOS[3] = {
  {-2.00026996133106f, +1.00027056142719f, -1.060868438509278f, -0.163987445885926f},
  {+4.35912384203144f, +3.09120265783884f, +1.208419926363593f, -0.273166998428332f},
  {-0.70930303489759f, -0.29071868393580f, +1.982242159753048f, -0.982298594928989f}
};
static float sosW[3][2] = {{0,0},{0,0},{0,0}};

double  somaQuadrados = 0;   // energia acumulada (pos-ponderacao A)
uint32_t amostrasAcum = 0;
float   picoRms       = 0;   // maior RMS de bloco -> aproxima o LAFmax

static inline float aplicaPonderacaoA(float x) {
  // Forma direta II transposta, secao a secao.
  for (int s = 0; s < 3; s++) {
    float w = x - A_SOS[s].a1 * sosW[s][0] - A_SOS[s].a2 * sosW[s][1];
    x = w + A_SOS[s].b1 * sosW[s][0] + A_SOS[s].b2 * sosW[s][1];
    sosW[s][1] = sosW[s][0];
    sosW[s][0] = w;
  }
  return x * A_GANHO;
}

void iniciarMicrofone() {
  i2s_config_t cfg = {};
  cfg.mode                = (i2s_mode_t)(I2S_MODE_MASTER | I2S_MODE_RX);
  cfg.sample_rate         = TAXA_AMOSTRAGEM;
  cfg.bits_per_sample     = I2S_BITS_PER_SAMPLE_32BIT;
  cfg.channel_format      = I2S_CHANNEL_FMT_ONLY_LEFT;
  cfg.communication_format= I2S_COMM_FORMAT_STAND_I2S;
  cfg.intr_alloc_flags    = ESP_INTR_FLAG_LEVEL1;
  cfg.dma_buf_count       = 8;
  cfg.dma_buf_len         = BLOCO_AMOSTRAS;
  cfg.use_apll            = false;

  i2s_pin_config_t pinos = {};
  pinos.bck_io_num   = PIN_I2S_SCK;
  pinos.ws_io_num    = PIN_I2S_WS;
  pinos.data_out_num = I2S_PIN_NO_CHANGE;
  pinos.data_in_num  = PIN_I2S_SD;

  if (i2s_driver_install(I2S_NUM_0, &cfg, 0, NULL) != ESP_OK) return;
  if (i2s_set_pin(I2S_NUM_0, &pinos) != ESP_OK) return;
  sensorOk[9] = true;
  Serial.println("[MIC] I2S iniciado a 48 kHz");
}

// Consome o que estiver no buffer DMA sem bloquear o loop.
void acumularAudio() {
  static int32_t buf[BLOCO_AMOSTRAS];
  size_t lidos = 0;
  while (i2s_read(I2S_NUM_0, buf, sizeof(buf), &lidos, 0) == ESP_OK && lidos > 0) {
    const int n = lidos / sizeof(int32_t);
    double somaBloco = 0;
    for (int i = 0; i < n; i++) {
      // Dado util nos 24 bits altos; normaliza para -1.0 .. +1.0
      float amostra = (float)(buf[i] >> 8) / 8388608.0f;
      float a = aplicaPonderacaoA(amostra);
      somaBloco += (double)a * a;
    }
    somaQuadrados += somaBloco;
    amostrasAcum  += n;
    float rmsBloco = sqrtf((float)(somaBloco / n));
    if (rmsBloco > picoRms) picoRms = rmsBloco;
    lidos = 0;
    if (amostrasAcum > TAXA_AMOSTRAGEM * 90UL) break;  // trava de seguranca
  }
}

float rmsParaSPL(float rms) {
  if (rms <= 1e-9f) return NAN;
  // Sensibilidade: MIC_DBFS_94DB dBFS equivalem a 94 dB SPL.
  return (94.0f - MIC_DBFS_94DB) + 20.0f * log10f(rms) + MIC_OFFSET_DB;
}

void fecharJanelaAcustica() {
  if (amostrasAcum < TAXA_AMOSTRAGEM / 2) return;   // menos de 0,5 s: descarta
  float rms = sqrtf((float)(somaQuadrados / amostrasAcum));
  m.ruido     = rmsParaSPL(rms);
  m.ruido_max = rmsParaSPL(picoRms);
  somaQuadrados = 0;
  amostrasAcum  = 0;
  picoRms       = 0;
}
#endif  // USA_MIC

// ═══ FLICKER — modulacao percentual da luz ═════════════════════════════════
#ifdef USA_FLICKER
// Percentual de modulacao conforme IEEE 1789-2015:
//   Mod% = 100 * (Lmax - Lmin) / (Lmax + Lmin)
// Amostra o fotodiodo a 5 kHz por ~410 ms, o que cobre dezenas de ciclos da
// ondulacao de 120 Hz tipica de driver de LED alimentado em 60 Hz.
float medirFlicker() {
  uint16_t maxV = 0, minV = 4095;
  unsigned long proxima = micros();
  for (uint16_t i = 0; i < FLICKER_AMOSTRAS; i++) {
    while ((long)(micros() - proxima) < 0) { /* espera a cadencia */ }
    proxima += FLICKER_INTERVALO_US;
    uint16_t v = analogRead(PIN_FLICKER_ADC);
    if (v > maxV) maxV = v;
    if (v < minV) minV = v;
  }
  // Ambiente escuro ou fotodiodo saturado: leitura sem significado.
  if (maxV < 60 || maxV >= 4090) return NAN;
  if ((uint32_t)maxV + minV == 0) return NAN;
  return 100.0f * (float)(maxV - minV) / (float)(maxV + minV);
}
#endif

// ═══ TEMPERATURA DE COR a partir do AS7341 ═════════════════════════════════
#ifdef USA_AS7341
// Converte os 8 canais espectrais em XYZ usando as funcoes de casamento de
// cor CIE 1931 2 graus nos comprimentos de onda centrais de cada canal, e
// aplica a formula de McCamy.
//
// LIMITACAO: 8 bandas sao uma amostragem grosseira do espectro. O erro
// tipico e de algumas centenas de kelvin — serve para verificar se a
// luminaria instalada e 3000/4000/6500 K, nao para ensaio fotometrico.
float calcularTCC(uint16_t canais[]) {
  //                  415     445     480     515     555     590     630     680  nm
  const float cx[8] = {0.3275f,0.3420f,0.0956f,0.0360f,0.5140f,1.0263f,0.6424f,0.1649f};
  const float cy[8] = {0.0120f,0.0300f,0.1390f,0.6070f,0.9950f,0.7570f,0.2650f,0.0610f};
  const float cz[8] = {1.7581f,1.7600f,0.8130f,0.1180f,0.0060f,0.0011f,0.0000f,0.0000f};
  // Indices dos canais F1..F8 no buffer de 12 posicoes da Adafruit
  const uint8_t idx[8] = {0, 1, 2, 3, 6, 7, 8, 9};

  float X = 0, Y = 0, Z = 0;
  for (int i = 0; i < 8; i++) {
    float v = (float)canais[idx[i]];
    X += v * cx[i];
    Y += v * cy[i];
    Z += v * cz[i];
  }
  float soma = X + Y + Z;
  if (soma < 1.0f) return NAN;              // escuro demais
  float x = X / soma, y = Y / soma;
  if (fabsf(0.1858f - y) < 1e-6f) return NAN;
  float n = (x - 0.3320f) / (0.1858f - y);
  float cct = 449.0f*n*n*n + 3525.0f*n*n + 6823.3f*n + 5520.33f;
  if (cct < 1500 || cct > 15000) return NAN;
  return cct;
}
#endif

// ═══ INICIALIZACAO DOS SENSORES ════════════════════════════════════════════
void iniciarSensores() {
  Wire.begin(PIN_SDA, PIN_SCL, I2C_FREQ);
  delay(50);

#ifdef USA_VEML7700
  if (veml.begin()) {
    veml.setGain(VEML7700_GAIN_1_8);            // sala iluminada nao satura
    veml.setIntegrationTime(VEML7700_IT_100MS);
    sensorOk[0] = true;
    Serial.println("[VEML7700] ok");
  } else Serial.println("[VEML7700] FALHOU");
#endif

#ifdef USA_AS7341
  if (as7341.begin()) {
    as7341.setATIME(100);
    as7341.setASTEP(999);
    as7341.setGain(AS7341_GAIN_16X);
    sensorOk[1] = true;
    Serial.println("[AS7341] ok");
  } else Serial.println("[AS7341] FALHOU");
#endif

#ifdef USA_SCD41
  scd4x.begin(Wire);
  scd4x.stopPeriodicMeasurement();
  delay(500);
  if (scd4x.startPeriodicMeasurement() == 0) {
    sensorOk[2] = true;
    Serial.println("[SCD41] ok — primeira leitura valida em ~5 s");
  } else Serial.println("[SCD41] FALHOU");
#endif

#ifdef USA_SHT45
  sht4x.begin(Wire);
  {
    uint32_t serial = 0;
    if (sht4x.serialNumber(serial) == 0) {
      sensorOk[3] = true;
      Serial.printf("[SHT45] ok (serie %lu)\n", (unsigned long)serial);
    } else Serial.println("[SHT45] FALHOU");
  }
#endif

#ifdef USA_SPS30
  sps30.begin(Wire);
  delay(100);
  if (sps30.startMeasurement(0x05) == 0) {     // 0x05 = float IEEE754
    sensorOk[4] = true;
    Serial.println("[SPS30] ok — estabiliza em ~30 s");
  } else Serial.println("[SPS30] FALHOU");
#endif

#ifdef USA_SGP41
  sgp41.begin(Wire);
  {
    uint16_t teste = 0;
    if (sgp41.executeSelfTest(teste) == 0 && teste == 0xD400) {
      sensorOk[5] = true;
      Serial.println("[SGP41] ok — indice de COV estabiliza em ~1 h");
    } else Serial.println("[SGP41] autoteste FALHOU");
  }
#endif

#ifdef USA_SFA30
  sfa3x.begin(Wire);
  if (sfa3x.startContinuousMeasurement() == 0) {
    sensorOk[6] = true;
    Serial.println("[SFA30] ok");
  } else Serial.println("[SFA30] FALHOU");
#endif

#ifdef USA_FS3000
  if (fs3000.begin()) {
    fs3000.setRange(AIRFLOW_RANGE_7_MPS);
    sensorOk[7] = true;
    Serial.println("[FS3000] ok");
  } else Serial.println("[FS3000] FALHOU");
#endif

#ifdef USA_FLICKER
  analogSetPinAttenuation(PIN_FLICKER_ADC, ADC_11db);
  sensorOk[8] = true;
#endif

#ifdef USA_MIC
  iniciarMicrofone();
#endif

#ifdef USA_LD2410
  Serial1.begin(256000, SERIAL_8N1, PIN_LD2410_RX, PIN_LD2410_TX);
  delay(200);
  if (radar.begin(Serial1)) {
    sensorOk[10] = true;
    Serial.println("[LD2410] ok");
  } else Serial.println("[LD2410] FALHOU");
#endif
}

// ═══ LEITURA PERIODICA ═════════════════════════════════════════════════════
void lerSensores() {
#ifdef USA_VEML7700
  if (sensorOk[0]) {
    float lx = veml.readLux(VEML_LUX_AUTO);    // troca ganho/integracao sozinho
    if (!isnan(lx) && lx >= 0) m.lux = lx;
  }
#endif

#ifdef USA_AS7341
  if (sensorOk[1]) {
    uint16_t canais[12];
    if (as7341.readAllChannels(canais)) m.tcc = calcularTCC(canais);
  }
#endif

#ifdef USA_SCD41
  if (sensorOk[2]) {
    bool pronto = false;
    if (scd4x.getDataReadyFlag(pronto) == 0 && pronto) {
      uint16_t co2 = 0; float t = 0, h = 0;
      if (scd4x.readMeasurement(co2, t, h) == 0 && co2 != 0) {
        m.co2 = co2;
        // Fallback: sem SHT45 instalado, T e UR vem do proprio SCD41.
        if (isnan(m.temp)) m.temp = t;
        if (isnan(m.ur))   m.ur   = h;
      }
    }
  }
#endif

#ifdef USA_SHT45
  if (sensorOk[3]) {
    float t = 0, h = 0;
    if (sht4x.measureHighPrecision(t, h) == 0) { m.temp = t; m.ur = h; }
  }
#endif

#ifdef USA_SPS30
  if (sensorOk[4]) {
    uint16_t pronto = 0;
    if (sps30.readDataReadyFlag(pronto) == 0 && pronto) {
      float p1, p25, p4, p10, n05, n1, n25, n4, n10, tam;
      if (sps30.readMeasurementValuesFloat(p1, p25, p4, p10,
                                           n05, n1, n25, n4, n10, tam) == 0) {
        m.pm1 = p1; m.pm25 = p25; m.pm4 = p4; m.pm10 = p10;
      }
    }
  }
#endif

#ifdef USA_SGP41
  if (sensorOk[5]) {
    // O SGP41 precisa de T e UR para compensar a leitura dos sinais brutos.
    uint16_t ticksUR = 0x8000, ticksT = 0x6666;   // 50% e 25 C como padrao
    if (!isnan(m.ur))   ticksUR = (uint16_t)(m.ur * 65535.0f / 100.0f);
    if (!isnan(m.temp)) ticksT  = (uint16_t)((m.temp + 45.0f) * 65535.0f / 175.0f);

    uint16_t brutoVoc = 0, brutoNox = 0;
    if (sgpCondicionamento > 0) {
      // Os primeiros 10 s exigem o comando de conditioning do fabricante.
      if (sgp41.executeConditioning(ticksUR, ticksT, brutoVoc) == 0) {
        sgpCondicionamento -= (sgpCondicionamento > 5 ? 5 : sgpCondicionamento);
      }
    } else if (sgp41.measureRawSignals(ticksUR, ticksT, brutoVoc, brutoNox) == 0) {
      m.voc = vocAlg.process((int32_t)brutoVoc);
      m.nox = noxAlg.process((int32_t)brutoNox);
    }
  }
#endif

#ifdef USA_SFA30
  if (sensorOk[6]) {
    int16_t hcho = 0, ur = 0, t = 0;
    if (sfa3x.readMeasuredValues(hcho, ur, t) == 0) {
      // O SFA30 entrega ppb; a OMS especifica em ug/m3.
      // 1 ppb HCHO = 1,23 ug/m3 a 25 C e 1 atm.
      m.hcho = (hcho / 5.0f) * 1.23f;
    }
  }
#endif

#ifdef USA_FS3000
  if (sensorOk[7]) {
    float v = fs3000.readMetersPerSecond();
    if (v >= 0 && v < 10) m.vel_ar = v;
  }
#endif

#ifdef USA_LD2410
  if (sensorOk[10] && radar.isConnected()) {
    radar.read();
    m.presenca    = radar.presenceDetected();
    m.temPresenca = true;
  }
#endif
}

// ═══ VAZAO DE AR EXTERIOR pelo balanco de CO2 ══════════════════════════════
// Em regime permanente, cada pessoa gera ~0,005 L/s de CO2 (adulto sentado,
// 1,0 met). Igualando geracao e diluicao:
//     Q [L/s.pessoa] = 10^6 * 0,005 / (CO2_interno - CO2_externo)
// A NBR 16401-3 pede 3,8 L/s.pessoa em sala de aula e 2,5 em escritorio.
// Vale so com a sala ocupada e estabilizada — por isso os limites abaixo.
float estimarVazaoArExterior(float co2Interno, float co2Externo) {
  float delta = co2Interno - co2Externo;
  if (isnan(co2Interno) || delta < 50) return NAN;   // sala vazia ou recem-aberta
  float q = 5000.0f / delta;
  if (q > 50) return NAN;                            // fora de regime permanente
  return q;
}

// ═══ ENVIO ═════════════════════════════════════════════════════════════════
bool enviarLeitura() {
  if (WiFi.status() != WL_CONNECTED) return false;

  JsonDocument doc;
  doc["device_id"] = DEVICE_ID;
  doc["fw_versao"] = FW_VERSAO;
  doc["ip"]        = WiFi.localIP().toString();
  doc["rssi_dbm"]  = WiFi.RSSI();
  doc["uptime_s"]  = (uint32_t)(millis() / 1000);
  doc["co2_ext"]   = CO2_EXTERNO_PADRAO;

  // Só entra no JSON o que foi realmente medido.
  if (!isnan(m.lux))      doc["lux"]      = roundf(m.lux);
  if (!isnan(m.tcc))      doc["tcc"]      = roundf(m.tcc);
  if (!isnan(m.flicker))  doc["flicker"]  = roundf(m.flicker * 10) / 10.0;
  if (!isnan(m.co2))      doc["co2"]      = roundf(m.co2);
  if (!isnan(m.temp))     doc["temp"]     = roundf(m.temp * 10) / 10.0;
  if (!isnan(m.ur))       doc["ur"]       = roundf(m.ur);
  if (!isnan(m.pm1))      doc["pm1"]      = roundf(m.pm1 * 10) / 10.0;
  if (!isnan(m.pm25))     doc["pm25"]     = roundf(m.pm25 * 10) / 10.0;
  if (!isnan(m.pm4))      doc["pm4"]      = roundf(m.pm4 * 10) / 10.0;
  if (!isnan(m.pm10))     doc["pm10"]     = roundf(m.pm10 * 10) / 10.0;
  if (!isnan(m.voc))      doc["voc"]      = roundf(m.voc);
  if (!isnan(m.nox))      doc["nox"]      = roundf(m.nox);
  if (!isnan(m.hcho))     doc["hcho"]     = roundf(m.hcho);
  if (!isnan(m.vel_ar))   doc["vel_ar"]   = roundf(m.vel_ar * 100) / 100.0;
  if (!isnan(m.ruido))    doc["ruido"]    = roundf(m.ruido * 10) / 10.0;
  if (!isnan(m.ruido_max))doc["ruido_max"]= roundf(m.ruido_max * 10) / 10.0;
  if (!isnan(m.ar_ext))   doc["ar_ext"]   = roundf(m.ar_ext * 10) / 10.0;
  if (m.temPresenca) {
    doc["presenca"] = m.presenca;
    doc["ocupacao"] = m.presenca ? OCUPACAO_PROJETO : 0;
  }

  String corpo;
  serializeJson(doc, corpo);

  WiFiClientSecure cliente;
  cliente.setInsecure();          // Supabase usa cadeia publica; sem CA no flash
  HTTPClient http;
  http.begin(cliente, String(SUPABASE_URL) + INGEST_PATH);
  http.addHeader("Content-Type", "application/json");
  http.addHeader("Authorization", String("Bearer ") + SUPABASE_ANON_KEY);
  http.addHeader("x-device-token", DEVICE_TOKEN);
  http.setTimeout(15000);

  int codigo = http.POST(corpo);
  String resposta = http.getString();
  http.end();

  if (codigo == 200) {
    Serial.printf("[ENVIO] ok — %s\n", resposta.c_str());
    return true;
  }
  Serial.printf("[ENVIO] HTTP %d — %s\n", codigo, resposta.c_str());
  return false;
}

void mostrarNoSerial() {
  Serial.println("──────────────────────────────────────────────");
  if (!isnan(m.lux))     Serial.printf("  Iluminancia .. %.0f lx\n",     m.lux);
  if (!isnan(m.tcc))     Serial.printf("  Temp. de cor . %.0f K\n",      m.tcc);
  if (!isnan(m.flicker)) Serial.printf("  Flicker ...... %.1f %%\n",     m.flicker);
  if (!isnan(m.co2))     Serial.printf("  CO2 .......... %.0f ppm (ext %.0f)\n", m.co2, (float)CO2_EXTERNO_PADRAO);
  if (!isnan(m.temp))    Serial.printf("  Temperatura .. %.1f C\n",      m.temp);
  if (!isnan(m.ur))      Serial.printf("  Umidade ...... %.0f %%\n",     m.ur);
  if (!isnan(m.pm25))    Serial.printf("  MP2,5 ........ %.1f ug/m3\n",  m.pm25);
  if (!isnan(m.pm10))    Serial.printf("  MP10 ......... %.1f ug/m3\n",  m.pm10);
  if (!isnan(m.voc))     Serial.printf("  Indice COV ... %.0f\n",        m.voc);
  if (!isnan(m.hcho))    Serial.printf("  Formaldeido .. %.0f ug/m3\n",  m.hcho);
  if (!isnan(m.vel_ar))  Serial.printf("  Vel. do ar ... %.2f m/s\n",    m.vel_ar);
  if (!isnan(m.ruido))   Serial.printf("  LAeq ......... %.1f dB(A)\n",  m.ruido);
  if (!isnan(m.ar_ext))  Serial.printf("  Ar exterior .. %.1f L/s.pessoa\n", m.ar_ext);
  if (m.temPresenca)     Serial.printf("  Presenca ..... %s\n", m.presenca ? "sim" : "nao");
}

void conectarWiFi() {
  WiFi.mode(WIFI_STA);
  WiFiManager wm;
  wm.setConfigPortalTimeout(PORTAL_TIMEOUT_S);
  wm.setConnectTimeout(20);

  Serial.println("[WiFi] Conectando na rede salva (ou abrindo portal)...");
  if (!wm.autoConnect(AP_CONFIG_NOME, AP_CONFIG_SENHA)) {
    Serial.println("[WiFi] Portal expirou sem configuracao. Reiniciando...");
    delay(2000);
    ESP.restart();
  }
  Serial.println("[WiFi] Conectado! IP: " + WiFi.localIP().toString());
}

void setup() {
  Serial.begin(115200);
  delay(1500);
  Serial.println();
  Serial.println("═══ MONITOR DE AMBIENTES — NBR 8995-1 / 17037 / 16401 / 10152 ═══");
  Serial.printf("Dispositivo: %s  ·  firmware %s\n", DEVICE_ID, FW_VERSAO);

  iniciarSensores();
  conectarWiFi();

  ultimoEnvio = millis();
}

void loop() {
  // Reconexao silenciosa — a rede da escola cai com frequencia.
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long ultimaTentativa = 0;
    if (millis() - ultimaTentativa > 30000UL) {
      ultimaTentativa = millis();
      Serial.println("[WiFi] Reconectando...");
      WiFi.reconnect();
    }
  }

#ifdef USA_MIC
  if (sensorOk[9]) acumularAudio();
#endif

  if (millis() - ultimaLeitura >= INTERVALO_LEITURA_MS) {
    ultimaLeitura = millis();
    lerSensores();
  }

  if (millis() - ultimoEnvio >= INTERVALO_ENVIO_MS) {
    ultimoEnvio = millis();

#ifdef USA_FLICKER
    // Bloqueia ~410 ms — por isso roda logo antes de fechar a janela
    // acustica, cujo acumulador e zerado em seguida.
    if (sensorOk[8]) m.flicker = medirFlicker();
#endif
#ifdef USA_MIC
    if (sensorOk[9]) fecharJanelaAcustica();
#endif

    m.ar_ext = estimarVazaoArExterior(m.co2, CO2_EXTERNO_PADRAO);

    mostrarNoSerial();
    enviarLeitura();
  }

  delay(10);
}
