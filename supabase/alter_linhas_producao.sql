-- Adicionar coluna 'linhas' (linhas de produção que geraram o lançamento, ex: "1,2")
ALTER TABLE producao_registros
ADD COLUMN IF NOT EXISTS linhas text;

ALTER TABLE perdas_embalagem
ADD COLUMN IF NOT EXISTS linhas text;

ALTER TABLE problemas_qualidade
ADD COLUMN IF NOT EXISTS linhas text;
