-- Adicionar campos de localização e lote na tabela produtos
ALTER TABLE produtos
ADD COLUMN IF NOT EXISTS rua text,
ADD COLUMN IF NOT EXISTS prateleira text,
ADD COLUMN IF NOT EXISTS lote text;
