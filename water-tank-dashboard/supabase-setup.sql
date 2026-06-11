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

-- ─── VIEW: lista de condominios (para o seletor da dashboard) ──
CREATE OR REPLACE VIEW condominios AS
SELECT condominio, MAX(created_at) AS ultima_leitura
FROM leituras
GROUP BY condominio
ORDER BY condominio;
