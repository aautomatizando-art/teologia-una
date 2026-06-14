-- Rastrear quando uma OP foi finalizada automaticamente (100%)
ALTER TABLE ordens_producao ADD COLUMN IF NOT EXISTS finalizado_em timestamptz;
