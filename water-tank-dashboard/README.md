# Dashboard Caixa d'Água — Vercel + Supabase

Dashboard moderna (single-page, sem build) para acompanhar em tempo real:

- **Nível animado da caixa d'água** (recebido do ESP32 via ESP-NOW)
- **Entrada 1** — Bomba ligou
- **Entrada 2** — Bomba falhou
- **Entrada 3** — Falha no inversor
- **Entrada 4** — Painel sem energia (sem rede CA)

```
[Caixa d'agua]      [ESP32 #1 Sensor]   ESP-NOW   [ESP32 #2 Gateway]   HTTPS   [Supabase]   [Dashboard Vercel]
 nível + 4 entradas ------------------> ------> envia leitura -------> tabela ----------> realtime / polling
                                                  + alerta WhatsApp     "leituras"
```

## 1. Criar o projeto no Supabase

1. Acesse [app.supabase.com](https://app.supabase.com) e crie um novo projeto (gratuito).
2. Vá em **SQL Editor** e execute o script [`supabase-setup.sql`](./supabase-setup.sql).
   Ele cria a tabela `leituras`, habilita RLS (acesso via chave `anon`) e ativa o
   Realtime na tabela.
3. Em **Settings &rarr; API**, copie:
   - **Project URL** (ex: `https://xxxxxxxx.supabase.co`)
   - **anon public key** (chave pública, segura para uso no navegador
     porque a tabela usa RLS)

## 2. Configurar o ESP32 Gateway

Em `water-tank-espnow/esp32_gateway/config.h`:

```cpp
#define SUPABASE_URL  "https://xxxxxxxx.supabase.co"
#define SUPABASE_KEY  "eyJ...sua_anon_key..."
```

Grave novamente o `esp32_gateway.ino`. A cada leitura recebida do sensor via
ESP-NOW, o gateway envia um `INSERT` para a tabela `leituras` (além dos
alertas de WhatsApp já existentes).

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

## Funcionamento

- Ao abrir, a dashboard busca a última leitura da tabela `leituras`.
- Se inscreve no **Realtime** do Supabase: cada novo `INSERT` (a cada
  ~30s, conforme `INTERVALO_MEDICAO_MS`) atualiza o nível animado e os
  cartões das 4 entradas instantaneamente.
- Um *polling* de segurança a cada 30s garante atualização mesmo se o
  Realtime cair.
- O indicador **Online / Sensor offline** no topo segue o mesmo limite de
  `TIMEOUT_SENSOR_MS` (2 minutos) usado no gateway.
