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

// ═══ PERFIL DO MODULO ═════════════════════════════════════════════════════
// A instalacao recomendada usa DOIS modulos por sala, porque os pontos de
// medicao exigidos pelas normas ficam em alturas diferentes:
//
//   PAREDE (1,10 m do piso, parede interna) — ar, termico, acustico.
//     A zona ocupada da NBR 16401-2 vai de 0,10 a 1,80 m. No teto o ar e
//     estratificado: mais quente e com CO2 diferente do que se respira.
//
//   TETO (centro da sala) — iluminancia, cor, flicker e presenca.
//     Centro do teto e o melhor lugar para o radar de presenca. Para luz e
//     a unica posicao pratica, mas exige o fator de calibracao abaixo.
//
// Descomente UM dos dois perfis. Cada modulo tem seu proprio DEVICE_ID e seu
// proprio cadastro em amb_dispositivos, os dois apontando para o MESMO
// ambiente_id — o backend consolida os parametros por sala.
#define PERFIL_PAREDE
// #define PERFIL_TETO

#ifdef PERFIL_PAREDE
  #define USA_SCD41      // CO2 + temperatura + umidade
  #define USA_SHT45      // temperatura + umidade de referencia (mais preciso)
  #define USA_SPS30      // MP1 / MP2,5 / MP4 / MP10
  #define USA_SGP41      // indice de COV e de NOx
  // #define USA_SFA30   // formaldeido (HCHO) — opcional, sensor caro
  #define USA_FS3000     // velocidade do ar
  #define USA_MIC        // nivel sonoro LAeq dB(A)
#endif

#ifdef PERFIL_TETO
  #define USA_VEML7700   // iluminancia (lux)
  #define USA_AS7341     // temperatura de cor (TCC)
  #define USA_FLICKER    // modulacao de flicker via fotodiodo
  #define USA_LD2410     // presenca por radar mmWave
#endif

// Modulo unico (nao recomendado — ver README §2.4): descomente tudo abaixo e
// comente os dois perfis acima. Aceite que a iluminancia e a temperatura
// perdem validade metrologica.
// #define USA_VEML7700
// #define USA_AS7341
// #define USA_FLICKER
// #define USA_SCD41
// #define USA_SHT45
// #define USA_SPS30
// #define USA_SGP41
// #define USA_FS3000
// #define USA_MIC
// #define USA_LD2410

// ═══ CALIBRACOES DE INSTALACAO ════════════════════════════════════════════

// ─── Correcao de autoaquecimento (modulo de parede) ──────────────────────
// O ESP32 com WiFi dissipa ~0,4 W e o ventilador do SPS30 mais ~0,3 W. Dentro
// de uma caixa fechada isso eleva o ar interno de 5 a 10 C acima do ambiente,
// e o SHT45 le esse ar quente. Como a umidade relativa deriva da temperatura,
// um erro de 5 C vira ~20 pontos percentuais de erro em UR — o suficiente para
// invalidar a conformidade termica.
//
// A solucao mecanica vem primeiro: SHT45 FORA da caixa, num toco de 10 cm.
// Este offset e a correcao residual, nao substituto do arranjo fisico.
// Como levantar: com a sala em regime, compare com um termohigrometro de
// referencia a 1,10 m e ajuste ate coincidir. Refaca se trocar a caixa.
#define TEMP_OFFSET_C   0.0    // subtraido da leitura (positivo = caixa quente)
#define UR_OFFSET_PCT   0.0    // somado a leitura

// ─── Fator de iluminancia (modulo de teto) ───────────────────────────────
// A NBR ISO/CIE 8995-1 mede Em NO PLANO DE TAREFA (carteira, ~0,75 m), com o
// sensor voltado para cima. Um sensor no teto olhando para baixo mede outra
// coisa: a luminancia refletida do piso e do mobiliario.
//
// Este fator converte uma na outra. Levantamento, uma vez por sala:
//   1. luximetro sobre a carteira, voltado para cima, sala em uso normal
//   2. leitura simultanea do modulo de teto (Serial ou dashboard)
//   3. LUX_FATOR = (lux do luximetro) / (lux do modulo de teto)
//
// O fator so vale enquanto o piso, o mobiliario e a pintura nao mudarem.
// Refaca depois de reforma. E ele NAO substitui a medicao em malha do
// Anexo B para verificacao formal — serve para tendencia continua.
#define LUX_FATOR       1.0

#endif
