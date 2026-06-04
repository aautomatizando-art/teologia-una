-- ═══════════════════════════════════════════════════════════
--  Quiz Bíblico Pentecostal — Supabase Schema + Seed
--  Execute no SQL Editor: https://app.supabase.com → SQL Editor
-- ═══════════════════════════════════════════════════════════

-- 1. PERGUNTAS ──────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS quiz_perguntas (
  id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  nivel       TEXT NOT NULL CHECK (nivel IN ('basico','medio','avancado')),
  pergunta    TEXT NOT NULL,
  opcoes      JSONB NOT NULL,          -- array com 5 alternativas (strings)
  correta     INT  NOT NULL CHECK (correta BETWEEN 1 AND 5),
  referencia  TEXT DEFAULT '',         -- ex: "Gênesis 6:14"
  explicacao  TEXT DEFAULT '',
  ativa       BOOLEAN DEFAULT TRUE,
  created_at  TIMESTAMPTZ DEFAULT NOW()
);

-- 2. CASTIGOS BÍBLICOS (penalidades edificantes ao errar) ────
CREATE TABLE IF NOT EXISTS quiz_castigos (
  id     UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  texto  TEXT NOT NULL,
  ativo  BOOLEAN DEFAULT TRUE
);

-- 3. JOGADORES (cadastro reutilizável) ──────────────────────
CREATE TABLE IF NOT EXISTS quiz_jogadores (
  id     INT PRIMARY KEY,            -- 1..8  (corresponde à saída do ESP32)
  nome   TEXT NOT NULL,
  cor    TEXT DEFAULT '#35c8ff',
  ativo  BOOLEAN DEFAULT TRUE
);

-- 4. PARTIDAS ───────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS quiz_partidas (
  id           UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  nivel        TEXT NOT NULL,
  qtd_jogadores INT DEFAULT 0,
  data_inicio  TIMESTAMPTZ DEFAULT NOW(),
  data_fim     TIMESTAMPTZ,
  status       TEXT DEFAULT 'em_andamento',  -- em_andamento | finalizada
  placar       JSONB DEFAULT '[]'::jsonb
);

-- 5. RESPOSTAS (log de cada rodada) ─────────────────────────
CREATE TABLE IF NOT EXISTS quiz_respostas (
  id           UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  partida_id   UUID REFERENCES quiz_partidas(id) ON DELETE CASCADE,
  pergunta_id  UUID REFERENCES quiz_perguntas(id) ON DELETE SET NULL,
  jogador_id   INT,
  jogador_nome TEXT,
  nivel        TEXT,
  resposta     INT,           -- 1..5  (0 = tempo esgotado)
  correto      BOOLEAN,
  tempo_s      NUMERIC,
  pontos       INT DEFAULT 0,
  created_at   TIMESTAMPTZ DEFAULT NOW()
);

-- ─── ROW LEVEL SECURITY ─────────────────────────────────────
ALTER TABLE quiz_perguntas ENABLE ROW LEVEL SECURITY;
ALTER TABLE quiz_castigos  ENABLE ROW LEVEL SECURITY;
ALTER TABLE quiz_jogadores ENABLE ROW LEVEL SECURITY;
ALTER TABLE quiz_partidas  ENABLE ROW LEVEL SECURITY;
ALTER TABLE quiz_respostas ENABLE ROW LEVEL SECURITY;

CREATE POLICY "anon_perguntas" ON quiz_perguntas FOR ALL USING (true) WITH CHECK (true);
CREATE POLICY "anon_castigos"  ON quiz_castigos  FOR ALL USING (true) WITH CHECK (true);
CREATE POLICY "anon_jogadores" ON quiz_jogadores FOR ALL USING (true) WITH CHECK (true);
CREATE POLICY "anon_partidas"  ON quiz_partidas  FOR ALL USING (true) WITH CHECK (true);
CREATE POLICY "anon_respostas" ON quiz_respostas FOR ALL USING (true) WITH CHECK (true);

-- ─── ÍNDICES ────────────────────────────────────────────────
CREATE INDEX IF NOT EXISTS idx_perguntas_nivel  ON quiz_perguntas(nivel) WHERE ativa;
CREATE INDEX IF NOT EXISTS idx_respostas_partida ON quiz_respostas(partida_id);

-- ═══════════════════════════════════════════════════════════
--  SEED — PERGUNTAS
-- ═══════════════════════════════════════════════════════════

-- NÍVEL BÁSICO ──────────────────────────────────────────────
INSERT INTO quiz_perguntas (nivel,pergunta,opcoes,correta,referencia) VALUES
('basico','Quem construiu a arca por ordem de Deus?',
  '["Moisés","Noé","Abraão","Davi","Josué"]',2,'Gênesis 6:14'),
('basico','Por quantos dias e noites choveu durante o dilúvio?',
  '["7","12","40","100","365"]',3,'Gênesis 7:12'),
('basico','Qual profeta foi engolido por um grande peixe?',
  '["Elias","Eliseu","Jonas","Daniel","Isaías"]',3,'Jonas 1:17'),
('basico','Qual é o primeiro livro da Bíblia?',
  '["Êxodo","Salmos","Gênesis","João","Mateus"]',3,'Gênesis 1:1'),
('basico','Em qual cidade Jesus nasceu?',
  '["Nazaré","Jerusalém","Belém","Cafarnaum","Betânia"]',3,'Mateus 2:1'),
('basico','Quantos discípulos Jesus escolheu?',
  '["7","10","12","24","70"]',3,'Mateus 10:1'),
('basico','Quem matou o gigante Golias com uma funda?',
  '["Saul","Davi","Sansão","Jônatas","Gideão"]',2,'1 Samuel 17:50'),
('basico','Qual mar foi dividido para Israel atravessar a seco?',
  '["Mar da Galileia","Mar Morto","Mar Mediterrâneo","Mar Vermelho","Mar de Tiberíades"]',4,'Êxodo 14:21'),
('basico','Quem foi lançado na cova dos leões e saiu ileso?',
  '["Daniel","Sadraque","José","Jeremias","Mardoqueu"]',1,'Daniel 6:22'),
('basico','Em quantos dias Deus criou o mundo, descansando no sétimo?',
  '["3","5","6","7","12"]',3,'Gênesis 2:2'),
('basico','Qual é o último livro da Bíblia?',
  '["Judas","Malaquias","Apocalipse","Atos","Hebreus"]',3,'Apocalipse 1:1'),
('basico','Quem traiu Jesus por trinta moedas de prata?',
  '["Pedro","Tomé","Judas Iscariotes","Tiago","Filipe"]',3,'Mateus 26:15');

-- NÍVEL MÉDIO ───────────────────────────────────────────────
INSERT INTO quiz_perguntas (nivel,pergunta,opcoes,correta,referencia) VALUES
('medio','Quantos livros tem a Bíblia (Antigo + Novo Testamento)?',
  '["27","39","50","66","73"]',4,'Cânon bíblico'),
('medio','Quem sucedeu Moisés na liderança de Israel?',
  '["Arão","Calebe","Josué","Gideão","Samuel"]',3,'Josué 1:1-2'),
('medio','No dia de Pentecostes, o que desceu sobre os discípulos?',
  '["Uma nuvem","Línguas de fogo (Espírito Santo)","Um anjo","Maná","Uma estrela"]',2,'Atos 2:3-4'),
('medio','Qual foi o primeiro milagre de Jesus?',
  '["Curar um cego","Multiplicar pães","Transformar água em vinho","Andar sobre as águas","Ressuscitar Lázaro"]',3,'João 2:1-11'),
('medio','Em que monte Moisés recebeu os Dez Mandamentos?',
  '["Monte Carmelo","Monte Sinai","Monte das Oliveiras","Monte Hermon","Monte Sião"]',2,'Êxodo 19:20'),
('medio','Quantos frutos do Espírito Paulo cita em Gálatas?',
  '["3","7","9","10","12"]',3,'Gálatas 5:22-23'),
('medio','Qual apóstolo negou Jesus três vezes?',
  '["João","Pedro","André","Tomé","Bartolomeu"]',2,'Lucas 22:61'),
('medio','Quem foi a mãe de João Batista?',
  '["Maria","Ana","Isabel","Sara","Raquel"]',3,'Lucas 1:13'),
('medio','Por quantos anos Israel peregrinou no deserto?',
  '["7","12","40","70","100"]',3,'Números 14:33'),
('medio','Qual era a profissão de Mateus antes de seguir Jesus?',
  '["Pescador","Carpinteiro","Cobrador de impostos","Médico","Pastor"]',3,'Mateus 9:9'),
('medio','Quem teve o sonho de uma escada que ligava a terra ao céu?',
  '["José","Jacó","Daniel","Salomão","Faraó"]',2,'Gênesis 28:12'),
('medio','Qual rei escreveu a maioria dos Salmos?',
  '["Salomão","Saul","Davi","Ezequias","Josias"]',3,'Salmos');

-- NÍVEL AVANÇADO ────────────────────────────────────────────
INSERT INTO quiz_perguntas (nivel,pergunta,opcoes,correta,referencia) VALUES
('avancado','Qual profeta foi levado ao céu numa carruagem de fogo?',
  '["Eliseu","Enoque","Elias","Isaías","Ezequiel"]',3,'2 Reis 2:11'),
('avancado','Em que cidade os seguidores de Jesus foram chamados "cristãos" pela primeira vez?',
  '["Jerusalém","Roma","Antioquia","Éfeso","Corinto"]',3,'Atos 11:26'),
('avancado','Qual livro do Antigo Testamento não menciona explicitamente o nome de Deus?',
  '["Rute","Ester","Eclesiastes","Cantares","Provérbios"]',2,'Livro de Ester'),
('avancado','Quantos capítulos tem o livro de Salmos?',
  '["100","119","120","150","176"]',4,'Salmos'),
('avancado','Qual era o nome dado a Daniel na Babilônia?',
  '["Beltessazar","Sadraque","Mesaque","Abede-Nego","Nabucodonosor"]',1,'Daniel 1:7'),
('avancado','Quem substituiu Judas Iscariotes entre os doze apóstolos?',
  '["Paulo","Barnabé","Estêvão","Matias","Silas"]',4,'Atos 1:26'),
('avancado','Qual rei viu a escrita misteriosa na parede ("Mene, Mene, Tequel e Parsim")?',
  '["Nabucodonosor","Belsazar","Dario","Ciro","Senaqueribe"]',2,'Daniel 5:25'),
('avancado','Quem foi o sogro de Moisés, sacerdote de Midiã?',
  '["Labão","Jetro","Aarão","Hur","Coré"]',2,'Êxodo 18:1'),
('avancado','Quantos dons espirituais Paulo lista em 1 Coríntios 12?',
  '["5","7","9","10","12"]',3,'1 Coríntios 12:8-10'),
('avancado','Em qual livro do AT está originalmente a frase "o justo viverá pela fé"?',
  '["Isaías","Jeremias","Habacuque","Ezequiel","Amós"]',3,'Habacuque 2:4'),
('avancado','Qual apóstolo é tradicionalmente chamado de "o discípulo amado"?',
  '["Pedro","João","Tiago","André","Tomé"]',2,'João 13:23'),
('avancado','Quantas pessoas foram salvas na arca de Noé?',
  '["2","4","6","8","12"]',4,'1 Pedro 3:20');

-- SEED — CASTIGOS BÍBLICOS ──────────────────────────────────
INSERT INTO quiz_castigos (texto) VALUES
('Recite o Salmo 23 de cor para toda a turma.'),
('Cante um louvor à escolha da plateia.'),
('Cite, em ordem, 5 livros do Novo Testamento.'),
('Diga o nome de pelo menos 6 dos 12 apóstolos.'),
('Cite os 9 frutos do Espírito (Gálatas 5:22-23).'),
('Ore em voz alta agradecendo por 3 bênçãos.'),
('Recite pelo menos 5 dos Dez Mandamentos.'),
('Cite os 5 livros do Pentateuco.'),
('Faça uma mímica de uma história bíblica para os outros adivinharem.'),
('Decore e recite João 3:16.'),
('Diga 3 nomes ou títulos de Deus encontrados na Bíblia.'),
('Cite 3 milagres realizados por Jesus.');

-- SEED — JOGADORES PADRÃO ───────────────────────────────────
INSERT INTO quiz_jogadores (id,nome,cor) VALUES
 (1,'Jogador 1','#35c8ff'),(2,'Jogador 2','#4ade80'),
 (3,'Jogador 3','#ffd166'),(4,'Jogador 4','#ff6b35'),
 (5,'Jogador 5','#a78bfa'),(6,'Jogador 6','#f472b6'),
 (7,'Jogador 7','#22d3ee'),(8,'Jogador 8','#fb7185')
ON CONFLICT (id) DO NOTHING;

-- ─── VIEW: RANKING GERAL ────────────────────────────────────
CREATE OR REPLACE VIEW quiz_ranking AS
SELECT
  jogador_nome,
  COUNT(*)                                  AS rodadas,
  SUM(CASE WHEN correto THEN 1 ELSE 0 END)  AS acertos,
  SUM(pontos)                               AS pontos,
  ROUND(AVG(tempo_s),1)                     AS tempo_medio_s
FROM quiz_respostas
GROUP BY jogador_nome
ORDER BY pontos DESC;
