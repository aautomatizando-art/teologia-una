-- Adicionar campos de insumos (consumo por caixa produzida) na tabela produtos
ALTER TABLE produtos
ADD COLUMN IF NOT EXISTS kg_batata_por_caixa numeric,
ADD COLUMN IF NOT EXISTS caixa_por_caixa numeric,
ADD COLUMN IF NOT EXISTS kg_filme_bopp_por_caixa numeric,
ADD COLUMN IF NOT EXISTS kg_condimento_por_caixa numeric,
ADD COLUMN IF NOT EXISTS kg_oleo_por_caixa numeric,
ADD COLUMN IF NOT EXISTS cm_fita_adesiva_por_caixa numeric;
