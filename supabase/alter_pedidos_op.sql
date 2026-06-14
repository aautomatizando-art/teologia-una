-- Adicionar coluna para rastrear quantidade produzida de cada pedido
ALTER TABLE pedidos_op ADD COLUMN IF NOT EXISTS quantidade_produzida integer NOT NULL DEFAULT 0;

-- Seed: OP-1001 e OP-1002 já têm pedidos_op, então inicializamos com 0 (ou valores demo)
-- UPDATE pedidos_op SET quantidade_produzida = 0; -- já padrão
