-- Adicionar coluna caixas_por_palete na tabela produtos
ALTER TABLE produtos
ADD COLUMN IF NOT EXISTS caixas_por_palete integer NOT NULL DEFAULT 1;
