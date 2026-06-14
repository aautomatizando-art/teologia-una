-- ============================================================
-- DASHBOARD PRODUÇÃO & ESTOQUE — Schema Supabase (PostgreSQL)
-- Execute este arquivo inteiro no SQL Editor do Supabase.
-- ============================================================

-- ---------- USUÁRIOS DO APP ----------
create table if not exists app_users (
  id bigint generated always as identity primary key,
  username text unique not null,
  password_hash text not null,          -- sha256 hex
  role text not null check (role in ('producao','estoque','admin')),
  criado_em timestamptz default now()
);

-- Usuários iniciais (troque as senhas depois!)
-- producao / producao123 | estoque / estoque123 | admin / admin123
insert into app_users (username, password_hash, role) values
  ('producao', '247c5a87c4292ac36590492c216e8f565e56b85584892b89fb45dd3a9b6fd2ab', 'producao'),
  ('estoque',  'b567ce29622c16851ee4e288b0e74f62a9ec1094e2b60bbb088f30320ada9eae', 'estoque'),
  ('admin',    '240be518fabd2724ddb6f04eeb1da5967448d7e831c08c8fa822809f74c720a9', 'admin')
on conflict (username) do nothing;

-- ---------- PRODUTOS ----------
create table if not exists produtos (
  id bigint generated always as identity primary key,
  nome text unique not null,
  categoria text not null,
  quantidade integer not null default 0,
  qtd_minima integer not null default 50,
  qtd_maxima integer not null default 500,
  criado_em timestamptz default now()
);

insert into produtos (nome, categoria, quantidade, qtd_minima, qtd_maxima) values
  ('BATATA FAT 24X30G NATURAL(30g)',                'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT 24X30G CEB/SAL (30g)',               'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT 24X30G CHURRASCO (30g)',             'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT. 24X40G CEB/SAL  - FRITISCO',        'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT. 24X40G CHURRASCO -FRITISCO',        'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT. 24X40G NAT -FRITISCO',              'Batata Fatiada', 120, 50, 500),
  ('BATATA FATIADA 10X80G LISA NATURAL',            'Batata Fatiada', 120, 50, 500),
  ('BATATA FATIADA 10X80G LISA CEBOLA',             'Batata Fatiada', 120, 50, 500),
  ('BATATA FATIADA 10X80G LISA FRANGO GRELHADO',    'Batata Fatiada', 120, 50, 500),
  ('BATATA FATIADA 10X80G LISA PARMESÃO',           'Batata Fatiada', 120, 50, 500),
  ('BATATA FATIADA 24X40G LISA NATURAL',            'Batata Fatiada', 120, 50, 500),
  ('BATATA FATIADA 24X40G LISA CEBOLA',             'Batata Fatiada', 120, 50, 500),
  ('BATATA FATIADA 24X40G LISA CHURRASCO',          'Batata Fatiada', 120, 50, 500),
  ('BATATA PALHA BISCOITONE 30X100G',               'Batata Palha',   120, 50, 500),
  ('BATATA PALHA 24x100g',                          'Batata Palha',   120, 50, 500),
  ('BATATA PALHA FRIT 24X100G FINISSIMA',           'Batata Palha',   120, 50, 500),
  ('BATATA PALHA FRITISCO 24X150',                  'Batata Palha',   120, 50, 500),
  ('BATATA PALHA FRITISCO 40X150G',                 'Batata Palha',   120, 50, 500),
  ('BATATA PALHA 24X150G (BISCOITONE)',             'Batata Palha',   120, 50, 500),
  ('BATATA PALHA FRITISCO 20X400G',                 'Batata Palha',   120, 50, 500),
  ('BATATA PALHA FRITISCO 20X500G',                 'Batata Palha',   120, 50, 500),
  ('BATATA PALHA FRITISCO 16X500G',                 'Batata Palha',   120, 50, 500),
  ('SALG.MILHO SAB.BACON 10X10X45',                 'Salgadinho de Milho', 120, 50, 500),
  ('SALG.MILHO.SAB.CEB 10X10X45',                   'Salgadinho de Milho', 120, 50, 500),
  ('SALG.MILHO.SAB.CHU 10X10X45G',                  'Salgadinho de Milho', 120, 50, 500),
  ('SALG.MILHO.SAB.PRESUNTO 10X10X45',              'Salgadinho de Milho', 120, 50, 500),
  ('SAG.MILHO.SAB REQUEIJAO 10X10X45',              'Salgadinho de Milho', 120, 50, 500),
  ('SALG.MILHO.SAB.QUEI 10X10X45',                  'Salgadinho de Milho', 120, 50, 500),
  ('PELLET BACON 10X90G',                           'Pellet', 120, 50, 500),
  ('PELLET BACON 08X10X90G',                        'Pellet', 120, 50, 500),
  ('PELLET BACON 10x50G',                           'Pellet', 120, 50, 500),
  ('PELLET BACON 10x10x50G',                        'Pellet', 120, 50, 500),
  ('BATATA FAT BISCOITONE NAT 24X40G',              'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT BISCOITONE CHUR 24X40G',             'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT BISCOITONE CEB 24X40G',              'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT 24X40G NATURAL (REAL)',              'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT 24X40G CHURRASCO (REAL)',            'Batata Fatiada', 120, 50, 500),
  ('BATATA FAT 24X40G CEBOLA (REAL)',               'Batata Fatiada', 120, 50, 500)
on conflict (nome) do nothing;

-- ---------- ORDENS DE PRODUÇÃO ----------
create table if not exists ordens_producao (
  id bigint generated always as identity primary key,
  numero text unique not null,            -- ex: OP-1001
  produto_id bigint references produtos(id),
  meta_paletes integer not null default 100,
  status text not null default 'ABERTA' check (status in ('ABERTA','CONCLUIDA','CANCELADA')),
  criado_em timestamptz default now()
);

-- Pedidos vinculados a uma OP (uma OP pode ter vários pedidos)
create table if not exists pedidos_op (
  id bigint generated always as identity primary key,
  ordem_id bigint not null references ordens_producao(id) on delete cascade,
  codigo_pedido text not null,
  qtd_planejada integer not null default 0
);

-- Registros de produção (paletes ao longo do tempo)
create table if not exists producao_registros (
  id bigint generated always as identity primary key,
  ordem_id bigint not null references ordens_producao(id) on delete cascade,
  paletes integer not null,
  registrado_em timestamptz default now()
);

-- Perdas de embalagem
create table if not exists perdas_embalagem (
  id bigint generated always as identity primary key,
  ordem_id bigint not null references ordens_producao(id) on delete cascade,
  tipo text not null,                      -- ex: Filme rasgado, Solda aberta...
  quantidade integer not null,
  registrado_em timestamptz default now()
);

-- Problemas qualitativos
create table if not exists problemas_qualidade (
  id bigint generated always as identity primary key,
  ordem_id bigint not null references ordens_producao(id) on delete cascade,
  tipo text not null,                      -- ex: Peso fora do padrão, Cor...
  quantidade integer not null,
  registrado_em timestamptz default now()
);

-- ---------- RESERVAS DE ESTOQUE ----------
create table if not exists reservas (
  id bigint generated always as identity primary key,
  produto_id bigint not null references produtos(id),
  quantidade integer not null,
  pedido text,
  ordem_producao text,
  solicitante text not null,
  data date not null,
  hora time not null,
  criado_em timestamptz default now()
);

-- ---------- PEDIDOS DE COMPRA ----------
create table if not exists pedidos_compra (
  id bigint generated always as identity primary key,
  produto_id bigint not null references produtos(id),
  quantidade integer not null default 0,
  data date not null,
  hora time not null,
  solicitante text not null,
  criticidade text not null check (criticidade in ('EMERGENCIAL','URGENTE','MODERADO')),
  -- Rastreio do pedido (página 6)
  status_rastreio integer not null default 1 check (status_rastreio between 1 and 5),
  -- 1 GERADO ORDEM DE PRODUÇÃO | 2 PRODUZINDO | 3 ENTRADO NO ESTOQUE
  -- 4 NA EXPEDIÇÃO             | 5 EM ROTA PARA ENTREGA
  criado_em timestamptz default now()
);

-- ---------- DADOS DEMO (opcional — para os gráficos já nascerem com vida) ----------
insert into ordens_producao (numero, produto_id, meta_paletes, status)
select 'OP-1001', id, 120, 'ABERTA' from produtos where nome = 'BATATA PALHA FRITISCO 24X150'
on conflict (numero) do nothing;

insert into pedidos_op (ordem_id, codigo_pedido, qtd_planejada)
select o.id, p.codigo, p.qtd from ordens_producao o,
  (values ('PED-501', 50), ('PED-502', 40), ('PED-503', 30)) as p(codigo, qtd)
where o.numero = 'OP-1001'
  and not exists (select 1 from pedidos_op x where x.ordem_id = o.id);

insert into producao_registros (ordem_id, paletes, registrado_em)
select o.id, v.paletes, now() - (v.h || ' hours')::interval
from ordens_producao o,
  (values (8, '7'), (12, '6'), (10, '5'), (14, '4'), (11, '3'), (13, '2'), (9, '1')) as v(paletes, h)
where o.numero = 'OP-1001'
  and not exists (select 1 from producao_registros x where x.ordem_id = o.id);

insert into perdas_embalagem (ordem_id, tipo, quantidade)
select o.id, v.tipo, v.qtd from ordens_producao o,
  (values ('Filme rasgado', 14), ('Solda aberta', 8), ('Impressão borrada', 5), ('Fardo violado', 3)) as v(tipo, qtd)
where o.numero = 'OP-1001'
  and not exists (select 1 from perdas_embalagem x where x.ordem_id = o.id);

insert into problemas_qualidade (ordem_id, tipo, quantidade)
select o.id, v.tipo, v.qtd from ordens_producao o,
  (values ('Peso fora do padrão', 6), ('Cor irregular', 4), ('Umidade alta', 3), ('Sabor fora do padrão', 2)) as v(tipo, qtd)
where o.numero = 'OP-1001'
  and not exists (select 1 from problemas_qualidade x where x.ordem_id = o.id);

-- ---------- SEGURANÇA ----------
-- O app acessa o banco apenas pelo backend (service_role), então RLS pode
-- ficar habilitado sem políticas públicas:
alter table app_users enable row level security;
alter table produtos enable row level security;
alter table ordens_producao enable row level security;
alter table pedidos_op enable row level security;
alter table producao_registros enable row level security;
alter table perdas_embalagem enable row level security;
alter table problemas_qualidade enable row level security;
alter table reservas enable row level security;
alter table pedidos_compra enable row level security;
