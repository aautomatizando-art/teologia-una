-- ═══════════════════════════════════════════════════════════════════════════════
-- MONITOR DE AMBIENTES — Qualidade do Ar e Iluminação segundo normas ABNT
-- Schema Supabase (PostgreSQL 15+)
--
-- Execute no SQL Editor: https://app.supabase.com -> SQL Editor
-- Projeto sugerido: encoder-dashboard (odnjbvsjqteqapppkkpc) ou projeto novo
-- em sa-east-1.
--
-- Prefixo de tabelas: amb_  (evita colisão com as tabelas já existentes)
--
-- Normas de referência embarcadas no seed de amb_limites:
--   ABNT NBR ISO/CIE 8995-1:2013 — Iluminação de ambientes de trabalho
--   ABNT NBR 17037:2023          — Qualidade do ar interior (substituiu a
--                                  Resolução RE 09/2003 da ANVISA)
--   ABNT NBR 16401-2:2008        — Parâmetros de conforto térmico
--   ABNT NBR 16401-3:2008        — Qualidade do ar interior (ventilação)
--   ABNT NBR 10152:2017          — Níveis de pressão sonora em ambientes internos
--   NR-17 (Portaria MTP 423/2021)— Ergonomia / conforto em atividades intelectuais
--   OMS AQG 2021                 — Material particulado, formaldeído, CO
-- ═══════════════════════════════════════════════════════════════════════════════

-- ─────────────────────────────────────────────────────────────────────────────
-- 1. AMBIENTES MONITORADOS
-- ─────────────────────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS amb_ambientes (
  id             UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  codigo         TEXT NOT NULL UNIQUE,          -- 'SALA-101', 'ADM-02'
  nome           TEXT NOT NULL,
  tipo           TEXT NOT NULL,                 -- FK lógica -> amb_limites.tipo_ambiente
  predio         TEXT,
  andar          TEXT,
  area_m2        NUMERIC,
  pe_direito_m   NUMERIC DEFAULT 3.0,
  ocupacao_max   INT,                           -- nº de pessoas de projeto
  climatizado    BOOLEAN NOT NULL DEFAULT TRUE, -- ar-condicionado? (muda o alvo térmico)
  observacao     TEXT,
  ativo          BOOLEAN NOT NULL DEFAULT TRUE,
  criado_em      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

COMMENT ON COLUMN amb_ambientes.tipo IS
  'Casa com amb_limites.tipo_ambiente — define qual linha da norma se aplica.';

-- ─────────────────────────────────────────────────────────────────────────────
-- 2. DISPOSITIVOS (ESP32)
-- ─────────────────────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS amb_dispositivos (
  id             UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  device_id      TEXT NOT NULL UNIQUE,          -- MAC sem ':' ou etiqueta física
  ambiente_id    UUID REFERENCES amb_ambientes(id) ON DELETE SET NULL,
  modelo         TEXT DEFAULT 'ESP32-S3-WROOM-1',
  fw_versao      TEXT,
  token_hash     TEXT NOT NULL,                 -- sha256(token) em hex, minúsculo
  sensores       JSONB DEFAULT '{}'::JSONB,     -- {"veml7700":true,"scd41":true,...}
  ultimo_contato TIMESTAMPTZ,
  rssi_dbm       INT,
  uptime_s       BIGINT,
  ip             TEXT,
  ativo          BOOLEAN NOT NULL DEFAULT TRUE,
  criado_em      TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

COMMENT ON COLUMN amb_dispositivos.token_hash IS
  'sha256 do segredo gravado no config.h do ESP32. O segredo em claro nunca é '
  'armazenado. Gere com: select encode(digest(''MEU_TOKEN'',''sha256''),''hex'');';

-- ─────────────────────────────────────────────────────────────────────────────
-- 3. CATÁLOGO DE PARÂMETROS MENSURÁVEIS
-- ─────────────────────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS amb_parametros (
  chave        TEXT PRIMARY KEY,
  nome         TEXT NOT NULL,
  unidade      TEXT NOT NULL,
  grupo        TEXT NOT NULL,        -- 'iluminacao' | 'ar' | 'termico' | 'acustico'
  norma        TEXT NOT NULL,
  sentido      TEXT NOT NULL,        -- 'maior_melhor' | 'menor_melhor' | 'faixa'
  origem       TEXT NOT NULL,        -- 'sensor' | 'levantamento' | 'projeto'
  casas        INT NOT NULL DEFAULT 0,
  descricao    TEXT,
  ordem        INT NOT NULL DEFAULT 100
);

COMMENT ON COLUMN amb_parametros.origem IS
  'sensor       = o ESP32 mede continuamente;'
  'levantamento = medição de campo em vários pontos (luxímetro em malha), '
  '               lançada manualmente ou por vários sensores na mesma sala;'
  'projeto      = vem do memorial luminotécnico / ficha do fabricante e NÃO é '
  '               medível por sensor de campo (UGR e Ra são deste grupo). '
  'A norma cobra todos, mas só os de origem=sensor podem ser monitorados 24/7.';

INSERT INTO amb_parametros (chave, nome, unidade, grupo, norma, sentido, origem, casas, descricao, ordem) VALUES
  ('lux',     'Iluminância mantida (Ēm)',   'lx',        'iluminacao', 'ABNT NBR ISO/CIE 8995-1:2013', 'faixa',        'sensor',       0, 'Iluminância média mantida sobre a área de tarefa.', 10),
  ('ugr',     'Ofuscamento (UGRL)',         '—',         'iluminacao', 'ABNT NBR ISO/CIE 8995-1:2013', 'menor_melhor', 'projeto',      0, 'Calculado no projeto luminotécnico (DIALux/Relux) a partir da luminância da luminária e da geometria. Não existe sensor de campo para UGR.', 20),
  ('uo',      'Uniformidade (Uo)',          '—',         'iluminacao', 'ABNT NBR ISO/CIE 8995-1:2013', 'maior_melhor', 'levantamento', 2, 'Emin/Ēm — exige malha de pontos conforme NBR ISO/CIE 8995-1 Anexo B.', 30),
  ('ra',      'Reprodução de cor (Ra)',     '—',         'iluminacao', 'ABNT NBR ISO/CIE 8995-1:2013', 'maior_melhor', 'projeto',      0, 'Característica da lâmpada — vem do datasheet, não do sensor.', 40),
  ('tcc',     'Temperatura de cor (TCC)',   'K',         'iluminacao', 'ABNT NBR ISO/CIE 8995-1:2013', 'faixa',        'sensor',       0, 'Estimada pelo sensor espectral AS7341. 4000 K é o alvo usual em salas de aula.', 50),
  ('flicker', 'Modulação de flicker',       '%',         'iluminacao', 'IEEE 1789-2015 / NBR ISO/CIE 8995-1 §4.10', 'menor_melhor', 'sensor', 1, 'Percentual de modulação da luz. A NBR exige ausência de cintilação e de efeito estroboscópico; o IEEE 1789 quantifica o limite.', 60),

  ('co2',     'Dióxido de carbono (CO₂)',   'ppm',       'ar',         'ABNT NBR 17037:2023',          'menor_melhor', 'sensor',       0, 'Indicador de renovação de ar. Limite é DIFERENCIAL sobre o ar externo.', 70),
  ('pm25',    'Material particulado MP2,5', 'µg/m³',     'ar',         'ABNT NBR 17037:2023 / OMS AQG 2021', 'menor_melhor', 'sensor', 1, 'Substituiu os aerodispersóides da RE 09/2003.', 80),
  ('pm10',    'Material particulado MP10',  'µg/m³',     'ar',         'ABNT NBR 17037:2023 / OMS AQG 2021', 'menor_melhor', 'sensor', 1, 'Substituiu os aerodispersóides da RE 09/2003.', 90),
  ('voc',     'Índice de COV (VOC Index)',  '—',         'ar',         'ISO 16000-29 / WELL v2 A01',   'menor_melhor', 'sensor',       0, 'Escala 1–500 do SGP41; 100 = linha de base do ambiente.', 100),
  ('nox',     'Índice de NOx',              '—',         'ar',         'ISO 16000-29',                 'menor_melhor', 'sensor',       0, 'Escala 1–500 do SGP41; 1 = linha de base.', 110),
  ('hcho',    'Formaldeído (HCHO)',         'µg/m³',     'ar',         'OMS AQG (média 30 min)',       'menor_melhor', 'sensor',       0, 'Emitido por MDF, colas, mobiliário novo e produtos de limpeza.', 120),
  ('co',      'Monóxido de carbono (CO)',   'ppm',       'ar',         'OMS AQG 2021 (média 8 h)',     'menor_melhor', 'sensor',       1, 'Relevante onde há garagem, gerador ou combustão próxima.', 130),
  ('ar_ext',  'Vazão de ar exterior',       'L/s·pessoa','ar',         'ABNT NBR 16401-3:2008',        'maior_melhor', 'sensor',       1, 'Estimada pelo balanço de CO₂ em regime permanente. Ensaio de comissionamento usa anemômetro no difusor.', 140),

  ('temp',    'Temperatura de bulbo seco',  '°C',        'termico',    'ABNT NBR 16401-2:2008',        'faixa',        'sensor',       1, 'Zona de conforto para 80% de aceitação dos ocupantes.', 150),
  ('ur',      'Umidade relativa',           '%',         'termico',    'ABNT NBR 16401-2:2008 / NR-17','faixa',        'sensor',       0, 'Teto de 65% controla proliferação microbiana; piso de 40% pela NR-17.', 160),
  ('vel_ar',  'Velocidade do ar',           'm/s',       'termico',    'ABNT NBR 16401-2:2008',        'menor_melhor', 'sensor',       2, 'Acima do limite gera corrente de ar desconfortável (draft).', 170),

  ('ruido',   'Nível sonoro (LAeq)',        'dB(A)',     'acustico',   'ABNT NBR 10152:2017',          'menor_melhor', 'sensor',       1, 'Nível equivalente ponderado em A, medido no período de uso.', 180)
ON CONFLICT (chave) DO UPDATE SET
  nome = EXCLUDED.nome, unidade = EXCLUDED.unidade, grupo = EXCLUDED.grupo,
  norma = EXCLUDED.norma, sentido = EXCLUDED.sentido, origem = EXCLUDED.origem,
  casas = EXCLUDED.casas, descricao = EXCLUDED.descricao, ordem = EXCLUDED.ordem;

-- ─────────────────────────────────────────────────────────────────────────────
-- 4. LIMITES NORMATIVOS  (a norma virando dado consultável)
-- ─────────────────────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS amb_limites (
  id              BIGSERIAL PRIMARY KEY,
  tipo_ambiente   TEXT NOT NULL,
  parametro       TEXT NOT NULL REFERENCES amb_parametros(chave) ON DELETE CASCADE,
  minimo          NUMERIC,        -- NULL = sem limite inferior
  maximo          NUMERIC,        -- NULL = sem limite superior
  alvo            NUMERIC,        -- valor de referência da norma (a linha "ideal" do gráfico)
  delta_externo   BOOLEAN NOT NULL DEFAULT FALSE, -- se TRUE, 'maximo' soma-se à leitura externa
  so_ocupado      BOOLEAN NOT NULL DEFAULT FALSE, -- só avalia com o ambiente em uso
  tol_atencao_pct NUMERIC NOT NULL DEFAULT 10,    -- margem antes de violar -> faixa de ATENÇÃO
  norma           TEXT NOT NULL,
  observacao      TEXT,
  UNIQUE (tipo_ambiente, parametro)
);

COMMENT ON COLUMN amb_limites.delta_externo IS
  'NBR 17037:2023 define o CO₂ como no máximo 700 ppm ACIMA do ar externo, e não '
  'como um teto fixo. Com esta flag o limite efetivo vira maximo + co2_ext_ppm.';
COMMENT ON COLUMN amb_limites.alvo IS
  'Valor indicado pela norma — é a série "índice ideal" plotada contra o lido.';

-- ── 4.1 INSTITUIÇÕES EDUCACIONAIS ────────────────────────────────────────────
-- Iluminação: ABNT NBR ISO/CIE 8995-1:2013, Tabela 5.36 (educacionais)
INSERT INTO amb_limites (tipo_ambiente, parametro, minimo, maximo, alvo, delta_externo, tol_atencao_pct, norma, observacao) VALUES
  -- Sala de aula (ensino regular / diurno)
  ('sala_aula','lux',     300,  750,  300, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Ēm = 300 lx na área de tarefa (carteiras). Acima de 750 lx indica superdimensionamento.'),
  ('sala_aula','ugr',    NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', 'UGRL = 19.'),
  ('sala_aula','uo',     0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Uo ≥ 0,60.'),
  ('sala_aula','ra',       80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Ra ≥ 80.'),
  ('sala_aula','tcc',    3000, 6000, 4000, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §4.5',  'Branco neutro (4000 K) é o usual para tarefa visual prolongada.'),
  ('sala_aula','flicker',NULL, 10.0,  4.0, FALSE, 10, 'IEEE 1789-2015 / NBR ISO/CIE 8995-1 §4.10', 'Em 120 Hz (ondulação de rede 60 Hz): ≤ 9,6% é a zona de baixo risco e ≤ 4% é o nível sem efeito observável.'),
  ('sala_aula','co2',    NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               'Máx. 700 ppm ACIMA do ar externo. Legado RE 09/2003: teto fixo de 1000 ppm.'),
  ('sala_aula','pm25',   NULL, 15.0,  5.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021','15 µg/m³ média 24 h; 5 µg/m³ média anual.'),
  ('sala_aula','pm10',   NULL, 45.0, 15.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021','45 µg/m³ média 24 h; 15 µg/m³ média anual.'),
  ('sala_aula','voc',    NULL,  250,  100, FALSE, 20, 'ISO 16000-29 / WELL v2',       'Índice do SGP41; 100 = linha de base aprendida do ambiente.'),
  ('sala_aula','nox',    NULL,   20,    1, FALSE, 25, 'ISO 16000-29',                 'Índice do SGP41; 1 = linha de base.'),
  ('sala_aula','hcho',   NULL,  100,   30, FALSE, 20, 'OMS AQG (30 min)',             'Mobiliário e piso novos elevam o HCHO por semanas.'),
  ('sala_aula','co',     NULL,  9.0,  0.0, FALSE, 20, 'OMS AQG 2021 (8 h)',           'Monitorar onde houver garagem/gerador contíguo.'),
  ('sala_aula','temp',   22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      'Verão, 0,5 clo, 1,0 met, UR 65%. Inverno (0,9 clo): 21,0–23,5 °C.'),
  ('sala_aula','ur',       40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     'Teto de 65% (controle microbiano); piso de 40% (NR-17 item 17.8).'),
  ('sala_aula','vel_ar', NULL, 0.20, 0.15, FALSE, 20, 'NBR 16401-2:2008 §5.3',        'Verão ≤ 0,20 m/s; inverno ≤ 0,15 m/s na zona ocupada.'),
  ('sala_aula','ar_ext',  3.8, NULL,  3.8, FALSE, 10, 'NBR 16401-3:2008 Tab. 1',      'Nível 1: 3,8 L/s por pessoa + 0,3 L/s por m² de piso.'),
  ('sala_aula','ruido',  NULL, 45.0, 35.0, FALSE, 10, 'NBR 10152:2017',               'Faixa 35–45 dB(A): 35 é o conforto adequado, 45 o limite superior.'),

  -- Sala de aula noturna / educação de adultos (Ēm dobra para 500 lx)
  ('sala_aula_noturna','lux',    500, 1000,  500, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Ēm = 500 lx — sem contribuição de luz natural.'),
  ('sala_aula_noturna','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_aula_noturna','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_aula_noturna','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_aula_noturna','tcc',   3000, 6000, 4000, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §4.5',  NULL),
  ('sala_aula_noturna','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               NULL),
  ('sala_aula_noturna','pm25',  NULL, 15.0,  5.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021', NULL),
  ('sala_aula_noturna','pm10',  NULL, 45.0, 15.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021', NULL),
  ('sala_aula_noturna','voc',   NULL,  250,  100, FALSE, 20, 'ISO 16000-29 / WELL v2',       NULL),
  ('sala_aula_noturna','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      NULL),
  ('sala_aula_noturna','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('sala_aula_noturna','vel_ar',NULL, 0.20, 0.15, FALSE, 20, 'NBR 16401-2:2008 §5.3',        NULL),
  ('sala_aula_noturna','ar_ext', 3.8, NULL,  3.8, FALSE, 10, 'NBR 16401-3:2008 Tab. 1',      NULL),
  ('sala_aula_noturna','ruido', NULL, 45.0, 35.0, FALSE, 10, 'NBR 10152:2017',               NULL),

  -- Quadro-negro / lousa (plano vertical — exige uniformidade maior)
  ('quadro','lux',    500, 1000,  500, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Ēm = 500 lx no plano vertical do quadro.'),
  ('quadro','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Evitar reflexão especular em quadro branco.'),
  ('quadro','uo',    0.70, NULL, 0.70, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Uo ≥ 0,70 — mais exigente que a sala.'),
  ('quadro','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),

  -- Laboratório / sala de prática
  ('laboratorio','lux',    500, 1000,  500, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('laboratorio','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('laboratorio','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('laboratorio','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('laboratorio','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               NULL),
  ('laboratorio','pm25',  NULL, 15.0,  5.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021', NULL),
  ('laboratorio','pm10',  NULL, 45.0, 15.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021', NULL),
  ('laboratorio','voc',   NULL,  250,  100, FALSE, 20, 'ISO 16000-29 / WELL v2',       'Solventes e reagentes elevam o índice rapidamente.'),
  ('laboratorio','hcho',  NULL,  100,   30, FALSE, 20, 'OMS AQG (30 min)',             NULL),
  ('laboratorio','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      NULL),
  ('laboratorio','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('laboratorio','vel_ar',NULL, 0.20, 0.15, FALSE, 20, 'NBR 16401-2:2008 §5.3',        NULL),
  ('laboratorio','ruido', NULL, 50.0, 40.0, FALSE, 10, 'NBR 10152:2017',               NULL),

  -- Sala de professores
  ('sala_professores','lux',    300,  750,  300, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_professores','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_professores','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_professores','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_professores','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               NULL),
  ('sala_professores','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      NULL),
  ('sala_professores','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('sala_professores','ruido', NULL, 45.0, 35.0, FALSE, 10, 'NBR 10152:2017',               NULL),

  -- Biblioteca (mesas de leitura)
  ('biblioteca','lux',    500, 1000,  500, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Leitura: 500 lx. Estantes: 200 lx (cadastrar como ambiente próprio).'),
  ('biblioteca','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('biblioteca','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('biblioteca','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('biblioteca','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               NULL),
  ('biblioteca','pm25',  NULL, 15.0,  5.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021', NULL),
  ('biblioteca','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      NULL),
  ('biblioteca','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('biblioteca','ruido', NULL, 45.0, 35.0, FALSE, 10, 'NBR 10152:2017',               NULL),

  -- Auditório / sala de conferências
  ('auditorio','lux',    500, 1000,  500, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Com dimerização para projeção.'),
  ('auditorio','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('auditorio','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('auditorio','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('auditorio','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               'Ocupação alta e intermitente — o CO₂ sobe rápido.'),
  ('auditorio','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      NULL),
  ('auditorio','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('auditorio','ruido', NULL, 40.0, 30.0, FALSE, 10, 'NBR 10152:2017',               'Faixa 30–40 dB(A) — mais exigente pela inteligibilidade da fala.'),

  -- Sala de informática
  ('sala_informatica','lux',    300,  750,  300, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.36', 'Ēm reduzida para limitar reflexo em telas.'),
  ('sala_informatica','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_informatica','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_informatica','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.36', NULL),
  ('sala_informatica','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               NULL),
  ('sala_informatica','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      NULL),
  ('sala_informatica','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('sala_informatica','ruido', NULL, 45.0, 35.0, FALSE, 10, 'NBR 10152:2017',               NULL),

-- ── 4.2 ESCRITÓRIOS ──────────────────────────────────────────────────────────
-- Iluminação: ABNT NBR ISO/CIE 8995-1:2013, Tabela 5.26 (escritórios)
  -- Escritório privativo / individual (escrita, digitação, leitura, dados)
  ('escritorio','lux',    500, 1000,  500, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.26', 'Ēm = 500 lx sobre a mesa de trabalho.'),
  ('escritorio','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', 'UGRL = 19 — crítico com monitores.'),
  ('escritorio','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('escritorio','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('escritorio','tcc',   3000, 6000, 4000, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §4.5',  NULL),
  ('escritorio','flicker',NULL,10.0,  4.0, FALSE, 10, 'IEEE 1789-2015',               'Driver de LED barato é a causa mais comum de reprovação neste item.'),
  ('escritorio','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               'Máx. 700 ppm acima do externo.'),
  ('escritorio','pm25',  NULL, 15.0,  5.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021', NULL),
  ('escritorio','pm10',  NULL, 45.0, 15.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021', NULL),
  ('escritorio','voc',   NULL,  250,  100, FALSE, 20, 'ISO 16000-29 / WELL v2',       NULL),
  ('escritorio','nox',   NULL,   20,    1, FALSE, 25, 'ISO 16000-29',                 NULL),
  ('escritorio','hcho',  NULL,  100,   30, FALSE, 20, 'OMS AQG (30 min)',             NULL),
  ('escritorio','co',    NULL,  9.0,  0.0, FALSE, 20, 'OMS AQG 2021 (8 h)',           NULL),
  ('escritorio','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      'NR-17 recomenda 20–23 °C para atividades intelectuais.'),
  ('escritorio','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('escritorio','vel_ar',NULL, 0.20, 0.15, FALSE, 20, 'NBR 16401-2:2008 §5.3',        'NR-17 limita a 0,75 m/s; a NBR é mais restritiva.'),
  ('escritorio','ar_ext', 2.5, NULL,  2.5, FALSE, 10, 'NBR 16401-3:2008 Tab. 1',      'Nível 1: 2,5 L/s por pessoa + 0,3 L/s por m².'),
  ('escritorio','ruido', NULL, 45.0, 35.0, FALSE, 10, 'NBR 10152:2017',               NULL),

  -- Escritório panorâmico / open plan
  ('escritorio_aberto','lux',    500, 1000,  500, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('escritorio_aberto','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('escritorio_aberto','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('escritorio_aberto','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('escritorio_aberto','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               NULL),
  ('escritorio_aberto','pm25',  NULL, 15.0,  5.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021', NULL),
  ('escritorio_aberto','pm10',  NULL, 45.0, 15.0, FALSE, 20, 'NBR 17037:2023 / OMS AQG 2021', NULL),
  ('escritorio_aberto','voc',   NULL,  250,  100, FALSE, 20, 'ISO 16000-29 / WELL v2',       NULL),
  ('escritorio_aberto','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      NULL),
  ('escritorio_aberto','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('escritorio_aberto','vel_ar',NULL, 0.20, 0.15, FALSE, 20, 'NBR 16401-2:2008 §5.3',        NULL),
  ('escritorio_aberto','ar_ext', 2.5, NULL,  2.5, FALSE, 10, 'NBR 16401-3:2008 Tab. 1',      NULL),
  ('escritorio_aberto','ruido', NULL, 50.0, 40.0, FALSE, 10, 'NBR 10152:2017',               'Faixa 40–50 dB(A) — conversas cruzadas são a principal fonte.'),

  -- Sala de reunião / conferência
  ('sala_reuniao','lux',    500, 1000,  500, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.26', 'Preferencialmente dimerizável.'),
  ('sala_reuniao','ugr',   NULL,   19,   19, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('sala_reuniao','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('sala_reuniao','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('sala_reuniao','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               'Densidade de ocupação alta — é o pior caso de CO₂ do prédio.'),
  ('sala_reuniao','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      NULL),
  ('sala_reuniao','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('sala_reuniao','vel_ar',NULL, 0.20, 0.15, FALSE, 20, 'NBR 16401-2:2008 §5.3',        NULL),
  ('sala_reuniao','ruido', NULL, 45.0, 35.0, FALSE, 10, 'NBR 10152:2017',               NULL),

  -- Desenho técnico / CAD
  ('desenho_tecnico','lux',  750, 1500,  750, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.26', 'Ēm = 750 lx.'),
  ('desenho_tecnico','ugr', NULL,   16,   16, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', 'UGRL = 16 — o mais restritivo dos escritórios.'),
  ('desenho_tecnico','uo',  0.70, NULL, 0.70, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('desenho_tecnico','ra',    80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),

  -- Balcão de recepção
  ('recepcao','lux',    300,  750,  300, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('recepcao','ugr',   NULL,   22,   22, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', 'UGRL = 22.'),
  ('recepcao','uo',    0.60, NULL, 0.60, FALSE,  8, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('recepcao','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.26', NULL),
  ('recepcao','co2',   NULL,  700,  500, TRUE,  15, 'NBR 17037:2023',               NULL),
  ('recepcao','temp',  22.5, 25.5, 24.0, FALSE,  8, 'NBR 16401-2:2008 Tab. 1',      NULL),
  ('recepcao','ur',      40,   65,   50, FALSE, 10, 'NBR 16401-2:2008 / NR-17',     NULL),
  ('recepcao','ruido', NULL, 50.0, 40.0, FALSE, 10, 'NBR 10152:2017',               NULL),

  -- Circulação / corredores
  ('corredor','lux',    100,  300,  100, FALSE, 15, 'NBR ISO/CIE 8995-1:2013 §5.1',  'Ēm = 100 lx.'),
  ('corredor','ugr',   NULL,   25,   25, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.1',  NULL),
  ('corredor','uo',    0.40, NULL, 0.40, FALSE, 10, 'NBR ISO/CIE 8995-1:2013 §5.1',  NULL),
  ('corredor','ra',      80, NULL,   80, FALSE,  5, 'NBR ISO/CIE 8995-1:2013 §5.1',  NULL),
  ('corredor','temp',  20.0, 27.0, 24.0, FALSE, 10, 'NBR 16401-2:2008',              'Área de passagem — faixa ampliada.')
ON CONFLICT (tipo_ambiente, parametro) DO UPDATE SET
  minimo = EXCLUDED.minimo, maximo = EXCLUDED.maximo, alvo = EXCLUDED.alvo,
  delta_externo = EXCLUDED.delta_externo, tol_atencao_pct = EXCLUDED.tol_atencao_pct,
  norma = EXCLUDED.norma, observacao = EXCLUDED.observacao;

-- Parâmetros que só fazem sentido com o ambiente EM USO. Sem isto o sistema
-- acorda o zelador às 3 da manhã porque a sala de aula vazia, de luz apagada,
-- está com 8 lx — o que não é uma não conformidade, é uma sala vazia.
-- O mesmo vale para ruído (silêncio não é problema), temperatura de cor e
-- flicker (luminária desligada) e vazão de ar por pessoa (sem pessoas).
UPDATE amb_limites SET so_ocupado = TRUE
 WHERE parametro IN ('lux', 'tcc', 'flicker', 'ruido', 'ar_ext');

CREATE INDEX IF NOT EXISTS idx_amb_limites_tipo ON amb_limites(tipo_ambiente);

-- ─────────────────────────────────────────────────────────────────────────────
-- 5. LEITURAS (telemetria)
-- ─────────────────────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS amb_leituras (
  id            BIGSERIAL PRIMARY KEY,
  ambiente_id   UUID NOT NULL REFERENCES amb_ambientes(id) ON DELETE CASCADE,
  device_id     TEXT NOT NULL,
  lido_em       TIMESTAMPTZ NOT NULL DEFAULT NOW(),

  -- Iluminação
  lux           NUMERIC,
  ugr           NUMERIC,
  uo            NUMERIC,
  ra            NUMERIC,
  tcc           NUMERIC,
  flicker       NUMERIC,

  -- Qualidade do ar
  co2           NUMERIC,
  co2_ext       NUMERIC,   -- referência externa p/ o limite diferencial da NBR 17037
  pm1           NUMERIC,
  pm25          NUMERIC,
  pm4           NUMERIC,
  pm10          NUMERIC,
  voc           NUMERIC,
  nox           NUMERIC,
  hcho          NUMERIC,
  co            NUMERIC,
  ar_ext        NUMERIC,   -- L/s·pessoa estimado pelo balanço de CO₂

  -- Conforto térmico
  temp          NUMERIC,
  ur            NUMERIC,
  vel_ar        NUMERIC,
  pressao_hpa   NUMERIC,

  -- Acústica
  ruido         NUMERIC,   -- LAeq do intervalo
  ruido_max     NUMERIC,   -- LAFmax do intervalo

  -- Contexto
  ocupacao      INT,
  presenca      BOOLEAN,
  rssi_dbm      INT,
  uptime_s      BIGINT,
  bruto         JSONB
);

CREATE INDEX IF NOT EXISTS idx_amb_leituras_amb_data
  ON amb_leituras(ambiente_id, lido_em DESC);
CREATE INDEX IF NOT EXISTS idx_amb_leituras_data
  ON amb_leituras(lido_em DESC);

-- ─────────────────────────────────────────────────────────────────────────────
-- 6. ALERTAS
-- ─────────────────────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS amb_alertas (
  id            UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  ambiente_id   UUID NOT NULL REFERENCES amb_ambientes(id) ON DELETE CASCADE,
  parametro     TEXT NOT NULL REFERENCES amb_parametros(chave),
  severidade    TEXT NOT NULL CHECK (severidade IN ('atencao','critico')),
  valor         NUMERIC NOT NULL,
  limite_min    NUMERIC,
  limite_max    NUMERIC,
  desvio_pct    NUMERIC,          -- quanto excedeu o limite violado, em %
  norma         TEXT,
  mensagem      TEXT NOT NULL,
  aberto_em     TIMESTAMPTZ NOT NULL DEFAULT NOW(),
  fechado_em    TIMESTAMPTZ,
  notificado_em TIMESTAMPTZ,
  canal         TEXT,             -- 'whatsapp'
  erro_envio    TEXT
);

CREATE INDEX IF NOT EXISTS idx_amb_alertas_abertos
  ON amb_alertas(ambiente_id, parametro) WHERE fechado_em IS NULL;
CREATE INDEX IF NOT EXISTS idx_amb_alertas_data ON amb_alertas(aberto_em DESC);

-- ─────────────────────────────────────────────────────────────────────────────
-- 7. DESTINATÁRIOS DAS NOTIFICAÇÕES WHATSAPP
-- ─────────────────────────────────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS amb_destinatarios (
  id           UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  nome         TEXT NOT NULL,
  destino      TEXT NOT NULL,     -- '5511999999999' ou '1203634...@g.us' (grupo)
  tipo         TEXT NOT NULL DEFAULT 'individual' CHECK (tipo IN ('individual','grupo')),
  ambientes    UUID[],            -- NULL/vazio = todos os ambientes
  severidades  TEXT[] NOT NULL DEFAULT ARRAY['critico'],
  janela_ini   TIME DEFAULT '06:00',   -- não incomodar fora da janela
  janela_fim   TIME DEFAULT '22:00',
  ativo        BOOLEAN NOT NULL DEFAULT TRUE,
  criado_em    TIMESTAMPTZ NOT NULL DEFAULT NOW()
);

-- ─────────────────────────────────────────────────────────────────────────────
-- 8. FUNÇÕES DE AVALIAÇÃO
-- ─────────────────────────────────────────────────────────────────────────────

-- Classifica um valor contra a faixa normativa.
-- 'atencao' é a margem tol_atencao_pct antes de efetivamente violar o limite —
-- ainda DENTRO da norma, mas já encostando.
--
-- A margem é calculada de formas diferentes conforme o tipo de limite:
--   faixa fechada (temperatura 22,5–25,5 °C) -> % da LARGURA da faixa. Usar
--     % do valor do limite seria errado em escalas que não começam no zero:
--     8% de 25,5 °C dá 2 °C, o que marcaria 23,5 °C — o meio da zona de
--     conforto — como se estivesse à beira de violar a norma.
--   limite simples (CO₂ ≤ 1120 ppm)          -> % do próprio limite.
CREATE OR REPLACE FUNCTION amb_status_valor(
  p_valor   NUMERIC,
  p_minimo  NUMERIC,
  p_maximo  NUMERIC,
  p_tol_pct NUMERIC
) RETURNS TEXT
LANGUAGE sql IMMUTABLE AS $$
  SELECT CASE
    WHEN p_valor IS NULL THEN 'sem_dado'
    WHEN p_maximo IS NOT NULL AND p_valor > p_maximo THEN 'nao_conforme'
    WHEN p_minimo IS NOT NULL AND p_valor < p_minimo THEN 'nao_conforme'
    WHEN p_minimo IS NOT NULL AND p_maximo IS NOT NULL THEN
      CASE WHEN p_valor >= p_maximo - (p_maximo - p_minimo) * COALESCE(p_tol_pct,0)/100.0
             OR p_valor <= p_minimo + (p_maximo - p_minimo) * COALESCE(p_tol_pct,0)/100.0
           THEN 'atencao' ELSE 'conforme' END
    WHEN p_maximo IS NOT NULL AND p_valor >= p_maximo * (1 - COALESCE(p_tol_pct,0)/100.0) THEN 'atencao'
    WHEN p_minimo IS NOT NULL AND p_valor <= p_minimo * (1 + COALESCE(p_tol_pct,0)/100.0) THEN 'atencao'
    ELSE 'conforme'
  END;
$$;

-- Transforma uma leitura (linha larga) em pares (parametro, valor).
CREATE OR REPLACE FUNCTION amb_desempilhar(p_leitura amb_leituras)
RETURNS TABLE (parametro TEXT, valor NUMERIC)
LANGUAGE sql IMMUTABLE AS $$
  SELECT * FROM (VALUES
    ('lux',     p_leitura.lux),
    ('ugr',     p_leitura.ugr),
    ('uo',      p_leitura.uo),
    ('ra',      p_leitura.ra),
    ('tcc',     p_leitura.tcc),
    ('flicker', p_leitura.flicker),
    ('co2',     p_leitura.co2),
    ('pm25',    p_leitura.pm25),
    ('pm10',    p_leitura.pm10),
    ('voc',     p_leitura.voc),
    ('nox',     p_leitura.nox),
    ('hcho',    p_leitura.hcho),
    ('co',      p_leitura.co),
    ('ar_ext',  p_leitura.ar_ext),
    ('temp',    p_leitura.temp),
    ('ur',      p_leitura.ur),
    ('vel_ar',  p_leitura.vel_ar),
    ('ruido',   p_leitura.ruido)
  ) AS t(parametro, valor)
  WHERE t.valor IS NOT NULL;
$$;

-- ─────────────────────────────────────────────────────────────────────────────
-- 9. VIEWS
-- ─────────────────────────────────────────────────────────────────────────────

-- Última leitura de cada ambiente (linha bruta, por dispositivo mais recente).
CREATE OR REPLACE VIEW amb_v_ultima_leitura AS
SELECT DISTINCT ON (l.ambiente_id) l.*
FROM amb_leituras l
ORDER BY l.ambiente_id, l.lido_em DESC;

-- Janela de validade de uma medida. Passado esse tempo sem leitura nova, o
-- parâmetro vira 'sem_dado' em vez de exibir eternamente um valor velho —
-- sensor mudo tem que aparecer como mudo, não como conforme.
CREATE OR REPLACE FUNCTION amb_validade() RETURNS INTERVAL
LANGUAGE sql IMMUTABLE AS $$ SELECT INTERVAL '2 hours' $$;

-- ─────────────────────────────────────────────────────────────────────────
-- VALOR ATUAL POR PARÂMETRO — consolidando TODOS os dispositivos da sala
--
-- Uma sala costuma ter dois nós: o de parede (ar, térmico, acústico, a 1,10 m)
-- e o de teto (iluminância, cor, flicker, presença). Cada um envia um conjunto
-- diferente de colunas. Pegar "a última linha da sala" faria os parâmetros
-- piscarem entre presente e ausente conforme qual nó falou por último.
-- Aqui a consolidação é por PARÂMETRO: o valor não nulo mais recente, venha
-- do nó que vier.
-- ─────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE VIEW amb_v_valores_atuais AS
SELECT DISTINCT ON (l.ambiente_id, d.parametro)
  l.ambiente_id,
  d.parametro,
  d.valor,
  l.lido_em,
  l.device_id
FROM amb_leituras l
CROSS JOIN LATERAL amb_desempilhar(l.*) d
WHERE l.lido_em > NOW() - amb_validade()
ORDER BY l.ambiente_id, d.parametro, l.lido_em DESC;

-- Contexto da sala (presença e CO₂ externo) também consolidado entre nós:
-- quem detecta presença é o nó de teto, quem mede CO₂ é o de parede.
CREATE OR REPLACE VIEW amb_v_contexto_atual AS
SELECT
  a.id AS ambiente_id,
  (SELECT l.presenca FROM amb_leituras l
    WHERE l.ambiente_id = a.id AND l.presenca IS NOT NULL
      AND l.lido_em > NOW() - amb_validade()
    ORDER BY l.lido_em DESC LIMIT 1)  AS presenca,
  (SELECT l.co2_ext FROM amb_leituras l
    WHERE l.ambiente_id = a.id AND l.co2_ext IS NOT NULL
      AND l.lido_em > NOW() - amb_validade()
    ORDER BY l.lido_em DESC LIMIT 1)  AS co2_ext,
  (SELECT MAX(l.lido_em) FROM amb_leituras l
    WHERE l.ambiente_id = a.id)       AS lido_em
FROM amb_ambientes a;

-- Conformidade atual: uma linha por (ambiente × parâmetro), com o valor lido,
-- o limite efetivo da norma e o status. É a fonte dos cards e do gráfico de
-- "% do limite normativo".
--
-- Lê de amb_v_valores_atuais, então funciona igual com um nó por sala ou com
-- vários (parede + teto), sem que um apague os parâmetros do outro.
CREATE OR REPLACE VIEW amb_v_conformidade AS
SELECT
  a.id                AS ambiente_id,
  a.codigo            AS ambiente_codigo,
  a.nome              AS ambiente_nome,
  a.tipo              AS ambiente_tipo,
  p.chave             AS parametro,
  p.nome              AS parametro_nome,
  p.unidade,
  p.grupo,
  p.sentido,
  p.origem,
  p.casas,
  v.lido_em,
  v.device_id,
  v.valor             AS valor,
  lim.minimo          AS limite_min,
  -- Limite efetivo: a NBR 17037 define o CO₂ como diferencial sobre o externo.
  CASE WHEN lim.delta_externo
       THEN lim.maximo + COALESCE(ctx.co2_ext, 420)
       ELSE lim.maximo
  END                 AS limite_max,
  CASE WHEN lim.delta_externo
       THEN lim.alvo + COALESCE(ctx.co2_ext, 420)
       ELSE lim.alvo
  END                 AS alvo,
  lim.tol_atencao_pct,
  lim.delta_externo,
  lim.so_ocupado,
  ctx.presenca,
  lim.norma,
  lim.observacao,
  -- Parâmetro marcado como so_ocupado com o ambiente vazio não é avaliado:
  -- sala escura e silenciosa fora do expediente não é não conformidade.
  CASE WHEN lim.so_ocupado AND ctx.presenca IS FALSE THEN 'sem_dado'
       ELSE amb_status_valor(
              v.valor,
              lim.minimo,
              CASE WHEN lim.delta_externo THEN lim.maximo + COALESCE(ctx.co2_ext, 420) ELSE lim.maximo END,
              lim.tol_atencao_pct)
  END                 AS status,
  -- Uso da faixa normativa, normalizado para que 100% signifique SEMPRE
  -- "encostado no limite" e >100% signifique SEMPRE "fora da norma",
  -- independente de o parâmetro ter teto (CO₂), piso (lux) ou faixa (temp).
  -- É o que permite comparar parâmetros de naturezas diferentes no mesmo eixo.
  CASE
    -- Faixa fechada: distância do centro, em % da semiamplitude.
    WHEN lim.minimo IS NOT NULL AND lim.maximo IS NOT NULL THEN
      ROUND(100.0 * ABS(v.valor - (lim.minimo + lim.maximo) / 2.0)
            / NULLIF((lim.maximo - lim.minimo) / 2.0, 0), 1)
    -- Só teto: quanto do teto está sendo consumido.
    WHEN lim.maximo IS NOT NULL THEN
      ROUND(100.0 * v.valor / NULLIF(
        CASE WHEN lim.delta_externo THEN lim.maximo + COALESCE(ctx.co2_ext, 420) ELSE lim.maximo END, 0), 1)
    -- Só piso: invertido, para que ficar abaixo do mínimo também passe de 100%.
    WHEN lim.minimo IS NOT NULL THEN
      ROUND(100.0 * lim.minimo / NULLIF(v.valor, 0), 1)
    ELSE NULL
  END                 AS pct_limite,
  p.ordem
FROM amb_ambientes a
JOIN amb_v_valores_atuais v  ON v.ambiente_id = a.id
LEFT JOIN amb_v_contexto_atual ctx ON ctx.ambiente_id = a.id
JOIN amb_limites lim  ON lim.tipo_ambiente = a.tipo AND lim.parametro = v.parametro
JOIN amb_parametros p ON p.chave = v.parametro
WHERE a.ativo;

-- Índice de conformidade por ambiente: % dos parâmetros avaliados que estão
-- DENTRO da faixa normativa na última leitura.
--
-- 'atencao' conta como dentro da norma: significa que o valor se aproximou do
-- limite (dentro da margem tol_atencao_pct), não que o ultrapassou. Contá-lo
-- como reprovação inflaria o índice de não conformidade sem respaldo na norma.
-- Parâmetros com status 'sem_dado' — inclusive os so_ocupado com o ambiente
-- vazio — ficam fora do denominador.
CREATE OR REPLACE VIEW amb_v_indice_ambiente AS
SELECT
  ambiente_id,
  ambiente_codigo,
  ambiente_nome,
  ambiente_tipo,
  MAX(lido_em)                                                      AS lido_em,
  COUNT(*) FILTER (WHERE status <> 'sem_dado')                      AS parametros,
  COUNT(*) FILTER (WHERE status = 'conforme')                       AS conformes,
  COUNT(*) FILTER (WHERE status = 'atencao')                        AS em_atencao,
  COUNT(*) FILTER (WHERE status = 'nao_conforme')                   AS nao_conformes,
  COUNT(*) FILTER (WHERE status = 'sem_dado')                       AS sem_dado,
  ROUND(100.0 * COUNT(*) FILTER (WHERE status IN ('conforme','atencao'))
        / NULLIF(COUNT(*) FILTER (WHERE status <> 'sem_dado'), 0), 1)
                                                                    AS indice_pct,
  CASE
    WHEN COUNT(*) FILTER (WHERE status = 'nao_conforme') > 0 THEN 'nao_conforme'
    WHEN COUNT(*) FILTER (WHERE status = 'atencao')      > 0 THEN 'atencao'
    ELSE 'conforme'
  END                                                               AS status
FROM amb_v_conformidade
GROUP BY ambiente_id, ambiente_codigo, ambiente_nome, ambiente_tipo;

-- Série horária por ambiente — alimenta o gráfico temporal sem trazer
-- milhares de pontos brutos para o navegador.
CREATE OR REPLACE VIEW amb_v_serie_horaria AS
SELECT
  ambiente_id,
  date_trunc('hour', lido_em)     AS hora,
  COUNT(*)                        AS amostras,
  ROUND(AVG(lux)::NUMERIC, 0)     AS lux,
  ROUND(AVG(co2)::NUMERIC, 0)     AS co2,
  ROUND(AVG(co2_ext)::NUMERIC, 0) AS co2_ext,
  ROUND(AVG(temp)::NUMERIC, 1)    AS temp,
  ROUND(AVG(ur)::NUMERIC, 0)      AS ur,
  ROUND(AVG(pm25)::NUMERIC, 1)    AS pm25,
  ROUND(AVG(pm10)::NUMERIC, 1)    AS pm10,
  ROUND(AVG(voc)::NUMERIC, 0)     AS voc,
  ROUND(AVG(ruido)::NUMERIC, 1)   AS ruido,
  ROUND(MAX(ruido_max)::NUMERIC,1)AS ruido_max,
  ROUND(AVG(vel_ar)::NUMERIC, 2)  AS vel_ar,
  ROUND(AVG(hcho)::NUMERIC, 0)    AS hcho,
  ROUND(AVG(co)::NUMERIC, 1)      AS co,
  ROUND(AVG(ar_ext)::NUMERIC, 1)  AS ar_ext,
  ROUND(AVG(ocupacao)::NUMERIC, 0)AS ocupacao
FROM amb_leituras
GROUP BY ambiente_id, date_trunc('hour', lido_em);

-- Conformidade diária — histórico para relatório e tendência.
CREATE OR REPLACE VIEW amb_v_conformidade_diaria AS
SELECT
  l.ambiente_id,
  (l.lido_em AT TIME ZONE 'America/Sao_Paulo')::DATE AS dia,
  d.parametro,
  COUNT(*)                                                          AS amostras,
  -- Mesma definição de amb_v_indice_ambiente: 'atencao' ainda está DENTRO da
  -- norma. As três leituras de conformidade do sistema precisam concordar.
  COUNT(*) FILTER (WHERE amb_status_valor(
      d.valor, lim.minimo,
      CASE WHEN lim.delta_externo THEN lim.maximo + COALESCE(l.co2_ext,420) ELSE lim.maximo END,
      lim.tol_atencao_pct) IN ('conforme','atencao'))               AS dentro_norma,
  COUNT(*) FILTER (WHERE amb_status_valor(
      d.valor, lim.minimo,
      CASE WHEN lim.delta_externo THEN lim.maximo + COALESCE(l.co2_ext,420) ELSE lim.maximo END,
      lim.tol_atencao_pct) = 'nao_conforme')                        AS fora_norma,
  ROUND(100.0 * COUNT(*) FILTER (WHERE amb_status_valor(
      d.valor, lim.minimo,
      CASE WHEN lim.delta_externo THEN lim.maximo + COALESCE(l.co2_ext,420) ELSE lim.maximo END,
      lim.tol_atencao_pct) IN ('conforme','atencao')) / NULLIF(COUNT(*),0), 1)
                                                                    AS indice_pct,
  ROUND(AVG(d.valor), 2)                                            AS media,
  MIN(d.valor)                                                      AS minimo,
  MAX(d.valor)                                                      AS maximo
FROM amb_leituras l
JOIN amb_ambientes a ON a.id = l.ambiente_id
CROSS JOIN LATERAL amb_desempilhar(l.*) d
JOIN amb_limites lim ON lim.tipo_ambiente = a.tipo AND lim.parametro = d.parametro
-- Mesma regra da conformidade instantânea: amostra colhida com o ambiente
-- vazio não entra no cômputo dos parâmetros que só valem em uso. Sem isto o
-- índice diário afundaria por causa das noites e dos fins de semana.
WHERE NOT (lim.so_ocupado AND l.presenca IS FALSE)
GROUP BY l.ambiente_id, 2, d.parametro;

-- ─────────────────────────────────────────────────────────────────────────────
-- 10. ROW LEVEL SECURITY
--
-- Modelo: a chave anon do dashboard é SOMENTE LEITURA. Toda escrita passa
-- pelas Edge Functions, que usam a service_role e validam o token do
-- dispositivo. Um ESP32 comprometido não consegue apagar histórico nem
-- alterar os limites normativos.
-- ─────────────────────────────────────────────────────────────────────────────
ALTER TABLE amb_ambientes     ENABLE ROW LEVEL SECURITY;
ALTER TABLE amb_dispositivos  ENABLE ROW LEVEL SECURITY;
ALTER TABLE amb_parametros    ENABLE ROW LEVEL SECURITY;
ALTER TABLE amb_limites       ENABLE ROW LEVEL SECURITY;
ALTER TABLE amb_leituras      ENABLE ROW LEVEL SECURITY;
ALTER TABLE amb_alertas       ENABLE ROW LEVEL SECURITY;
ALTER TABLE amb_destinatarios ENABLE ROW LEVEL SECURITY;

DROP POLICY IF EXISTS amb_ambientes_leitura   ON amb_ambientes;
DROP POLICY IF EXISTS amb_parametros_leitura  ON amb_parametros;
DROP POLICY IF EXISTS amb_limites_leitura     ON amb_limites;
DROP POLICY IF EXISTS amb_leituras_leitura    ON amb_leituras;
DROP POLICY IF EXISTS amb_alertas_leitura     ON amb_alertas;

CREATE POLICY amb_ambientes_leitura  ON amb_ambientes  FOR SELECT TO anon, authenticated USING (ativo);
CREATE POLICY amb_parametros_leitura ON amb_parametros FOR SELECT TO anon, authenticated USING (true);
CREATE POLICY amb_limites_leitura    ON amb_limites    FOR SELECT TO anon, authenticated USING (true);
CREATE POLICY amb_leituras_leitura   ON amb_leituras   FOR SELECT TO anon, authenticated USING (true);
CREATE POLICY amb_alertas_leitura    ON amb_alertas    FOR SELECT TO anon, authenticated USING (true);

-- amb_dispositivos e amb_destinatarios ficam SEM policy para anon:
-- guardam token_hash e números de telefone. Só a service_role enxerga.

-- As views herdam a RLS das tabelas base (security_invoker), então o anon
-- lê apenas o que as policies acima permitem.
ALTER VIEW amb_v_ultima_leitura      SET (security_invoker = true);
ALTER VIEW amb_v_valores_atuais      SET (security_invoker = true);
ALTER VIEW amb_v_contexto_atual      SET (security_invoker = true);
ALTER VIEW amb_v_conformidade        SET (security_invoker = true);
ALTER VIEW amb_v_indice_ambiente     SET (security_invoker = true);
ALTER VIEW amb_v_serie_horaria       SET (security_invoker = true);
ALTER VIEW amb_v_conformidade_diaria SET (security_invoker = true);

-- Privilégio de tabela é uma camada separada da RLS: sem o GRANT, a policy
-- nunca chega a ser avaliada. O Supabase já concede por padrão, mas deixar
-- explícito evita um "permission denied" caso o default tenha sido alterado.
GRANT SELECT ON amb_ambientes, amb_parametros, amb_limites,
                amb_leituras, amb_alertas,
                amb_v_ultima_leitura, amb_v_valores_atuais, amb_v_contexto_atual,
                amb_v_conformidade, amb_v_indice_ambiente,
                amb_v_serie_horaria, amb_v_conformidade_diaria
  TO anon, authenticated;

-- amb_dispositivos e amb_destinatarios ficam de fora de propósito.
REVOKE ALL ON amb_dispositivos, amb_destinatarios FROM anon, authenticated;

-- Realtime para o dashboard atualizar sozinho.
-- Em bloco tolerante: reexecutar o script inteiro é rotina, e um ALTER
-- PUBLICATION repetido aborta a transação com "already member of publication".
DO $$
BEGIN
  BEGIN ALTER PUBLICATION supabase_realtime ADD TABLE amb_leituras;
  EXCEPTION WHEN duplicate_object THEN NULL; END;
  BEGIN ALTER PUBLICATION supabase_realtime ADD TABLE amb_alertas;
  EXCEPTION WHEN duplicate_object THEN NULL; END;
END $$;

-- ─────────────────────────────────────────────────────────────────────────────
-- 11. RETENÇÃO — mantém o banco enxuto no plano free
-- ─────────────────────────────────────────────────────────────────────────────
CREATE OR REPLACE FUNCTION amb_limpar_historico(p_dias INT DEFAULT 180)
RETURNS INT LANGUAGE plpgsql SECURITY DEFINER
SET search_path = public AS $$
DECLARE v_removidas INT;
BEGIN
  DELETE FROM amb_leituras WHERE lido_em < NOW() - (p_dias || ' days')::INTERVAL;
  GET DIAGNOSTICS v_removidas = ROW_COUNT;
  DELETE FROM amb_alertas WHERE fechado_em IS NOT NULL
                            AND fechado_em < NOW() - (p_dias || ' days')::INTERVAL;
  RETURN v_removidas;
END;
$$;

-- Agende com pg_cron (Database -> Extensions -> pg_cron), 03:00 todo dia 1:
--   SELECT cron.schedule('amb-limpeza','0 3 1 * *','SELECT amb_limpar_historico(180)');

-- ─────────────────────────────────────────────────────────────────────────────
-- 12. SEED DE EXEMPLO — troque pelos ambientes reais
-- ─────────────────────────────────────────────────────────────────────────────
INSERT INTO amb_ambientes (codigo, nome, tipo, predio, andar, area_m2, ocupacao_max, climatizado) VALUES
  ('SALA-101', 'Sala 101 — Teologia I',   'sala_aula',        'Bloco A', '1º', 48.0, 40, TRUE),
  ('SALA-102', 'Sala 102 — Noturno',      'sala_aula_noturna','Bloco A', '1º', 48.0, 40, TRUE),
  ('ADM-01',   'Secretaria Acadêmica',    'escritorio',       'Bloco B', 'Térreo', 24.0, 6, TRUE),
  ('ADM-02',   'Sala de Reunião',         'sala_reuniao',     'Bloco B', '1º', 18.0, 12, TRUE),
  ('BIB-01',   'Biblioteca — Leitura',    'biblioteca',       'Bloco C', 'Térreo', 90.0, 30, TRUE)
ON CONFLICT (codigo) DO NOTHING;

-- Cadastro do dispositivo. Gere o hash a partir do token que vai no config.h:
--   CREATE EXTENSION IF NOT EXISTS pgcrypto;
--   SELECT encode(digest('TROQUE_ESTE_TOKEN','sha256'),'hex');
CREATE EXTENSION IF NOT EXISTS pgcrypto;

INSERT INTO amb_dispositivos (device_id, ambiente_id, fw_versao, token_hash, sensores)
SELECT 'ESP32-SALA101',
       a.id,
       '1.0.0',
       encode(digest('TROQUE_ESTE_TOKEN','sha256'),'hex'),
       '{"veml7700":true,"scd41":true,"sht45":true,"sps30":true,"sgp41":true,"inmp441":true,"fs3000":true,"ld2410":true}'::JSONB
FROM amb_ambientes a WHERE a.codigo = 'SALA-101'
ON CONFLICT (device_id) DO NOTHING;

-- ═══════════════════════════════════════════════════════════════════════════════
-- FIM
-- ═══════════════════════════════════════════════════════════════════════════════
