-- Adicionar campo veiculo à tabela expedicao_carregamentos
ALTER TABLE expedicao_carregamentos
ADD COLUMN IF NOT EXISTS veiculo text;
