-- ═══════════════════════════════════════════════════════
-- Dashboard Caixa d'Agua (ESP-NOW) — Supabase Schema Setup
-- Execute no SQL Editor: https://app.supabase.com -> SQL Editor
-- ═══════════════════════════════════════════════════════

-- Tabela de leituras enviadas pelo ESP32 Gateway
CREATE TABLE IF NOT EXISTS leituras (
  id                          BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
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

CREATE INDEX IF NOT EXISTS idx_leituras_created_at ON leituras(created_at DESC);

-- ─── ROW LEVEL SECURITY ────────────────────────────────
ALTER TABLE leituras ENABLE ROW LEVEL SECURITY;

-- Permite leitura (dashboard) e escrita (ESP32 Gateway) via chave anon
CREATE POLICY "anon_leituras" ON leituras FOR ALL USING (true) WITH CHECK (true);

-- ─── REALTIME ──────────────────────────────────────────
-- Necessario para a dashboard atualizar sozinha quando chega leitura nova
ALTER PUBLICATION supabase_realtime ADD TABLE leituras;

-- ─── VIEW: ultima leitura ──────────────────────────────
CREATE OR REPLACE VIEW ultima_leitura AS
SELECT * FROM leituras ORDER BY created_at DESC LIMIT 1;
