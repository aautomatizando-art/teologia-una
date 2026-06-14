-- ============================================================
-- TABELAS PARA MÓDULO DE EXPEDIÇÃO
-- ============================================================

-- Tabela para rastreamento de localização de produtos em estoque
CREATE TABLE IF NOT EXISTS localizacao_estoque (
  id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  pedido_op_id BIGINT NOT NULL REFERENCES pedidos_op(id) ON DELETE CASCADE,
  numero_lote TEXT NOT NULL,
  rua TEXT NOT NULL,
  prateleira TEXT NOT NULL,
  criado_em TIMESTAMPTZ DEFAULT NOW()
);

-- Vincular localização ao pedido_op
ALTER TABLE pedidos_op ADD COLUMN IF NOT EXISTS localizacao_id BIGINT REFERENCES localizacao_estoque(id);

-- Tabela para registrar retiradas (Saída de Estoque)
CREATE TABLE IF NOT EXISTS expedicao_retiradas (
  id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  pedido_op_id BIGINT NOT NULL REFERENCES pedidos_op(id) ON DELETE CASCADE,
  data DATE NOT NULL,
  hora TIME NOT NULL,
  nome TEXT NOT NULL,
  quantidade INTEGER NOT NULL,
  retirado_por TEXT NOT NULL,
  criado_em TIMESTAMPTZ DEFAULT NOW()
);

-- Tabela para registrar carregamentos
CREATE TABLE IF NOT EXISTS expedicao_carregamentos (
  id BIGINT GENERATED ALWAYS AS IDENTITY PRIMARY KEY,
  pedido_op_id BIGINT NOT NULL REFERENCES pedidos_op(id) ON DELETE CASCADE,
  quantidade_carregada INTEGER NOT NULL,
  responsavel_expedicao TEXT NOT NULL,
  nome_motorista TEXT NOT NULL,
  nome_ajudante TEXT NOT NULL,
  criado_em TIMESTAMPTZ DEFAULT NOW()
);
