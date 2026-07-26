// ═══════════════════════════════════════════════════════════════════════════
// Edge Function: alerta-whatsapp
//
// Envia as notificações de não conformidade normativa para os destinatários
// cadastrados em amb_destinatarios. Suporta dois provedores:
//
//   evolution  — Evolution API auto-hospedada (é o que já roda no VPS
//                Hostinger dos outros projetos). Envia para números e para
//                grupos (destino terminado em @g.us).
//   meta       — WhatsApp Cloud API oficial da Meta. Fora da janela de 24 h
//                a Meta só entrega template aprovado, então o envio usa
//                template com parâmetros.
//
// Chamada:
//   POST { "alerta_ids": ["uuid", ...] }      -> notifica alertas específicos
//   POST { "modo": "resumo" }                 -> resumo diário de conformidade
//
// Deploy:
//   supabase functions deploy alerta-whatsapp
//
// Segredos:
//   WHATSAPP_PROVIDER=evolution|meta
//   EVO_BASE_URL, EVO_INSTANCE, EVO_APIKEY            (provider evolution)
//   META_PHONE_NUMBER_ID, META_TOKEN, META_TEMPLATE   (provider meta)
//   META_TEMPLATE_LANG (default pt_BR)
// ═══════════════════════════════════════════════════════════════════════════

import { createClient } from 'https://esm.sh/@supabase/supabase-js@2.45.0';

const SUPABASE_URL = Deno.env.get('SUPABASE_URL')!;
const SERVICE_KEY = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;
const PROVIDER = (Deno.env.get('WHATSAPP_PROVIDER') ?? 'evolution').toLowerCase();

const TZ = 'America/Sao_Paulo';

const cors = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'content-type, authorization',
  'Access-Control-Allow-Methods': 'POST, OPTIONS',
};

function json(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { ...cors, 'Content-Type': 'application/json' },
  });
}

const ICONE: Record<string, string> = { critico: '🔴', atencao: '🟡' };

/** Hora local (São Paulo) em minutos desde a meia-noite. */
function minutosAgora(): number {
  const partes = new Intl.DateTimeFormat('pt-BR', {
    timeZone: TZ, hour: '2-digit', minute: '2-digit', hour12: false,
  }).formatToParts(new Date());
  const h = Number(partes.find((p) => p.type === 'hour')?.value ?? 0);
  const m = Number(partes.find((p) => p.type === 'minute')?.value ?? 0);
  return h * 60 + m;
}

function minutosDeHora(hhmmss: string | null): number | null {
  if (!hhmmss) return null;
  const [h, m] = hhmmss.split(':').map(Number);
  return h * 60 + m;
}

/** Janela pode cruzar a meia-noite (ex.: 22:00 → 06:00). */
function dentroDaJanela(ini: string | null, fim: string | null): boolean {
  const agora = minutosAgora();
  const a = minutosDeHora(ini);
  const b = minutosDeHora(fim);
  if (a === null || b === null) return true;
  return a <= b ? agora >= a && agora <= b : agora >= a || agora <= b;
}

// ── Provedores ────────────────────────────────────────────────────────────

async function enviarEvolution(destino: string, texto: string): Promise<void> {
  const base = Deno.env.get('EVO_BASE_URL');
  const inst = Deno.env.get('EVO_INSTANCE');
  const key = Deno.env.get('EVO_APIKEY');
  if (!base || !inst || !key) throw new Error('Evolution API não configurada');

  const res = await fetch(`${base}/message/sendText/${inst}`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', apikey: key },
    body: JSON.stringify({ number: destino, text: texto }),
  });
  if (!res.ok) throw new Error(`Evolution ${res.status}: ${await res.text()}`);
}

async function enviarMeta(destino: string, texto: string, params: string[]): Promise<void> {
  const phoneId = Deno.env.get('META_PHONE_NUMBER_ID');
  const token = Deno.env.get('META_TOKEN');
  const template = Deno.env.get('META_TEMPLATE');
  if (!phoneId || !token) throw new Error('WhatsApp Cloud API não configurada');

  // Sem template configurado usa mensagem livre — só entrega dentro da
  // janela de 24 h após o usuário ter escrito para o número.
  const body = template
    ? {
        messaging_product: 'whatsapp',
        to: destino,
        type: 'template',
        template: {
          name: template,
          language: { code: Deno.env.get('META_TEMPLATE_LANG') ?? 'pt_BR' },
          components: [{
            type: 'body',
            parameters: params.map((t) => ({ type: 'text', text: t })),
          }],
        },
      }
    : {
        messaging_product: 'whatsapp',
        to: destino,
        type: 'text',
        text: { preview_url: false, body: texto },
      };

  const res = await fetch(`https://graph.facebook.com/v21.0/${phoneId}/messages`, {
    method: 'POST',
    headers: { 'Content-Type': 'application/json', Authorization: `Bearer ${token}` },
    body: JSON.stringify(body),
  });
  if (!res.ok) throw new Error(`Meta ${res.status}: ${await res.text()}`);
}

async function enviar(destino: string, texto: string, params: string[]): Promise<void> {
  if (PROVIDER === 'meta') return enviarMeta(destino, texto, params);
  return enviarEvolution(destino, texto);
}

// ── Modo 1: notificar alertas novos ───────────────────────────────────────

async function notificarAlertas(db: ReturnType<typeof createClient>, ids: string[]) {
  const { data: alertas, error } = await db
    .from('amb_alertas')
    .select(`
      id, parametro, severidade, valor, limite_min, limite_max, desvio_pct,
      norma, mensagem, aberto_em, ambiente_id,
      amb_ambientes ( codigo, nome, predio, andar ),
      amb_parametros ( nome, unidade, casas )
    `)
    .in('id', ids)
    .is('notificado_em', null);

  if (error) throw new Error(error.message);
  if (!alertas?.length) return { enviados: 0, motivo: 'nenhum alerta pendente' };

  const { data: destinatarios } = await db
    .from('amb_destinatarios')
    .select('*')
    .eq('ativo', true);

  let enviados = 0;
  const falhas: string[] = [];

  // Um alerta pode ir para vários destinatários; agrupa por destinatário para
  // não mandar três mensagens seguidas para a mesma pessoa.
  for (const dest of destinatarios ?? []) {
    if (!dentroDaJanela(dest.janela_ini, dest.janela_fim)) continue;

    const doDestinatario = alertas.filter((a) => {
      const sevOk = (dest.severidades ?? ['critico']).includes(a.severidade);
      const ambOk = !dest.ambientes?.length || dest.ambientes.includes(a.ambiente_id);
      return sevOk && ambOk;
    });
    if (!doDestinatario.length) continue;

    const linhas = doDestinatario.map((a) => {
      // deno-lint-ignore no-explicit-any
      const amb = a.amb_ambientes as any;
      // deno-lint-ignore no-explicit-any
      const par = a.amb_parametros as any;
      const casas = par?.casas ?? 0;
      const un = par?.unidade ?? '';
      const faixa = a.limite_min !== null && a.limite_max !== null
        ? `${a.limite_min}–${a.limite_max} ${un}`
        : a.limite_max !== null ? `≤ ${a.limite_max} ${un}` : `≥ ${a.limite_min} ${un}`;
      return (
        `${ICONE[a.severidade] ?? '⚪'} *${par?.nome ?? a.parametro}* — ${amb?.nome} (${amb?.codigo})\n` +
        `Lido: *${Number(a.valor).toFixed(casas)} ${un}*  ·  Norma: ${faixa}` +
        (a.desvio_pct ? `  ·  ${a.desvio_pct}% fora` : '') + '\n' +
        `_${a.norma}_`
      );
    });

    const hora = new Intl.DateTimeFormat('pt-BR', {
      timeZone: TZ, dateStyle: 'short', timeStyle: 'short',
    }).format(new Date());

    const texto =
      `🏫 *Monitoramento de Ambientes*\n` +
      `_Não conformidade normativa · ${hora}_\n\n` +
      linhas.join('\n\n') +
      `\n\nAbra a dashboard para o histórico e o gráfico da norma.`;

    // Parâmetros do template da Meta (na ordem do corpo aprovado).
    const p0 = doDestinatario[0];
    // deno-lint-ignore no-explicit-any
    const amb0 = p0.amb_ambientes as any;
    // deno-lint-ignore no-explicit-any
    const par0 = p0.amb_parametros as any;
    const params = [
      amb0?.nome ?? '—',
      par0?.nome ?? p0.parametro,
      `${Number(p0.valor).toFixed(par0?.casas ?? 0)} ${par0?.unidade ?? ''}`,
      p0.norma ?? '—',
    ];

    try {
      await enviar(dest.destino, texto, params);
      enviados++;
    } catch (e) {
      falhas.push(`${dest.nome}: ${(e as Error).message}`);
      console.error('[alerta-whatsapp] falha ao enviar:', e);
    }
  }

  await db
    .from('amb_alertas')
    .update({
      notificado_em: new Date().toISOString(),
      canal: 'whatsapp',
      erro_envio: falhas.length ? falhas.join(' | ') : null,
    })
    .in('id', alertas.map((a) => a.id));

  return { enviados, alertas: alertas.length, falhas };
}

// ── Modo 2: resumo diário ─────────────────────────────────────────────────
// Agende com pg_cron chamando esta função com { "modo": "resumo" }.

async function enviarResumo(db: ReturnType<typeof createClient>) {
  const { data: indices } = await db
    .from('amb_v_indice_ambiente')
    .select('*')
    .order('indice_pct', { ascending: true });

  if (!indices?.length) return { enviados: 0, motivo: 'sem leituras' };

  const geral = Math.round(
    indices.reduce((s, i) => s + Number(i.indice_pct ?? 0), 0) / indices.length,
  );

  const linhas = indices.map((i) => {
    const ic = i.status === 'conforme' ? '🟢' : i.status === 'atencao' ? '🟡' : '🔴';
    return `${ic} ${i.ambiente_nome} — *${i.indice_pct}%* (${i.nao_conformes} fora da norma)`;
  });

  const dia = new Intl.DateTimeFormat('pt-BR', { timeZone: TZ, dateStyle: 'long' })
    .format(new Date());

  const texto =
    `📊 *Resumo diário — Conformidade normativa*\n_${dia}_\n\n` +
    `Índice geral: *${geral}%*\n\n` + linhas.join('\n');

  const { data: destinatarios } = await db
    .from('amb_destinatarios')
    .select('*')
    .eq('ativo', true);

  let enviados = 0;
  for (const dest of destinatarios ?? []) {
    try {
      await enviar(dest.destino, texto, [dia, `${geral}%`, String(indices.length), '—']);
      enviados++;
    } catch (e) {
      console.error('[alerta-whatsapp] resumo falhou:', e);
    }
  }
  return { enviados, ambientes: indices.length, indice_geral: geral };
}

// ── Handler ───────────────────────────────────────────────────────────────

Deno.serve(async (req) => {
  if (req.method === 'OPTIONS') return new Response('ok', { headers: cors });
  if (req.method !== 'POST') return json({ erro: 'Use POST' }, 405);

  const db = createClient(SUPABASE_URL, SERVICE_KEY, { auth: { persistSession: false } });

  let body: { alerta_ids?: string[]; modo?: string } = {};
  try {
    body = await req.json();
  } catch { /* corpo vazio = resumo */ }

  try {
    if (body.modo === 'resumo') return json(await enviarResumo(db));
    if (!body.alerta_ids?.length) return json({ erro: 'alerta_ids vazio' }, 400);
    return json(await notificarAlertas(db, body.alerta_ids));
  } catch (e) {
    console.error('[alerta-whatsapp]', e);
    return json({ erro: (e as Error).message }, 500);
  }
});
