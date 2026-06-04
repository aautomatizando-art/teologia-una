# ✝️ Quiz Bíblico Pentecostal — Dashboard + ESP32

Dashboard moderno de quiz bíblico de base pentecostal, hospedado na **Vercel**
(projeto dedicado, Root Directory = `quiz`), com banco **Supabase** e integração
com **ESP32** (até 8 jogadores).

- **Online:** domínio próprio do projeto, ex.: `https://quiz-biblico.vercel.app` (abre na raiz).
- **Local:** abra `index.html` direto no navegador (funciona offline, com perguntas embutidas e teclado).

---

## 🎮 Como funciona

1. **Sorteio** — o botão *Sortear Jogador* escolhe aleatoriamente 1 dos jogadores ativos
   e envia o comando ao ESP32, que **aciona a saída** correspondente (LED/relé/sinaleiro
   na bancada daquele jogador).
2. **Pergunta** — aparece uma pergunta com **5 alternativas numeradas (1 a 5)**.
3. **Resposta** — o jogador sorteado responde apertando **um dos 5 botões** do ESP32
   (ou as teclas `1`–`5`, ou clicando).
4. **Cronômetro** — tempo configurável (padrão 30 s). Se zerar, conta como erro.
5. **Resultado:**
   - ✅ **Acertou** → mensagem de **Parabéns**, soma pontos (base do nível + bônus de tempo)
     e toca o som de acerto no buzzer.
   - ❌ **Errou / tempo esgotado** → mostra a resposta certa, sorteia um **castigo bíblico**,
     toca o som de erro e **liga a saída de castigo do ESP32** por X segundos (configurável),
     exibindo a animação de **esteira** enquanto a saída estiver ligada.
6. **Placar** lateral com ranking, acertos e aproveitamento.
7. **📖 Ler versículo (ARC)** — botão no resultado revela o versículo na Almeida Revista e Corrigida.

### Níveis
- 🟢 **Básico** (10 pts) · 🟡 **Médio** (20 pts) · 🔴 **Avançado** (30 pts) · 🟣 **Teológico** (50 pts)
- Bônus: + segundos restantes do cronômetro.

### 🔐 Gerenciar Perguntas (admin)
Botão **Gerenciar Perguntas** (senha padrão `1234`, alterável) abre o cadastro de
perguntas por nível: pergunta, 5 alternativas, alternativa correta, referência e
versículo ARC. Salva no Supabase (se configurado) ou localmente no navegador.

---

## 🔌 Hardware (ESP32)

Firmware: [`firmware/esp32_quiz_biblico/esp32_quiz_biblico.ino`](firmware/esp32_quiz_biblico/esp32_quiz_biblico.ino)

| Função | GPIO |
|---|---|
| Botões resposta 1–5 | 13, 12, 14, 27, 15 *(INPUT_PULLUP → botão entre pino e GND)* |
| Saídas jogador 1–8 | 16, 17, 18, 19, 21, 22, 23, 4 *(nível alto = acionado)* |
| Buzzer | 25 |
| **Saída de castigo** | **33** *(relé/sirene/lâmpada, ligada por X s ao errar)* |
| LED de status | 2 |

**Bibliotecas (Arduino IDE → Library Manager):** `WebSockets` (Markus Sattler) e `ArduinoJson` (6.x).
Edite `SSID` / `PASSWORD` no `.ino` antes de gravar. O ESP imprime o IP no Serial Monitor (115200).

### Protocolo
- ESP → Dashboard: `{"event":"button","btn":2}` ao apertar; estado a cada 200 ms (inclui `castigo`).
- Dashboard → ESP: `{"cmd":"sorteio","player":3}`, `{"cmd":"alloff"}`,
  `{"cmd":"resultado","ok":true}`, `{"cmd":"castigo","ms":5000}`.

---

## ☁️ Supabase

1. Crie um projeto em [app.supabase.com](https://app.supabase.com).
2. Em **SQL Editor**, rode o conteúdo de [`supabase-setup.sql`](supabase-setup.sql)
   (tabelas, RLS, perguntas dos 4 níveis, castigos e versículos ARC). É idempotente.
3. Em **Project Settings → API**, copie a **URL** e a **anon key**.
4. Na dashboard, cole nos campos *Supabase URL* / *anon key* e clique em **Salvar**.

> Sem Supabase a dashboard ainda funciona, usando as perguntas embutidas no HTML.
> Para adicionar perguntas, use o botão **Gerenciar Perguntas** ou insira linhas em
> `quiz_perguntas` (5 opções, `correta` de 1 a 5).

---

## 🌐 Conexão ESP32 ↔ Dashboard

A Vercel serve por **HTTPS**, então o navegador bloqueia `ws://` para IPs locais.
Duas opções:

- **Relay (recomendado p/ qualquer rede):** rode o `relay` (servidor WS genérico, em `../encoder/relay`):
  ```bash
  cd ../encoder/relay && npm install
  ESP32_IP=192.168.1.100 npm start
  npx localtunnel --port 3001   # ou cloudflared / ngrok
  ```
  Cole a URL `wss://…` no campo *Relay / WS URL* da dashboard.
- **Rede local direta:** abra o `index.html` salvo no PC (não em HTTPS) e informe o IP do ESP32.

---

## ⌨️ Atalhos
`1`–`5` respondem · `S` sorteia · `Espaço` avança para a próxima rodada.
