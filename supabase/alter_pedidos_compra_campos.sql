-- Adicionar campos de cliente, região, comprador e vendedor à tabela pedidos_compra
ALTER TABLE pedidos_compra
ADD COLUMN IF NOT EXISTS nome_cliente text,
ADD COLUMN IF NOT EXISTS regiao text,
ADD COLUMN IF NOT EXISTS comprador text,
ADD COLUMN IF NOT EXISTS vendedor text;
