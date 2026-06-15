-- Adicionar suporte para status_rastreio = 6 (ENTREGUE / FINALIZADO)
-- A coluna já existe, apenas garantindo que suporte o novo valor
ALTER TABLE pedidos_compra
ALTER COLUMN status_rastreio SET DEFAULT 1;

-- Adicionar coluna data_entrega se não existir
ALTER TABLE pedidos_compra
ADD COLUMN IF NOT EXISTS data_entrega TIMESTAMPTZ;
