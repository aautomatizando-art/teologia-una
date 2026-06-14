-- Rastrear se pedido já foi entregue ao estoque
ALTER TABLE pedidos_op ADD COLUMN IF NOT EXISTS entregue_ao_estoque boolean NOT NULL DEFAULT false;
