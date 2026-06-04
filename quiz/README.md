# ✝️ Quiz Bíblico Pentecostal — projeto Vercel dedicado

Esta pasta é um **projeto independente** para a Vercel: serve apenas o Quiz Bíblico
na **raiz** do domínio (`/`), separado do restante do repositório.

## Como criar o projeto na Vercel (uma vez)
1. Acesse **vercel.com → Add New… → Project**.
2. Importe o repositório **`aautomatizando-art/teologia-una`**.
3. Em **Root Directory**, clique em *Edit* e selecione a pasta **`quiz`**.
4. Framework Preset: **Other** (é estático, sem build).
5. Dê um nome ao projeto, ex.: **`quiz-biblico`**, e clique em **Deploy**.

Pronto — o quiz ficará no domínio próprio, ex.: `https://quiz-biblico.vercel.app`
(abre direto na raiz, sem `/quiz`).

> Cada `git push` na branch de produção (main) re-publica automaticamente este projeto.

## Conteúdo
- `index.html` — a dashboard do quiz (mesma do `/quiz` da raiz).
- `vercel.json` — serve o `index.html` na raiz.
- `supabase-setup.sql` — esquema + perguntas (rode no SQL Editor do Supabase).
- `firmware/esp32_quiz_biblico/…ino` — firmware do ESP32.

## Hardware / Supabase / uso
Veja o guia completo em [`../QUIZ-BIBLICO.md`](../QUIZ-BIBLICO.md).
Pinagem resumida: botões 1–5 → GPIO 13/12/14/27/15 · saídas jogador 1–8 →
16/17/18/19/21/22/23/4 · buzzer → 25 · **saída de castigo → 33**.
