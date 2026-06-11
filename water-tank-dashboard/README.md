# Dashboard Gestão Condomínio — Vercel + Supabase

Dashboard moderna (single-page, sem build) para acompanhar em tempo real
todos os módulos de monitoramento do condomínio:

- **Tanque Superior** — nível animado + histórico do nível
- **Tanque Inferior** — nível animado + Entradas 1-4 (Bomba ligou / Bomba
  falhou / Falha no inversor / Painel sem energia) + histórico do nível,
  temperatura e vibração da bomba
- **Alarme de Incêndio** — status (Sistema Normal / Alarme / Avaria), ícone de
  sinaleiro (verde) ou sirene (vermelho) e foto da central enviada pelo
  ESP32-CAM
- **Bombas de Incêndio** — Bomba Principal ligada / Falha na bomba / Bomba
  Reserva ligada
- **Cancela da Portaria** — Cancela 1 em falha / Cancela 2 em falha
- **Água da Rua** — vazão (L/min) e status do fluxo (ativo / parado)

```
[Tanque Superior] -ESP-NOW-> [ESP32 Gateway] --\
[Tanque Inferior] ------------- WiFi direto ----\
[Alarme Incêndio (ESP32-CAM)] -- WiFi direto -----\   HTTPS (REST + Storage)
[Bombas Incêndio] -------------- WiFi direto ------>  Supabase  ---->  Dashboard (Vercel)
[Cancela Portaria] -------------- WiFi direto ----/   (tabelas + bucket      realtime / polling
[Água da Rua] -------------------- WiFi direto --/     "fotos-incendio")
                                                  (todos também enviam
                                                   alertas para o grupo
                                                   do WhatsApp via Evolution API)
```

## 1. Criar o projeto no Supabase

1. Acesse [app.supabase.com](https://app.supabase.com) e crie um novo projeto (gratuito).
2. Vá em **SQL Editor** e execute o script [`supabase-setup.sql`](./supabase-setup.sql).
   Ele cria todas as tabelas usadas pela dashboard, habilita RLS (acesso via
   chave `anon`) e ativa o Realtime:
   - `leituras` — Tanque Superior (view `ultima_leitura`)
   - `tanque_inferior` — Tanque Inferior (view `ultima_tanque_inferior`)
   - `alarme_incendio` — Alarme de Incêndio (view `ultima_alarme_incendio`)
   - `bombas_incendio` — Bombas de Incêndio (view `ultima_bombas_incendio`)
   - `cancela_portaria` — Cancela da Portaria (view `ultima_cancela_portaria`)
   - `agua_rua` — Água da Rua (view `ultima_agua_rua`)
   - View `condominios` — lista os condomínios que já enviaram leitura em
     qualquer uma das tabelas acima (alimenta o seletor do topo)
   - Bucket de Storage `fotos-incendio` (público, com políticas de
     select/insert/update/delete) — onde o ESP32-CAM do Alarme de Incêndio
     envia as fotos da central
3. Em **Settings &rarr; API**, copie:
   - **Project URL** (ex: `https://xxxxxxxx.supabase.co`)
   - **anon public key** (chave pública, segura para uso no navegador
     porque as tabelas usam RLS)

> O script é **idempotente**: pode ser executado novamente a qualquer momento
> (ex: após atualizar o repositório) sem perder dados — ele usa
> `CREATE TABLE IF NOT EXISTS`, `CREATE OR REPLACE VIEW` e recria as policies.

## 2. Configurar os ESP32

Cada um dos 7 módulos ESP32 (veja
[`water-tank-espnow/README.md`](../water-tank-espnow/README.md)) tem seu
próprio `config.h` com:

```cpp
#define SUPABASE_URL  "https://xxxxxxxx.supabase.co"
#define SUPABASE_KEY  "eyJ...sua_anon_key..."
```

Use a **mesma** `SUPABASE_URL`/`SUPABASE_KEY` (a *anon key* do passo 1) em
todos os módulos do mesmo Supabase. A cada leitura/mudança de estado, cada
módulo envia um `INSERT` para sua tabela (além dos alertas de WhatsApp já
configurados).

> A chave **anon** é pública por design (protegida pela política RLS que só
> permite leitura/inserção, sem dados sensíveis). **Nunca** use a chave
> `service_role` no ESP32 ou no dashboard.

## 3. Configurar o dashboard

Edite `index.html` e ajuste, antes do deploy:

```js
const DEFAULT_SB_URL = "https://xxxxxxxx.supabase.co";
const DEFAULT_SB_KEY = "eyJ...sua_anon_key...";
```

Alternativamente, **sem editar o arquivo**, abra o dashboard publicado e
clique no ícone &#9881; no canto superior direito para informar a URL e a
chave — ficam salvas no `localStorage` do navegador.

## 4. Deploy no Vercel

1. Faça push deste diretório (`water-tank-dashboard/`) para o GitHub.
2. No [Vercel](https://vercel.com), **New Project &rarr; Import** o
   repositório, definindo `water-tank-dashboard` como **Root Directory**.
3. Não é necessário build command nem framework — é um site estático
   (`vercel.json` já configurado).
4. Deploy. A URL gerada (`https://seu-projeto.vercel.app`) é o link da
   dashboard.

## Vários condomínios

A mesma dashboard atende quantos condomínios você quiser:

1. Cada condomínio tem seu próprio conjunto de ESP32 (no mínimo o par do
   Tanque Superior — os demais módulos são opcionais)
2. No `config.h` de **cada** módulo de um condomínio, defina o **mesmo**
   `CONDOMINIO_NOME` (ex: `"Residencial Sol"`, `"Condominio Park"`)
3. Todos os módulos apontam para o **mesmo** Supabase (mesma URL e anon key)
4. O seletor no topo da dashboard lista automaticamente os condomínios que já
   enviaram leituras em qualquer uma das 6 tabelas — a escolha fica salva no
   navegador

O SQL de setup já cria a view `condominios` (união das 6 tabelas) usada pelo
seletor. Se as tabelas foram criadas numa versão antiga, basta rodar o
`supabase-setup.sql` de novo: ele adiciona colunas/tabelas/views novas sem
perder dados.

## Funcionamento

- Ao abrir (ou ao trocar o condomínio no seletor), a dashboard busca em
  paralelo a última leitura das 6 tabelas (`leituras`, `tanque_inferior`,
  `alarme_incendio`, `bombas_incendio`, `cancela_portaria`, `agua_rua`) e o
  histórico recente (para os gráficos).
- Se inscreve no **Realtime** do Supabase com um canal por tabela: cada novo
  `INSERT` atualiza o card correspondente instantaneamente — nível animado,
  cartões de entradas, ícones de status, gráficos (Chart.js) e a foto do
  Alarme de Incêndio.
- Um *polling* de segurança a cada 30s garante atualização mesmo se o
  Realtime cair.
- Os gráficos de histórico (nível do Tanque Superior/Inferior, temperatura e
  vibração da bomba) usam **Chart.js** (via CDN) e mantêm os últimos 30
  pontos recebidos.
- O indicador **SENSOR OFFLINE / Online** no topo segue o status do **Tanque
  Superior**, com o mesmo limite de `TIMEOUT_SENSOR_MS` (2 minutos) usado no
  gateway. Cada card também mostra o horário relativo da própria última
  atualização ("há Xs"/"há Xmin").
- **Alarme de Incêndio**: o card mostra "Sistema Normal" (sinaleiro verde),
  "Alarme acionado" ou "Central com avaria" (sirene vermelha piscando),
  conforme as colunas `entrada1_alarme_acionado` /
  `entrada2_central_avaria` / `status`. A foto mais recente
  (`foto_url`) é exibida com *cache-busting* (timestamp na URL) para sempre
  mostrar a imagem atual do bucket `fotos-incendio`.
