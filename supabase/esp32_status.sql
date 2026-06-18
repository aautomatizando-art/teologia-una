-- Tabela de status de conexão dos ESP32 por painel
CREATE TABLE IF NOT EXISTS esp32_status (
  painel      int PRIMARY KEY,           -- 1, 2 ou 3
  ultimo_ping timestamptz NOT NULL DEFAULT now(),
  ip          text                       -- IP local do ESP32 (informativo)
);
