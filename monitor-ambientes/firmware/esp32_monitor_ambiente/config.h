#ifndef CONFIG_H
#define CONFIG_H

// ═══════════════════════════════════════════════════════════════════════════
// MONITOR DE AMBIENTES — configuracao do no sensor
// Placa alvo: ESP32-S3-WROOM-1 (N16R8)
// ═══════════════════════════════════════════════════════════════════════════

#define FW_VERSAO  "1.0.0"

// ─── IDENTIFICACAO ────────────────────────────────────────────────────────
// Precisa bater EXATAMENTE com amb_dispositivos.device_id no Supabase.
#define DEVICE_ID  "ESP32-SALA101"

// Segredo do dispositivo. No Supabase e gravado apenas o sha256:
//   SELECT encode(digest('SEU_TOKEN_AQUI','sha256'),'hex');
// NAO comitar o token real no GitHub.
#define DEVICE_TOKEN  "TROQUE_ESTE_TOKEN"

// ─── WIFI (configuracao no local via WiFiManager) ─────────────────────────
// Na primeira ligacao o ESP32 sobe o AP abaixo; conecte pelo celular, o
// portal abre sozinho e voce escolhe a rede da escola/escritorio.
#define AP_CONFIG_NOME    "MonitorAmbiente-Setup"
#define AP_CONFIG_SENHA   "12345678"
#define PORTAL_TIMEOUT_S  180

// ─── SUPABASE ─────────────────────────────────────────────────────────────
#define SUPABASE_URL  "https://odnjbvsjqteqapppkkpc.supabase.co"

// A telemetria vai pela Edge Function 'ingest', que valida o DEVICE_TOKEN e
// usa a service_role internamente. O ESP32 nunca carrega a service_role.
#define INGEST_PATH   "/functions/v1/ingest"

// Chave anon — vai no header Authorization apenas para passar pelo gateway
// das Edge Functions. Nao da acesso de escrita a nenhuma tabela.
#define SUPABASE_ANON_KEY  "COLE_AQUI_A_ANON_KEY"

// ─── AMBIENTE ─────────────────────────────────────────────────────────────
// CO2 do ar exterior, em ppm. A NBR 17037:2023 define o limite interno como
// no maximo 700 ppm ACIMA do externo, entao este valor entra no calculo.
// Ideal: um segundo no instalado do lado de fora publicando o valor real.
// Enquanto nao houver, 420 ppm e a linha de base atmosferica atual.
#define CO2_EXTERNO_PADRAO  420.0

// Numero de pessoas usado para estimar a vazao de ar exterior por pessoa
// (L/s.pessoa) pelo balanco de CO2. Se houver sensor de presenca mmWave a
// contagem nao e feita — o LD2410 detecta presenca, nao quantidade — entao
// use a ocupacao de projeto da sala.
#define OCUPACAO_PROJETO  40

// ─── INTERVALOS ───────────────────────────────────────────────────────────
#define INTERVALO_ENVIO_MS   60000UL   // 1 min — resolucao boa sem estourar cota
#define INTERVALO_LEITURA_MS  5000UL   // amostragem interna dos sensores I2C
#define JANELA_LAEQ_MS       60000UL   // LAeq integrado sobre a janela de envio

// ─── I2C (todos os sensores Sensirion/Vishay no mesmo barramento) ─────────
#define PIN_SDA  8
#define PIN_SCL  9
#define I2C_FREQ 100000UL              // 100 kHz — SPS30 nao aceita 400 kHz

// ─── I2S — microfone MEMS (ICS-43434 ou INMP441) ──────────────────────────
#define PIN_I2S_WS   15   // LRCL / WS
#define PIN_I2S_SCK  16   // BCLK
#define PIN_I2S_SD    7   // DOUT do microfone

// Sensibilidade do microfone: dBFS produzidos por 94 dB SPL (1 kHz).
// ICS-43434: -26 dBFS   |   INMP441: -26 dBFS
// AJUSTE FINO OBRIGATORIO: compare com um decibelimetro classe 2 calibrado e
// corrija MIC_OFFSET_DB ate as leituras coincidirem.
#define MIC_DBFS_94DB  -26.0
#define MIC_OFFSET_DB    0.0

// ─── FOTODIODO PARA FLICKER (OPT101 ou BPW34 + TIA) ───────────────────────
// A saida analogica e amostrada rapido para medir a modulacao da luz.
// O VEML7700 e preciso em lux mas lento demais para enxergar 120 Hz.
#define PIN_FLICKER_ADC  4        // ADC1_CH3 — nao use ADC2 com WiFi ligado
#define FLICKER_AMOSTRAS 2048     // ~410 ms a 5 kHz: cobre varios ciclos de 120 Hz
#define FLICKER_INTERVALO_US 200  // 5 kHz

// ─── LD2410C — presenca por radar mmWave (UART) ───────────────────────────
#define PIN_LD2410_RX  18   // RX do ESP32  <- TX do sensor
#define PIN_LD2410_TX  19   // TX do ESP32  -> RX do sensor

// ─── HABILITAR / DESABILITAR SENSORES ─────────────────────────────────────
// Comente a linha do sensor que nao estiver instalado. O firmware simplesmente
// omite o campo no JSON, e o backend nao avalia o que nao recebeu.
#define USA_VEML7700   // iluminancia (lux)
#define USA_AS7341     // temperatura de cor (TCC)
#define USA_FLICKER    // modulacao de flicker via fotodiodo
#define USA_SCD41      // CO2 + temperatura + umidade
#define USA_SHT45      // temperatura + umidade de referencia (mais preciso)
#define USA_SPS30      // MP1 / MP2,5 / MP4 / MP10
#define USA_SGP41      // indice de COV e de NOx
// #define USA_SFA30   // formaldeido (HCHO) — opcional, sensor caro
#define USA_FS3000     // velocidade do ar
#define USA_MIC        // nivel sonoro LAeq dB(A)
#define USA_LD2410     // presenca

#endif
