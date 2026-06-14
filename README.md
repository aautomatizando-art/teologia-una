# 🏭 Dashboard de Produção & Estoque

Dashboard moderna (Next.js + Supabase + Vercel) para controle de produtividade, estoque,
pedidos de compra com criticidade, rastreio de pedidos e notificações em grupo de WhatsApp.

## Páginas

| # | Rota | Descrição |
|---|------|-----------|
| 1 | `/` | Login da área de **Produção** |
| 2 | `/producao` | Busca por nº de OP + gráficos: % produção vs OP, paletes × tempo, % por pedido, perdas de embalagem, problemas qualitativos |
| 3 | `/login-estoque` | Login da área de **Estoque** (redireciona para a página 4) |
| 4 | `/estoque` | Total em estoque, cards por produto com status (ESTOQUE BAIXO / ALTO), botão **Reservar** (qtd, pedido, OP, solicitante, data, hora) |
| 5 | `/compras` | Entrada de pedidos de compra + kanban de criticidade: 🟣 EMERGENCIAL, 🔴 URGENTE, 🟢 MODERADO |
| 6 | `/rastreio` | Timeline do pedido: GERADO ORDEM DE PRODUÇÃO → PRODUZINDO → ENTRADO NO ESTOQUE → NA EXPEDIÇÃO → EM ROTA PARA ENTREGA |

## Usuários iniciais (troque as senhas!)

| Usuário | Senha | Área |
|---------|-------|------|
| `producao` | `producao123` | Produção |
| `estoque` | `estoque123` | Estoque/Compras/Rastreio |
| `admin` | `admin123` | Todas |

Para trocar uma senha, gere o hash SHA-256 da nova senha
(ex.: `node -e "console.log(require('crypto').createHash('sha256').update('NOVASENHA').digest('hex'))"`)
e atualize a coluna `password_hash` na tabela `app_users`.

## Configuração (passo a passo)

### 1. Supabase
1. Crie uma conta/projeto em https://supabase.com (gratuito).
2. Abra **SQL Editor** → cole todo o conteúdo de [`supabase/schema.sql`](supabase/schema.sql) → **Run**.
   Isso cria as tabelas, os 38 produtos e dados de demonstração (OP-1001).
3. Em **Settings → API**, copie:
   - `Project URL` → variável `NEXT_PUBLIC_SUPABASE_URL`
   - `service_role` key → variável `SUPABASE_SERVICE_ROLE_KEY`

### 2. Variáveis de ambiente no Vercel
No painel do projeto no Vercel → **Settings → Environment Variables**, adicione:

```
NEXT_PUBLIC_SUPABASE_URL=https://xxxx.supabase.co
SUPABASE_SERVICE_ROLE_KEY=eyJ...
AUTH_SECRET=um-texto-longo-e-aleatorio
```

Depois clique em **Redeploy** (Deployments → ⋯ → Redeploy).

### 3. WhatsApp (opcional)
Escolha **um** provedor e adicione as variáveis correspondentes:

**Opção A — Evolution API** (recomendado para grupos; self-hosted ou cloud):
```
WHATSAPP_PROVIDER=evolution
EVOLUTION_URL=https://sua-evolution-api.com
EVOLUTION_INSTANCE=minha-instancia
EVOLUTION_APIKEY=sua-chave
EVOLUTION_GROUP_JID=120363xxxxxxxxxxxx@g.us
```

**Opção B — CallMeBot** (gratuito e simples, envia para um número):
```
WHATSAPP_PROVIDER=callmebot
CALLMEBOT_PHONE=+5511999999999
CALLMEBOT_APIKEY=123456
```

Teste com: `POST https://seu-app.vercel.app/api/whatsapp-teste`

Notificações enviadas automaticamente:
- 📦 Nova reserva de estoque (com alerta de **ESTOQUE BAIXO** quando aplicável)
- 🟣🔴🟢 Novo pedido de compra (com criticidade)
- 🚚 Mudança de status no rastreio do pedido

## Rodar localmente

```bash
npm install
copy .env.local.example .env.local   # e preencha as variáveis
npm run dev
```
