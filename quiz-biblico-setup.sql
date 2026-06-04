-- ═══════════════════════════════════════════════════════════
--  Quiz Bíblico Pentecostal — Supabase Schema + Seed
--  Execute no SQL Editor: https://app.supabase.com → SQL Editor
-- ═══════════════════════════════════════════════════════════

-- 1. PERGUNTAS ──────────────────────────────────────────────
CREATE TABLE IF NOT EXISTS quiz_perguntas (
  id          UUID PRIMARY KEY DEFAULT gen_random_uuid(),
  nivel       TEXT NOT NULL CHECK (nivel IN ('basico','medio','avancado','teologico')),
  pergunta    TEXT NOT NULL,
  opcoes      JSONB NOT NULL,          -- array com 5 alternativas (strings)
  correta     INT  NOT NULL CHECK (correta BETWEEN 1 AND 5),
  referencia  TEXT DEFAULT '',         -- ex: "Gênesis 6:14"
  versiculo   TEXT DEFAULT '',         -- texto ARC (Almeida Revista e Corrigida)
  explicacao  TEXT DEFAULT '',
  ativa       BOOLEAN DEFAULT TRUE,
  created_at  TIMESTAMPTZ DEFAULT NOW()
);

-- Bancos já existentes: amplia o CHECK para aceitar o nível "teologico"
ALTER TABLE quiz_perguntas DROP CONSTRAINT IF EXISTS quiz_perguntas_nivel_check;
ALTER TABLE quiz_perguntas ADD  CONSTRAINT quiz_perguntas_nivel_check
  CHECK (nivel IN ('basico','medio','avancado','teologico'));

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

-- NÍVEL BÁSICO (ampliação) ──────────────────────────────────
INSERT INTO quiz_perguntas (nivel,pergunta,opcoes,correta,referencia) VALUES
('basico','Quem foi o primeiro homem criado por Deus?',
  '["Caim","Adão","Sete","Enoque","Abel"]',2,'Gênesis 2:7'),
('basico','De qual árvore Adão e Eva não deviam comer?',
  '["Macieira","Figueira","Da ciência do bem e do mal","Videira","Oliveira"]',3,'Gênesis 2:17'),
('basico','Quantos filhos de Jacó deram origem às tribos de Israel?',
  '["7","10","12","14","40"]',3,'Gênesis 35:22-26'),
('basico','Quem foi vendido pelos irmãos como escravo no Egito?',
  '["Benjamim","José","Judá","Rúben","Levi"]',2,'Gênesis 37:28'),
('basico','Quem batizou Jesus no rio Jordão?',
  '["Pedro","André","João Batista","Tiago","Filipe"]',3,'Mateus 3:13'),
('basico','Quantos pães Jesus usou para alimentar os cinco mil?',
  '["2","3","5","7","12"]',3,'João 6:9'),
('basico','Em que dia da semana Jesus ressuscitou?',
  '["Sábado","Sexta-feira","Primeiro dia (domingo)","Quinta-feira","Segunda-feira"]',3,'Marcos 16:9'),
('basico','Qual rei mandou matar os meninos de Belém?',
  '["Pilatos","Herodes","César","Nero","Faraó"]',2,'Mateus 2:16');

-- NÍVEL MÉDIO (ampliação) ───────────────────────────────────
INSERT INTO quiz_perguntas (nivel,pergunta,opcoes,correta,referencia) VALUES
('medio','Quem escreveu a maior parte das epístolas do Novo Testamento?',
  '["Pedro","João","Paulo","Tiago","Lucas"]',3,'Epístolas paulinas'),
('medio','Em qual jardim Jesus orou antes de ser preso?',
  '["Éden","Getsêmani","Gólgota","Siloé","Betesda"]',2,'Mateus 26:36'),
('medio','Quantas pragas Deus enviou sobre o Egito?',
  '["3","7","10","12","40"]',3,'Êxodo 7-12'),
('medio','Qual profeta enfrentou os profetas de Baal no Monte Carmelo?',
  '["Eliseu","Elias","Isaías","Jeremias","Samuel"]',2,'1 Reis 18:19'),
('medio','Quem foi a primeira pessoa a ver Jesus ressuscitado?',
  '["Pedro","Maria Madalena","João","Tomé","Maria, mãe de Jesus"]',2,'João 20:14-16'),
('medio','Qual cidade caiu depois de Israel marchar 7 dias e tocar trombetas?',
  '["Ai","Jericó","Babilônia","Nínive","Gibeão"]',2,'Josué 6:20'),
('medio','Quantos espias Moisés enviou para a Terra Prometida?',
  '["2","7","10","12","40"]',4,'Números 13:1-16'),
('medio','Qual apóstolo era médico e escreveu um Evangelho e o livro de Atos?',
  '["Marcos","Lucas","Mateus","João","Paulo"]',2,'Colossenses 4:14');

-- NÍVEL AVANÇADO (ampliação) ────────────────────────────────
INSERT INTO quiz_perguntas (nivel,pergunta,opcoes,correta,referencia) VALUES
('avancado','Quais eram os nomes babilônicos dos três amigos de Daniel?',
  '["Hananias, Misael e Azarias","Sadraque, Mesaque e Abede-Nego","Gaspar, Melchior e Baltazar","Coré, Datã e Abirão","Pedro, Tiago e João"]',2,'Daniel 1:7'),
('avancado','Qual rei de Israel construiu o primeiro templo em Jerusalém?',
  '["Davi","Salomão","Roboão","Ezequias","Josias"]',2,'1 Reis 6:1'),
('avancado','Quantos anos viveu Matusalém, o homem mais velho da Bíblia?',
  '["777","900","950","969","1000"]',4,'Gênesis 5:27'),
('avancado','Qual profeta casou-se com Gômer por ordem de Deus?',
  '["Amós","Oseias","Joel","Miqueias","Naum"]',2,'Oseias 1:2-3'),
('avancado','Em qual ilha João recebeu a visão do Apocalipse?',
  '["Creta","Chipre","Patmos","Malta","Samos"]',3,'Apocalipse 1:9'),
('avancado','Qual era o nome de Paulo antes de sua conversão?',
  '["Silas","Saulo","Simão","Estêvão","Sóstenes"]',2,'Atos 9:1-4'),
('avancado','Qual sumo sacerdote presidiu o julgamento que condenou Jesus?',
  '["Anás","Caifás","Gamaliel","Nicodemos","Zacarias"]',2,'Mateus 26:57'),
('avancado','Quantos livros compõem o Pentateuco?',
  '["3","4","5","7","12"]',3,'Gênesis a Deuteronômio');

-- NÍVEL TEOLÓGICO ───────────────────────────────────────────
INSERT INTO quiz_perguntas (nivel,pergunta,opcoes,correta,referencia) VALUES
('teologico','Como se chama a doutrina de que Deus é um só em três pessoas: Pai, Filho e Espírito Santo?',
  '["Unicismo","Trindade","Panteísmo","Deísmo","Modalismo"]',2,'Mateus 28:19'),
('teologico','Qual termo teológico descreve o ato de Deus declarar justo o pecador mediante a fé?',
  '["Santificação","Justificação","Glorificação","Reconciliação","Expiação"]',2,'Romanos 5:1'),
('teologico','Na doutrina pentecostal, o batismo no Espírito Santo é evidenciado inicialmente por qual sinal?',
  '["Visões","O falar em outras línguas","Curas","Sonhos","Imposição de mãos"]',2,'Atos 2:4'),
('teologico','Qual ramo da teologia estuda as últimas coisas (o fim dos tempos)?',
  '["Eclesiologia","Cristologia","Escatologia","Soteriologia","Pneumatologia"]',3,''),
('teologico','O estudo teológico sobre a pessoa e a obra de Cristo é chamado de:',
  '["Pneumatologia","Cristologia","Antropologia","Bibliologia","Angelologia"]',2,''),
('teologico','Como é chamado o ramo da teologia que estuda o Espírito Santo?',
  '["Pneumatologia","Soteriologia","Eclesiologia","Hamartiologia","Bibliologia"]',1,''),
('teologico','Qual o nome do processo contínuo de tornar-se santo após a conversão?',
  '["Justificação","Santificação","Predestinação","Adoção","Expiação"]',2,'1 Tessalonicenses 4:3'),
('teologico','Como se chama o ramo da teologia que estuda a doutrina do pecado?',
  '["Soteriologia","Hamartiologia","Escatologia","Eclesiologia","Bibliologia"]',2,''),
('teologico','Qual termo descreve a vinda de Cristo para buscar a Igreja?',
  '["Ascensão","Arrebatamento","Transfiguração","Pentecostes","Milênio"]',2,'1 Tessalonicenses 4:16-17'),
('teologico','O ramo da teologia que trata da doutrina da salvação é a:',
  '["Soteriologia","Cristologia","Eclesiologia","Escatologia","Pneumatologia"]',1,''),
('teologico','Qual termo descreve a união das naturezas divina e humana na pessoa de Cristo?',
  '["Transubstanciação","União hipostática","Kénosis","Teofania","Apostasia"]',2,''),
('teologico','O dom espiritual de falar em outras línguas é tecnicamente chamado de:',
  '["Profecia","Glossolalia","Discernimento","Xenoglossia","Interpretação"]',2,'1 Coríntios 12:10');

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

-- ═══════════════════════════════════════════════════════════
--  VERSÍCULOS — Almeida Revista e Corrigida (ARC), texto fixo
--  (idempotente: rode mesmo em bancos já populados)
-- ═══════════════════════════════════════════════════════════
ALTER TABLE quiz_perguntas ADD COLUMN IF NOT EXISTS versiculo TEXT DEFAULT '';

UPDATE quiz_perguntas SET versiculo='E formou o Senhor Deus o homem do pó da terra e soprou em seus narizes o fôlego da vida; e o homem foi feito alma vivente.' WHERE referencia='Gênesis 2:7';
UPDATE quiz_perguntas SET versiculo='Mas da árvore do conhecimento do bem e do mal, dela não comerás; porque, no dia em que dela comeres, certamente morrerás.' WHERE referencia='Gênesis 2:17';
UPDATE quiz_perguntas SET versiculo='E foram todos os dias de Matusalém novecentos e sessenta e nove anos; e morreu.' WHERE referencia='Gênesis 5:27';
UPDATE quiz_perguntas SET versiculo='Faze para ti uma arca da madeira de gofer; farás compartimentos na arca e a betumarás por dentro e por fora com betume.' WHERE referencia='Gênesis 6:14';
UPDATE quiz_perguntas SET versiculo='E houve chuva sobre a terra quarenta dias e quarenta noites.' WHERE referencia='Gênesis 7:12';
UPDATE quiz_perguntas SET versiculo='E, passando os mercadores midianitas, tiraram e alçaram a José da cova e venderam José por vinte moedas de prata aos ismaelitas, os quais levaram José ao Egito.' WHERE referencia='Gênesis 37:28';
UPDATE quiz_perguntas SET versiculo='Então, Moisés estendeu a sua mão sobre o mar, e o Senhor fez retirar o mar por um forte vento oriental toda aquela noite; e o mar se tornou em seco, e as águas foram partidas.' WHERE referencia='Êxodo 14:21';
UPDATE quiz_perguntas SET versiculo='E vossos filhos pastorearão neste deserto quarenta anos e levarão sobre si as vossas prostituições, até que os vossos cadáveres se consumam neste deserto.' WHERE referencia='Números 14:33';
UPDATE quiz_perguntas SET versiculo='E sucedeu, depois da morte de Moisés, servo do Senhor, que o Senhor falou a Josué, filho de Num, servo de Moisés, dizendo: Moisés, meu servo, é morto; levanta-te, pois, agora, passa este Jordão, tu e todo este povo, à terra que eu dou aos filhos de Israel.' WHERE referencia='Josué 1:1-2';
UPDATE quiz_perguntas SET versiculo='Então, o povo jubilou, tocando os sacerdotes as buzinas; e sucedeu que, ouvindo o povo o sonido da buzina, jubilou o povo com grande júbilo, e o muro caiu abaixo, e o povo subiu à cidade, cada qual em frente de si, e tomaram a cidade.' WHERE referencia='Josué 6:20';
UPDATE quiz_perguntas SET versiculo='Assim, Davi prevaleceu contra o filisteu, com uma funda e com uma pedra, e feriu o filisteu, e o matou; sem que Davi tivesse uma espada na mão.' WHERE referencia='1 Samuel 17:50';
UPDATE quiz_perguntas SET versiculo='E sucedeu que, no ano quatrocentos e oitenta depois de saírem os filhos de Israel do Egito, no ano quarto do reinado de Salomão sobre Israel, no mês de Zive (este é o mês segundo), começou a edificar a casa do Senhor.' WHERE referencia='1 Reis 6:1';
UPDATE quiz_perguntas SET versiculo='Agora, pois, envia e ajunta a mim todo o Israel no monte Carmelo; como também os quatrocentos e cinquenta profetas de Baal e os quatrocentos profetas do bosque que comem da mesa de Jezabel.' WHERE referencia='1 Reis 18:19';
UPDATE quiz_perguntas SET versiculo='E sucedeu que, indo eles andando e falando, eis que um carro de fogo, com cavalos de fogo, os separou um do outro; e Elias subiu ao céu num redemoinho.' WHERE referencia='2 Reis 2:11';
UPDATE quiz_perguntas SET versiculo='E o chefe dos eunucos lhes pôs outros nomes, a saber: a Daniel pôs o de Beltessazar; a Hananias, o de Sadraque; a Misael, o de Mesaque; e a Azarias, o de Abede-Nego.' WHERE referencia='Daniel 1:7';
UPDATE quiz_perguntas SET versiculo='Esta, pois, é a escritura que se escreveu: MENE, MENE, TEQUEL e PARSIM.' WHERE referencia='Daniel 5:25';
UPDATE quiz_perguntas SET versiculo='O meu Deus enviou o seu anjo e fechou a boca dos leões, para que não me fizessem dano, porque foi achada em mim inocência diante dele; e também contra ti, ó rei, não tenho cometido delito algum.' WHERE referencia='Daniel 6:22';
UPDATE quiz_perguntas SET versiculo='O princípio da palavra do Senhor por Oseias. Disse, pois, o Senhor a Oseias: Vai, toma uma mulher de prostituições e filhos de prostituição; porque a terra se prostituiu gravemente, desviando-se do Senhor.' WHERE referencia='Oseias 1:2-3';
UPDATE quiz_perguntas SET versiculo='Preparou, porém, o Senhor um grande peixe, para que tragasse a Jonas; e esteve Jonas três dias e três noites nas entranhas do peixe.' WHERE referencia='Jonas 1:17';
UPDATE quiz_perguntas SET versiculo='E, tendo nascido Jesus em Belém de Judeia, no tempo do rei Herodes, eis que uns magos vieram do oriente a Jerusalém.' WHERE referencia='Mateus 2:1';
UPDATE quiz_perguntas SET versiculo='Então, Herodes, vendo que tinha sido iludido pelos magos, irou-se muito e mandou matar todos os meninos que havia em Belém e em todos os seus contornos, de dois anos para baixo, segundo o tempo que diligentemente inquirira dos magos.' WHERE referencia='Mateus 2:16';
UPDATE quiz_perguntas SET versiculo='Então, veio Jesus da Galileia ter com João, junto do Jordão, para ser batizado por ele.' WHERE referencia='Mateus 3:13';
UPDATE quiz_perguntas SET versiculo='E Jesus, passando adiante dali, viu assentado na alfândega um homem chamado Mateus e disse-lhe: Segue-me. E ele, levantando-se, o seguiu.' WHERE referencia='Mateus 9:9';
UPDATE quiz_perguntas SET versiculo='Então, chegou Jesus com eles a um lugar chamado Getsêmani e disse a seus discípulos: Assentai-vos aqui, enquanto vou ali orar.' WHERE referencia='Mateus 26:36';
UPDATE quiz_perguntas SET versiculo='E, os que prenderam a Jesus, o conduziram à casa de Caifás, o sumo sacerdote, onde os escribas e os anciãos estavam reunidos.' WHERE referencia='Mateus 26:57';
UPDATE quiz_perguntas SET versiculo='E Jesus, tendo ressuscitado na manhã do primeiro dia da semana, apareceu primeiramente a Maria Madalena, da qual tinha expulsado sete demônios.' WHERE referencia='Marcos 16:9';
UPDATE quiz_perguntas SET versiculo='E, virando-se o Senhor, olhou para Pedro, e Pedro lembrou-se da palavra do Senhor, como lhe havia dito: Antes que o galo cante, três vezes me negarás.' WHERE referencia='Lucas 22:61';
UPDATE quiz_perguntas SET versiculo='Jesus principiou assim os seus sinais em Caná da Galileia e manifestou a sua glória; e os seus discípulos creram nele. (v. 11)' WHERE referencia='João 2:1-11';
UPDATE quiz_perguntas SET versiculo='Está aqui um rapaz que tem cinco pães de cevada e dois peixinhos; mas que é isto para tantos?' WHERE referencia='João 6:9';
UPDATE quiz_perguntas SET versiculo='E, tendo dito isto, voltou-se para trás e viu Jesus em pé, mas não sabia que era Jesus. Jesus disse-lhe: Maria! Ela, voltando-se, disse-lhe: Rabôni (que quer dizer Mestre).' WHERE referencia='João 20:14-16';
UPDATE quiz_perguntas SET versiculo='E lançaram-lhes sortes, e caiu a sorte sobre Matias; e por voto comum foi contado com os onze apóstolos.' WHERE referencia='Atos 1:26';
UPDATE quiz_perguntas SET versiculo='E foram vistas por eles línguas repartidas, como que de fogo, as quais pousaram sobre cada um deles. E todos foram cheios do Espírito Santo e começaram a falar em outras línguas, conforme o Espírito Santo lhes concedia que falassem.' WHERE referencia='Atos 2:3-4';
UPDATE quiz_perguntas SET versiculo='E Saulo, respirando ainda ameaças e mortes contra os discípulos do Senhor, foi para Damasco. E, indo no caminho, subitamente o cercou um resplendor de luz do céu. E, caindo por terra, ouviu uma voz que lhe dizia: Saulo, Saulo, por que me persegues?' WHERE referencia='Atos 9:1-4';
UPDATE quiz_perguntas SET versiculo='E sucedeu que todo um ano se reuniram naquela igreja e ensinaram muita gente; e em Antioquia foram os discípulos, pela primeira vez, chamados cristãos.' WHERE referencia='Atos 11:26';
UPDATE quiz_perguntas SET versiculo='Mas o fruto do Espírito é: amor, gozo, paz, longanimidade, benignidade, bondade, fé, mansidão, temperança. Contra estas coisas não há lei.' WHERE referencia='Gálatas 5:22-23';
UPDATE quiz_perguntas SET versiculo='Saúda-vos Lucas, o médico amado, e Demas.' WHERE referencia='Colossenses 4:14';
UPDATE quiz_perguntas SET versiculo='Porque a um, pelo Espírito, é dada a palavra da sabedoria; e a outro, pelo mesmo Espírito, a palavra da ciência; e a outro, a fé; e a outro, os dons de curar; e a outro, a operação de maravilhas; e a outro, a profecia; e a outro, o dom de discernir os espíritos; e a outro, a variedade de línguas; e a outro, a interpretação das línguas.' WHERE referencia='1 Coríntios 12:8-10';
UPDATE quiz_perguntas SET versiculo='Os quais noutro tempo foram rebeldes, quando a longanimidade de Deus esperava nos dias de Noé, enquanto se preparava a arca; na qual poucas (isto é, oito) almas se salvaram pela água.' WHERE referencia='1 Pedro 3:20';
UPDATE quiz_perguntas SET versiculo='Revelação de Jesus Cristo, a qual Deus lhe deu, para mostrar aos seus servos as coisas que brevemente devem acontecer; e pelo seu anjo as enviou e as notificou a João, seu servo.' WHERE referencia='Apocalipse 1:1';
UPDATE quiz_perguntas SET versiculo='Eu, João, que também sou vosso irmão e companheiro na aflição, e no reino, e paciência de Jesus Cristo, estava na ilha chamada Patmos, por causa da palavra de Deus e pelo testemunho de Jesus Cristo.' WHERE referencia='Apocalipse 1:9';
UPDATE quiz_perguntas SET versiculo='Portanto, ide, ensinai todas as nações, batizando-as em nome do Pai, e do Filho, e do Espírito Santo;' WHERE referencia='Mateus 28:19';
UPDATE quiz_perguntas SET versiculo='Sendo, pois, justificados pela fé, temos paz com Deus, por nosso Senhor Jesus Cristo;' WHERE referencia='Romanos 5:1';
UPDATE quiz_perguntas SET versiculo='E todos foram cheios do Espírito Santo e começaram a falar em outras línguas, conforme o Espírito Santo lhes concedia que falassem.' WHERE referencia='Atos 2:4';
UPDATE quiz_perguntas SET versiculo='Porque esta é a vontade de Deus, a vossa santificação: que vos abstenhais da prostituição;' WHERE referencia='1 Tessalonicenses 4:3';
UPDATE quiz_perguntas SET versiculo='Porque o mesmo Senhor descerá do céu com alarido, e com voz de arcanjo, e com a trombeta de Deus; e os que morreram em Cristo ressuscitarão primeiro. Depois, nós, os que ficarmos vivos, seremos arrebatados juntamente com eles nas nuvens, a encontrar o Senhor nos ares, e assim estaremos sempre com o Senhor.' WHERE referencia='1 Tessalonicenses 4:16-17';
UPDATE quiz_perguntas SET versiculo='E a outro, a operação de maravilhas; e a outro, a profecia; e a outro, o dom de discernir os espíritos; e a outro, a variedade de línguas; e a outro, a interpretação das línguas.' WHERE referencia='1 Coríntios 12:10';
