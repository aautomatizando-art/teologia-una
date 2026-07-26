# Monitor de Ambientes — Conformidade NBR

Monitoramento contínuo de salas de aula e escritórios contra as normas brasileiras
de **iluminação**, **qualidade do ar**, **conforto térmico** e **acústica**.
ESP32 + Supabase + Vercel, com alerta em WhatsApp quando um parâmetro sai da faixa.

O ponto central do projeto: **a norma é dado, não código**. Todos os limites moram
na tabela `amb_limites` do Supabase. Editar uma linha ali muda o critério da
dashboard, dos gráficos e dos alertas de uma vez só — sem redeploy.

---

## 1. O que cada norma exige medir

### 1.1 Iluminação — ABNT NBR ISO/CIE 8995-1:2013

A norma cobra cinco grandezas por ambiente. **Só duas são medíveis por sensor de
campo** — é honesto saber disso antes de prometer "monitoramento da NBR 8995" a
um cliente.

| Grandeza | Símbolo | Medível? | Como se verifica |
|---|---|---|---|
| Iluminância mantida | Ēm (lx) | ✅ sensor | Fotômetro no plano de trabalho |
| Uniformidade | Uo | ⚠️ levantamento | Malha de pontos (Anexo B) — vários sensores ou luxímetro móvel |
| Ofuscamento | UGRL | ❌ projeto | Calculado em DIALux/Relux a partir da luminância e da geometria |
| Reprodução de cor | Ra | ❌ projeto | Datasheet da lâmpada |
| Temperatura de cor | TCC (K) | ✅ sensor | Sensor espectral (estimativa) |
| Cintilação / estroboscópico | § 4.10 | ✅ sensor | Fotodiodo rápido → % de modulação (IEEE 1789-2015) |

**Valores por ambiente** (Ēm mantida / UGRL / Uo / Ra):

| Ambiente | Ēm (lx) | UGRL | Uo | Ra |
|---|---|---|---|---|
| Sala de aula | 300 | 19 | 0,60 | 80 |
| Sala de aula noturna / adultos | 500 | 19 | 0,60 | 80 |
| Quadro-negro (plano vertical) | 500 | 19 | **0,70** | 80 |
| Laboratório / sala de prática | 500 | 19 | 0,60 | 80 |
| Sala de professores | 300 | 19 | 0,60 | 80 |
| Biblioteca — leitura | 500 | 19 | 0,60 | 80 |
| Biblioteca — estantes | 200 | 19 | 0,60 | 80 |
| Auditório / conferências | 500 | 19 | 0,60 | 80 |
| Sala de informática | 300 | 19 | 0,60 | 80 |
| Escritório (escrita, leitura, dados) | 500 | 19 | 0,60 | 80 |
| Escritório panorâmico | 500 | 19 | 0,60 | 80 |
| Sala de reunião | 500 | 19 | 0,60 | 80 |
| Desenho técnico / CAD | **750** | **16** | 0,70 | 80 |
| Balcão de recepção | 300 | **22** | 0,60 | 80 |
| Circulação / corredores | 100 | 25 | 0,40 | 80 |

### 1.2 Qualidade do ar — ABNT NBR 17037:2023

⚠️ **Mudança importante.** A NBR 17037:2023 **substituiu a Resolução RE 09/2003 da
ANVISA**, que era a referência desde 2003. Duas diferenças mudam o projeto do
sistema de medição:

| Item | RE 09/2003 (revogada) | NBR 17037:2023 |
|---|---|---|
| CO₂ | teto fixo de **1000 ppm** | **700 ppm acima do ar externo** — limite variável |
| Particulado | aerodispersóides ≤ 80 µg/m³ | **MP2,5 e MP10**, alinhado à OMS |
| Periodicidade | anual | **semestral** |

O limite diferencial de CO₂ tem consequência prática: **o sistema precisa de uma
referência externa**. Sem ela o teto é estimado com 420 ppm (linha de base
atmosférica atual) — configurável em `CO2_EXTERNO_PADRAO`. O ideal é um segundo
nó instalado do lado de fora.

| Parâmetro | Limite | Alvo | Fonte |
|---|---|---|---|
| CO₂ | ≤ externo + 700 ppm | externo + 500 | NBR 17037:2023 |
| MP2,5 | ≤ 15 µg/m³ (24 h) | 5 (anual) | NBR 17037 / OMS AQG 2021 |
| MP10 | ≤ 45 µg/m³ (24 h) | 15 (anual) | NBR 17037 / OMS AQG 2021 |
| Fungos | ≤ 750 UFC/m³ e I/E ≤ 1,5 | — | NBR 17037 — **ensaio laboratorial, não sensor** |
| COV (índice) | ≤ 250 | 100 | ISO 16000-29 / WELL v2 |
| Formaldeído | ≤ 100 µg/m³ (30 min) | 30 | OMS AQG |
| CO | ≤ 9 ppm (8 h) | 0 | OMS AQG 2021 |

> A contagem de fungos e bactérias exige coleta e cultura em laboratório. Nenhum
> sensor eletrônico substitui esse ensaio — ele continua sendo semestral e
> presencial. O sistema monitora as condições que **favorecem** a proliferação
> (umidade acima de 65%, ar parado), não a contagem em si.

### 1.3 Conforto térmico — ABNT NBR 16401-2:2008

Zona de conforto para 80% de aceitação dos ocupantes:

| Estação | Vestimenta | Temperatura operativa | UR |
|---|---|---|---|
| Verão | 0,5 clo | 22,5 – 25,5 °C (a 65% UR) · 23,0 – 26,0 °C (a 30% UR) | 30 – 65% |
| Inverno | 0,9 clo | 21,0 – 23,5 °C (a 60% UR) · 21,5 – 24,0 °C (a 30% UR) | 30 – 65% |

- Velocidade do ar: ≤ 0,20 m/s (verão), ≤ 0,15 m/s (inverno) na zona ocupada.
- Teto de 65% de UR é controle microbiano; piso de 40% vem da **NR-17**.
- A **NR-17** recomenda 20–23 °C para atividades intelectuais — mais restritivo
  que a NBR em escritórios. O seed usa a NBR; ajuste em `amb_limites` se o
  contrato exigir a NR.

### 1.4 Ventilação — ABNT NBR 16401-3:2008

Vazão de ar exterior mínima, Nível 1:

| Ambiente | Por pessoa | Por área |
|---|---|---|
| Sala de aula | 3,8 L/s | 0,3 L/s·m² |
| Escritório | 2,5 L/s | 0,3 L/s·m² |

O sistema **estima** essa vazão pelo balanço de CO₂ em regime permanente
(`Q ≈ 5000 / (CO₂interno − CO₂externo)`, L/s·pessoa). É um indicador de operação,
não o ensaio de comissionamento — esse se faz com anemômetro no difusor.

### 1.5 Acústica — ABNT NBR 10152:2017

Dois valores por ambiente: o de conforto adequado e o limite acima do qual o
desconforto é relevante.

| Ambiente | Conforto | Limite |
|---|---|---|
| Auditório | 30 | 40 dB(A) |
| Sala de aula | 35 | 45 dB(A) |
| Biblioteca | 35 | 45 dB(A) |
| Sala de reunião | 35 | 45 dB(A) |
| Escritório privativo | 35 | 45 dB(A) |
| Laboratório | 40 | 50 dB(A) |
| Escritório panorâmico | 40 | 50 dB(A) |
| Recepção | 40 | 50 dB(A) |

### 1.6 Cintilação — IEEE 1789-2015

A NBR ISO/CIE 8995-1 (§ 4.10) exige ausência de cintilação e de efeito
estroboscópico, mas não dá número. O IEEE 1789 quantifica, em % de modulação:

| Frequência | Sem efeito observável | Baixo risco |
|---|---|---|
| 120 Hz (ondulação de rede 60 Hz) | ≤ 4,0% | ≤ 9,6% |

Driver de LED barato é a causa mais comum de reprovação neste item.

---

## 2. Hardware

### 2.1 Lista de materiais por nó

| Função | Componente | Por quê |
|---|---|---|
| Processador | **ESP32-S3-WROOM-1 (N16R8)** | PSRAM e USB nativo; folga para o DSP do áudio |
| Iluminância | **Vishay VEML7700** | Resposta fotópica real, 16 bits, saída direta em lux. O BH1750 erra mais sob LED |
| Temperatura de cor | **AMS AS7341** | 11 canais espectrais → estimativa de TCC |
| Cintilação | **OPT101** ou BPW34 + TIA | O VEML7700 é lento demais para enxergar 120 Hz |
| CO₂ | **Sensirion SCD41** | NDIR fotoacústico, ±(50 ppm + 5%). Sensor **real** — MQ-135 não mede CO₂ |
| Temperatura / UR | **Sensirion SHT45** | ±0,1 °C e ±1% UR — precisão de referência |
| Particulado | **Sensirion SPS30** | Laser, MP1/2,5/4/10, autolimpeza, vida útil de 8 anos |
| COV / NOx | **Sensirion SGP41** | Índice normalizado (ISO 16000-29), com compensação de T/UR |
| Formaldeído | **Sensirion SFA30** *(opcional)* | ±20 ppb. Caro — instale onde houver mobiliário novo |
| Velocidade do ar | **Renesas FS3000-1005** | Fio quente I²C, 0–7,23 m/s, faixa certa para HVAC |
| Ruído | **ICS-43434** ou INMP441 | MEMS I²S de 24 bits; ponderação A feita no ESP32 |
| Presença | **LD2410C** | Radar mmWave — detecta pessoa parada, o PIR não |
| Alimentação | Fonte 5 V / 2 A + regulador 3V3 3 A | O SPS30 puxa picos de 80 mA no ventilador |

> **Sensores a evitar neste tipo de trabalho:** série MQ-x (MQ-135, MQ-7) para
> CO₂/CO — são sensores de gás resistivos, sem seletividade, com deriva enorme e
> sem calibração rastreável. Não sustentam laudo. LDR para iluminância — resposta
> espectral errada e não linear.

### 2.2 Ligações (ESP32-S3)

```
I²C  (VEML7700, AS7341, SCD41, SHT45, SPS30, SGP41, SFA30, FS3000)
  SDA ── GPIO8      SCL ── GPIO9      100 kHz (o SPS30 não aceita 400 kHz)
  Pull-ups de 4k7 para 3V3 — a maioria dos módulos já traz

I²S  (microfone ICS-43434 / INMP441)
  WS/LRCL ── GPIO15    SCK/BCLK ── GPIO16    SD/DOUT ── GPIO7
  L/R ao GND (canal esquerdo)

ADC  (fotodiodo de flicker)
  Saída do OPT101 ── GPIO4   (ADC1_CH3 — ADC2 não funciona com o WiFi ligado)

UART (LD2410C)
  Sensor TX ── GPIO18 (RX do ESP32)     Sensor RX ── GPIO19 (TX do ESP32)
  256000 8N1
```

**Onde instalar:** altura de 1,10 m do piso (zona de respiração sentada), longe
de porta, janela, difusor de ar e de qualquer fonte de calor. O sensor de
iluminância deve ficar no **plano de trabalho** (altura da carteira, ~0,75 m), com
o fotodiodo voltado para cima e sem sombra de estrutura.

### 2.3 Calibração obrigatória

Sem estes dois passos os números não têm validade:

1. **Nível sonoro.** Coloque um decibelímetro classe 2 calibrado ao lado do nó e
   ajuste `MIC_OFFSET_DB` no `config.h` até as leituras coincidirem. A
   sensibilidade de fábrica do MEMS tem espalhamento de ±3 dB.
2. **CO₂.** O SCD41 faz autocalibração (ASC) assumindo exposição periódica a ar
   fresco de 400 ppm. Numa sala que nunca é ventilada isso deriva. Deixe o nó ao
   ar livre por 10 minutos a cada troca de semestre, ou desative a ASC e faça
   calibração forçada.

---

## 3. Instalação

### 3.1 Supabase

1. Abra o SQL Editor do projeto (sugestão: `encoder-dashboard`,
   `odnjbvsjqteqapppkkpc`, região `sa-east-1`) e execute **`supabase-setup.sql`**
   por inteiro. Ele cria tabelas, limites normativos, views, RLS e um seed de
   exemplo.

2. Cadastre os ambientes reais em `amb_ambientes`. O campo `tipo` precisa casar
   com um `tipo_ambiente` de `amb_limites` — é ele que decide qual linha da norma
   se aplica.

3. Gere o token de cada dispositivo e grave só o hash:

   ```sql
   -- token em claro vai no config.h do ESP32; o banco só guarda o sha256
   SELECT encode(digest('um-segredo-longo-e-aleatorio','sha256'),'hex');

   INSERT INTO amb_dispositivos (device_id, ambiente_id, token_hash)
   SELECT 'ESP32-SALA101', id, '<hash-gerado-acima>'
     FROM amb_ambientes WHERE codigo = 'SALA-101';
   ```

4. Cadastre quem recebe alerta:

   ```sql
   INSERT INTO amb_destinatarios (nome, destino, tipo, severidades, janela_ini, janela_fim)
   VALUES ('Manutenção', '5531999999999', 'individual', ARRAY['critico'], '06:00', '22:00'),
          ('Grupo Gestão', '120363...@g.us', 'grupo',    ARRAY['critico','atencao'], '07:00', '19:00');
   ```

   `ambientes` em `NULL` significa "todos". A janela horária evita acordar alguém
   às 3 da manhã por uma sala vazia.

### 3.2 Edge Functions

```bash
supabase link --project-ref odnjbvsjqteqapppkkpc

# a ingestão precisa aceitar requisição sem JWT — o ESP32 não tem sessão
supabase functions deploy ingest --no-verify-jwt
supabase functions deploy alerta-whatsapp

# WhatsApp via Evolution API (o VPS que já roda nos outros projetos)
supabase secrets set WHATSAPP_PROVIDER=evolution
supabase secrets set EVO_BASE_URL=http://SEU_VPS:8080
supabase secrets set EVO_INSTANCE=escola-una-v2
supabase secrets set EVO_APIKEY=...

# — ou — via WhatsApp Cloud API oficial da Meta
supabase secrets set WHATSAPP_PROVIDER=meta
supabase secrets set META_PHONE_NUMBER_ID=...
supabase secrets set META_TOKEN=...
supabase secrets set META_TEMPLATE=alerta_ambiente     # opcional, ver abaixo
```

**Qual provedor escolher.** A Evolution API entrega texto livre a qualquer hora e
para grupos, e já está de pé na sua infraestrutura — é o caminho mais curto. A
Cloud API da Meta é oficial e estável, mas **fora da janela de 24 h só entrega
template aprovado**, e não envia para grupos. Se for de Meta, cadastre um template
com 4 variáveis (ambiente, parâmetro, valor lido, norma) e ponha o nome em
`META_TEMPLATE`.

Resumo diário (opcional), com pg_cron:

```sql
SELECT cron.schedule('amb-resumo-diario', '0 21 * * 1-5', $$
  SELECT net.http_post(
    url     := 'https://odnjbvsjqteqapppkkpc.supabase.co/functions/v1/alerta-whatsapp',
    headers := '{"Content-Type":"application/json","Authorization":"Bearer <SERVICE_ROLE>"}'::jsonb,
    body    := '{"modo":"resumo"}'::jsonb);
$$);
```

### 3.3 Firmware

1. Arduino IDE → Placa **ESP32S3 Dev Module**, core 2.0.14+ ou 3.x.
2. Instale as bibliotecas listadas no cabeçalho do `.ino`.
3. Em `config.h` preencha `DEVICE_ID`, `DEVICE_TOKEN`, `SUPABASE_ANON_KEY` e
   `OCUPACAO_PROJETO`. Comente os `#define USA_*` dos sensores que não instalou.
4. Grave. Na primeira ligação o ESP32 sobe o AP `MonitorAmbiente-Setup`
   (senha `12345678`) — conecte pelo celular e escolha a rede do prédio.

### 3.4 Vercel

A pasta é estática. No projeto `teologia-una`
(`prj_0du13rbyeQRstqCcIIX2nMnozFRX`, time `voltenergyengenharia`) a dashboard sai
em `/monitor-ambientes/`. Para publicar como projeto próprio, aponte o root
directory para esta pasta — o `vercel.json` já mapeia `/ambientes`.

Antes de publicar, preencha `SUPABASE_KEY` no `index.html` com a **anon key**
(nunca a `service_role`). Sem isso a página abre em modo demonstração, com dados
sintéticos — útil para conferir o layout antes de o hardware existir.

---

## 4. Decisões de projeto que valem explicar

**A chave anon é somente leitura.** Toda escrita passa pela Edge Function `ingest`,
que valida o token do dispositivo e usa a `service_role` internamente. Um ESP32
roubado da parede não consegue apagar histórico nem alterar os limites da norma.
As tabelas `amb_dispositivos` e `amb_destinatarios` não têm policy para `anon` —
guardam hash de token e números de telefone.

**Alerta só depois de três leituras seguidas fora.** Um pico isolado (alguém abriu
a porta, passou na frente do sensor, acendeu a luz) não dispara WhatsApp. E o
alerta só fecha quando o valor volta 5% para dentro do limite — sem essa banda
morta ele ficaria abrindo e fechando em torno da fronteira.

**Sala vazia não gera alerta.** Iluminância, ruído, flicker, temperatura de cor e
vazão por pessoa só são avaliados com presença detectada (coluna
`amb_limites.so_ocupado`). Sem isso o sistema acusaria "8 lx — não conforme" às
três da manhã, todas as noites, e em duas semanas ninguém mais leria as
notificações.

**"Atenção" está dentro da norma.** É a margem antes de violar, não uma
reprovação — por isso conta no índice de conformidade. Em faixa fechada
(temperatura 22,5–25,5 °C) essa margem é % da **largura da faixa**, não do valor
do limite: 8% de 25,5 °C daria 2 °C e marcaria 23,5 °C, o meio da zona de
conforto, como se estivesse à beira de violar.

**O gráfico de barras usa uma escala normalizada.** "% do limite normativo" põe
teto (CO₂), piso (iluminância) e faixa (temperatura) no mesmo eixo, com 100%
sempre significando "encostado no limite" e acima de 100% sempre significando
"fora". Sem essa normalização as barras não seriam comparáveis entre si.

---

## 5. Arquivos

```
monitor-ambientes/
├── index.html                                  dashboard (Vercel)
├── vercel.json
├── supabase-setup.sql                          schema + norma + RLS + views
├── supabase/functions/
│   ├── ingest/index.ts                         telemetria → avaliação → alerta
│   └── alerta-whatsapp/index.ts                Evolution API / Meta Cloud API
└── firmware/esp32_monitor_ambiente/
    ├── esp32_monitor_ambiente.ino
    └── config.h
```

---

## 6. Limitações declaradas

Vale ser explícito sobre o que este sistema **não** é, principalmente se ele
for apresentado a um cliente:

- **Não substitui o ensaio normativo.** A verificação formal da NBR 17037 é
  semestral, com instrumento calibrado e rastreável, e inclui coleta
  microbiológica em laboratório. Este sistema monitora **tendência e operação**
  entre um ensaio e outro, e mostra o que passou despercebido no intervalo.
- **UGR e Ra não são medidos.** Vêm do projeto luminotécnico. A dashboard os
  exibe como parâmetros de projeto, separados dos medidos.
- **A TCC pelo AS7341 é estimativa.** 8 bandas espectrais erram tipicamente
  algumas centenas de kelvin — serve para conferir se a luminária instalada é
  4000 K, não para ensaio fotométrico.
- **Fungos e bactérias exigem laboratório.** Nenhum sensor eletrônico os conta.
- **Sem calibração de campo, o nível sonoro não vale.** Ver § 2.3.
