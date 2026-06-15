-- Registrar data/hora de cada etapa do rastreio (1 a 6) e nome de quem recebeu a mercadoria
ALTER TABLE pedidos_compra
ADD COLUMN IF NOT EXISTS rastreio_timestamps jsonb DEFAULT '{}'::jsonb,
ADD COLUMN IF NOT EXISTS recebido_por text;
