-- ═══════════════════════════════════════════════════════
-- ESP32 Encoder Dashboard — Supabase Schema Setup
-- Execute no SQL Editor: https://app.supabase.com → SQL Editor
-- ═══════════════════════════════════════════════════════

-- 1. CLICHÊS (vida útil individual por modelo)
CREATE TABLE IF NOT EXISTS cliches (
  id            INT PRIMARY KEY,          -- 1 a 10
  nome          TEXT NOT NULL,
  vida_util_m   NUMERIC NOT NULL DEFAULT 1000,
  consumido_m   NUMERIC NOT NULL DEFAULT 0,
  updated_at    TIMESTAMPTZ DEFAULT NOW()
);

INSERT INTO cliches (id, nome, vida_util_m, consumido_m) VALUES
  (1,'Clichê 1',1000,0),(2,'Clichê 2',1000,0),(3,'Clichê 3',1000,0),
  (4,'Clichê 4',1000,0),(5,'Clichê 5',1000,0),(6,'Clichê 6',1000,0),
  (7,'Clichê 7',1000,0),(8,'Clichê 8',1000,0),(9,'Clichê 9',1000,0),
  (10,'Clichê 10',1000,0)
ON CONFLICT (id) DO NOTHING;

-- 2. ORDENS DE PRODUÇÃO
CREATE TABLE IF NOT EXISTS ordens (
  id               UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  numero           TEXT NOT NULL,
  cliche_id        INT REFERENCES cliches(id),
  data_inicio      TIMESTAMPTZ DEFAULT NOW(),
  data_fim         TIMESTAMPTZ,
  tempo_producao_s INT DEFAULT 0,
  tempo_parada_s   INT DEFAULT 0,
  posicao_total_m  NUMERIC DEFAULT 0,
  status           TEXT DEFAULT 'aberta'    -- aberta | fechada
);

-- 3. PARADAS
CREATE TABLE IF NOT EXISTS paradas (
  id        UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  ordem_id  UUID REFERENCES ordens(id) ON DELETE CASCADE,
  tipo      TEXT NOT NULL,   -- Manutenção | Setup | Falta de Material | etc.
  inicio    TIMESTAMPTZ NOT NULL,
  fim       TIMESTAMPTZ,
  duracao_s INT
);

-- ─── ROW LEVEL SECURITY ────────────────────────────────
ALTER TABLE cliches ENABLE ROW LEVEL SECURITY;
ALTER TABLE ordens  ENABLE ROW LEVEL SECURITY;
ALTER TABLE paradas ENABLE ROW LEVEL SECURITY;

-- Permite leitura e escrita via chave anon (dashboard)
CREATE POLICY "anon_cliches" ON cliches FOR ALL USING (true) WITH CHECK (true);
CREATE POLICY "anon_ordens"  ON ordens  FOR ALL USING (true) WITH CHECK (true);
CREATE POLICY "anon_paradas" ON paradas FOR ALL USING (true) WITH CHECK (true);

-- ─── ÍNDICES ───────────────────────────────────────────
CREATE INDEX IF NOT EXISTS idx_ordens_numero     ON ordens(numero);
CREATE INDEX IF NOT EXISTS idx_ordens_cliche     ON ordens(cliche_id);
CREATE INDEX IF NOT EXISTS idx_paradas_ordem     ON paradas(ordem_id);
CREATE INDEX IF NOT EXISTS idx_paradas_tipo      ON paradas(tipo);

-- ─── VIEW ÚTIL ─────────────────────────────────────────
CREATE OR REPLACE VIEW resumo_ordens AS
SELECT
  o.id, o.numero, c.nome AS cliche,
  o.data_inicio, o.data_fim,
  o.tempo_producao_s, o.tempo_parada_s, o.posicao_total_m,
  o.status,
  ROUND((o.tempo_producao_s::NUMERIC / NULLIF(o.tempo_producao_s + o.tempo_parada_s, 0)) * 100, 1) AS disponibilidade_pct
FROM ordens o
LEFT JOIN cliches c ON c.id = o.cliche_id
ORDER BY o.data_inicio DESC;
