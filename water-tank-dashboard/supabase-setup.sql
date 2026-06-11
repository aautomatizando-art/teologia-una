-- ═══════════════════════════════════════════════════════
-- Dashboard Caixa d'Agua (ESP-NOW) — Supabase Schema Setup
-- Execute no SQL Editor: https://app.supabase.com -> SQL Editor
-- ═══════════════════════════════════════════════════════

-- Tabela de leituras enviadas pelo ESP32 Gateway
CREATE TABLE IF NOT EXISTS leituras (
  id                          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  condominio                  TEXT NOT NULL DEFAULT 'Condominio Principal',
  distancia_cm                NUMERIC,
  nivel_pct                    INT,
  entrada1_bomba_ligada        BOOLEAN DEFAULT FALSE,  -- Entrada 1: Bomba ligou
  entrada2_bomba_falhou        BOOLEAN DEFAULT FALSE,  -- Entrada 2: Bomba falhou
  entrada3_falha_inversor      BOOLEAN DEFAULT FALSE,  -- Entrada 3: Falha no inversor
  entrada4_painel_sem_energia  BOOLEAN DEFAULT FALSE,  -- Entrada 4: Painel sem energia
  sensor_uptime_s              BIGINT,
  leitura_valida               BOOLEAN DEFAULT TRUE,
  created_at                   TIMESTAMPTZ DEFAULT NOW()
);

-- Migracao para tabelas criadas antes do suporte multi-condominio:
ALTER TABLE leituras ADD COLUMN IF NOT EXISTS condominio TEXT NOT NULL DEFAULT 'Condominio Principal';

CREATE INDEX IF NOT EXISTS idx_leituras_created_at ON leituras(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_leituras_condominio ON leituras(condominio, created_at DESC);

-- ─── ROW LEVEL SECURITY ────────────────────────────────
ALTER TABLE leituras ENABLE ROW LEVEL SECURITY;

-- Permite leitura (dashboard) e escrita (ESP32 Gateway) via chave anon
DROP POLICY IF EXISTS "anon_leituras" ON leituras;
CREATE POLICY "anon_leituras" ON leituras FOR ALL USING (true) WITH CHECK (true);

-- ─── REALTIME ──────────────────────────────────────────
-- Necessario para a dashboard atualizar sozinha quando chega leitura nova
DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM pg_publication_tables
    WHERE pubname = 'supabase_realtime' AND tablename = 'leituras'
  ) THEN
    ALTER PUBLICATION supabase_realtime ADD TABLE leituras;
  END IF;
END $$;

-- ─── VIEW: ultima leitura de cada condominio ───────────
CREATE OR REPLACE VIEW ultima_leitura AS
SELECT DISTINCT ON (condominio) *
FROM leituras
ORDER BY condominio, created_at DESC;


-- ═══════════════════════════════════════════════════════
-- EXPANSAO: Tanque Inferior, Alarme de Incendio (ESP32-CAM),
-- Bombas de Incendio, Cancela da Portaria e Agua da Rua
-- ═══════════════════════════════════════════════════════

-- ─── TANQUE INFERIOR (ESP32 #3 - WiFi direto) ──────────────────
CREATE TABLE IF NOT EXISTS tanque_inferior (
  id                          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  condominio                  TEXT NOT NULL DEFAULT 'Condominio Principal',
  distancia_cm                NUMERIC,
  nivel_pct                    INT,
  entrada2_bomba_falhou        BOOLEAN DEFAULT FALSE,  -- Entrada 2: Bomba falhou
  entrada3_falha_inversor      BOOLEAN DEFAULT FALSE,  -- Entrada 3: Falha no inversor
  entrada4_painel_sem_energia  BOOLEAN DEFAULT FALSE,  -- Entrada 4: Painel sem energia
  temperatura_c                NUMERIC,                 -- sensor de temperatura (ex: DS18B20)
  vibracao_valor               NUMERIC,                 -- leitura do sensor de vibracao
  vibracao_alerta              BOOLEAN DEFAULT FALSE,   -- vibracao acima do limite
  sensor_uptime_s              BIGINT,
  leitura_valida               BOOLEAN DEFAULT TRUE,
  created_at                   TIMESTAMPTZ DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_tanque_inferior_created_at ON tanque_inferior(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_tanque_inferior_condominio ON tanque_inferior(condominio, created_at DESC);
ALTER TABLE tanque_inferior ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "anon_tanque_inferior" ON tanque_inferior;
CREATE POLICY "anon_tanque_inferior" ON tanque_inferior FOR ALL USING (true) WITH CHECK (true);
DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM pg_publication_tables
    WHERE pubname = 'supabase_realtime' AND tablename = 'tanque_inferior'
  ) THEN
    ALTER PUBLICATION supabase_realtime ADD TABLE tanque_inferior;
  END IF;
END $$;
CREATE OR REPLACE VIEW ultima_tanque_inferior AS
SELECT DISTINCT ON (condominio) *
FROM tanque_inferior
ORDER BY condominio, created_at DESC;


-- ─── ALARME DE INCENDIO (ESP32-CAM) ─────────────────────────────
CREATE TABLE IF NOT EXISTS alarme_incendio (
  id                        BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  condominio                TEXT NOT NULL DEFAULT 'Condominio Principal',
  entrada1_alarme_acionado  BOOLEAN DEFAULT FALSE,  -- Entrada 1: Alarme acionado
  entrada2_central_avaria   BOOLEAN DEFAULT FALSE,  -- Entrada 2: Central com avaria/falha
  status                    TEXT NOT NULL DEFAULT 'normal',  -- normal | alarme | avaria
  foto_url                  TEXT,                    -- URL publica da foto (Supabase Storage)
  foto_atualizada_em        TIMESTAMPTZ,
  sensor_uptime_s           BIGINT,
  created_at                TIMESTAMPTZ DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_alarme_incendio_created_at ON alarme_incendio(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_alarme_incendio_condominio ON alarme_incendio(condominio, created_at DESC);
ALTER TABLE alarme_incendio ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "anon_alarme_incendio" ON alarme_incendio;
CREATE POLICY "anon_alarme_incendio" ON alarme_incendio FOR ALL USING (true) WITH CHECK (true);
DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM pg_publication_tables
    WHERE pubname = 'supabase_realtime' AND tablename = 'alarme_incendio'
  ) THEN
    ALTER PUBLICATION supabase_realtime ADD TABLE alarme_incendio;
  END IF;
END $$;
CREATE OR REPLACE VIEW ultima_alarme_incendio AS
SELECT DISTINCT ON (condominio) *
FROM alarme_incendio
ORDER BY condominio, created_at DESC;

-- Bucket de Storage para as fotos enviadas pelo ESP32-CAM (leitura publica)
INSERT INTO storage.buckets (id, name, public)
VALUES ('fotos-incendio', 'fotos-incendio', true)
ON CONFLICT (id) DO NOTHING;

DROP POLICY IF EXISTS "fotos_incendio_select" ON storage.objects;
CREATE POLICY "fotos_incendio_select" ON storage.objects
  FOR SELECT USING (bucket_id = 'fotos-incendio');

DROP POLICY IF EXISTS "fotos_incendio_insert" ON storage.objects;
CREATE POLICY "fotos_incendio_insert" ON storage.objects
  FOR INSERT WITH CHECK (bucket_id = 'fotos-incendio');

DROP POLICY IF EXISTS "fotos_incendio_update" ON storage.objects;
CREATE POLICY "fotos_incendio_update" ON storage.objects
  FOR UPDATE USING (bucket_id = 'fotos-incendio');

DROP POLICY IF EXISTS "fotos_incendio_delete" ON storage.objects;
CREATE POLICY "fotos_incendio_delete" ON storage.objects
  FOR DELETE USING (bucket_id = 'fotos-incendio');


-- ─── BOMBAS DE INCENDIO ──────────────────────────────────────────
CREATE TABLE IF NOT EXISTS bombas_incendio (
  id                                BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  condominio                        TEXT NOT NULL DEFAULT 'Condominio Principal',
  entrada1_bomba_principal_ligada   BOOLEAN DEFAULT FALSE,  -- Entrada 1: Bomba principal de incendio ligada
  entrada2_falha_bomba              BOOLEAN DEFAULT FALSE,  -- Entrada 2: Falha na bomba de incendio
  entrada3_bomba_reserva_ligada     BOOLEAN DEFAULT FALSE,  -- Entrada 3: Bomba reserva de incendio ligada
  sensor_uptime_s                   BIGINT,
  created_at                        TIMESTAMPTZ DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_bombas_incendio_created_at ON bombas_incendio(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_bombas_incendio_condominio ON bombas_incendio(condominio, created_at DESC);
ALTER TABLE bombas_incendio ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "anon_bombas_incendio" ON bombas_incendio;
CREATE POLICY "anon_bombas_incendio" ON bombas_incendio FOR ALL USING (true) WITH CHECK (true);
DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM pg_publication_tables
    WHERE pubname = 'supabase_realtime' AND tablename = 'bombas_incendio'
  ) THEN
    ALTER PUBLICATION supabase_realtime ADD TABLE bombas_incendio;
  END IF;
END $$;
CREATE OR REPLACE VIEW ultima_bombas_incendio AS
SELECT DISTINCT ON (condominio) *
FROM bombas_incendio
ORDER BY condominio, created_at DESC;


-- ─── CANCELA DA PORTARIA ─────────────────────────────────────────
CREATE TABLE IF NOT EXISTS cancela_portaria (
  id                       BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  condominio               TEXT NOT NULL DEFAULT 'Condominio Principal',
  entrada1_cancela1_falha  BOOLEAN DEFAULT FALSE,  -- Entrada 1: Cancela 1 em falha
  entrada2_cancela2_falha  BOOLEAN DEFAULT FALSE,  -- Entrada 2: Cancela 2 em falha
  sensor_uptime_s          BIGINT,
  created_at               TIMESTAMPTZ DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_cancela_portaria_created_at ON cancela_portaria(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_cancela_portaria_condominio ON cancela_portaria(condominio, created_at DESC);
ALTER TABLE cancela_portaria ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "anon_cancela_portaria" ON cancela_portaria;
CREATE POLICY "anon_cancela_portaria" ON cancela_portaria FOR ALL USING (true) WITH CHECK (true);
DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM pg_publication_tables
    WHERE pubname = 'supabase_realtime' AND tablename = 'cancela_portaria'
  ) THEN
    ALTER PUBLICATION supabase_realtime ADD TABLE cancela_portaria;
  END IF;
END $$;
CREATE OR REPLACE VIEW ultima_cancela_portaria AS
SELECT DISTINCT ON (condominio) *
FROM cancela_portaria
ORDER BY condominio, created_at DESC;


-- ─── AGUA DA RUA ──────────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS agua_rua (
  id              BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  condominio      TEXT NOT NULL DEFAULT 'Condominio Principal',
  fluxo_lpm       NUMERIC,                  -- vazao estimada em litros/minuto
  fluxo_ativo     BOOLEAN DEFAULT TRUE,     -- false = agua da rua parou
  sensor_uptime_s BIGINT,
  created_at      TIMESTAMPTZ DEFAULT NOW()
);
CREATE INDEX IF NOT EXISTS idx_agua_rua_created_at ON agua_rua(created_at DESC);
CREATE INDEX IF NOT EXISTS idx_agua_rua_condominio ON agua_rua(condominio, created_at DESC);
ALTER TABLE agua_rua ENABLE ROW LEVEL SECURITY;
DROP POLICY IF EXISTS "anon_agua_rua" ON agua_rua;
CREATE POLICY "anon_agua_rua" ON agua_rua FOR ALL USING (true) WITH CHECK (true);
DO $$
BEGIN
  IF NOT EXISTS (
    SELECT 1 FROM pg_publication_tables
    WHERE pubname = 'supabase_realtime' AND tablename = 'agua_rua'
  ) THEN
    ALTER PUBLICATION supabase_realtime ADD TABLE agua_rua;
  END IF;
END $$;
CREATE OR REPLACE VIEW ultima_agua_rua AS
SELECT DISTINCT ON (condominio) *
FROM agua_rua
ORDER BY condominio, created_at DESC;


-- ─── VIEW: lista de condominios (para o seletor da dashboard) ──
-- Substitui a versao anterior (so olhava "leituras"): agora considera
-- qualquer um dos ESP32 do condominio, mesmo que o tanque superior
-- ainda nao tenha enviado nenhuma leitura.
CREATE OR REPLACE VIEW condominios AS
SELECT condominio, MAX(created_at) AS ultima_leitura
FROM (
  SELECT condominio, created_at FROM leituras
  UNION ALL SELECT condominio, created_at FROM tanque_inferior
  UNION ALL SELECT condominio, created_at FROM alarme_incendio
  UNION ALL SELECT condominio, created_at FROM bombas_incendio
  UNION ALL SELECT condominio, created_at FROM cancela_portaria
  UNION ALL SELECT condominio, created_at FROM agua_rua
) t
GROUP BY condominio
ORDER BY condominio;
