// ═══════════════════════════════════════════════════════════════════════════
// Edge Function: ingest
//
// Recebe a telemetria do ESP32, grava a leitura, avalia cada parâmetro contra
// os limites normativos gravados em amb_limites, e abre/fecha alertas com
// histerese. Alertas novos são despachados para a função alerta-whatsapp.
//
// Deploy (o ESP32 não tem JWT, então a verificação precisa ser desligada):
//   supabase functions deploy ingest --no-verify-jwt
//
// Segredos necessários:
//   SUPABASE_URL, SUPABASE_SERVICE_ROLE_KEY  (injetados pela plataforma)
// ═══════════════════════════════════════════════════════════════════════════

import { createClient } from 'https://esm.sh/@supabase/supabase-js@2.45.0';

const SUPABASE_URL = Deno.env.get('SUPABASE_URL')!;
const SERVICE_KEY = Deno.env.get('SUPABASE_SERVICE_ROLE_KEY')!;

// Quantas leituras consecutivas fora da faixa antes de abrir um alerta.
// Evita disparar WhatsApp por um pico isolado (alguém acendeu a luz, abriu a
// porta, passou perto do sensor).
const AMOSTRAS_PARA_ABRIR = 3;

// Margem de retorno para fechar o alerta. O valor precisa voltar 5% para
// dentro do limite — sem isso o alerta ficaria abrindo e fechando em torno
// da fronteira.
const DEADBAND_PCT = 5;

// Colunas de amb_leituras que são medidas comparáveis a um limite normativo.
const PARAMETROS = [
  'lux', 'ugr', 'uo', 'ra', 'tcc', 'flicker',
  'co2', 'pm25', 'pm10', 'voc', 'nox', 'hcho', 'co', 'ar_ext',
  'temp', 'ur', 'vel_ar', 'ruido',
] as const;

// Colunas aceitas no corpo da requisição (as de medida + contexto).
const COLUNAS_LEITURA = [
  ...PARAMETROS,
  'co2_ext', 'pm1', 'pm4', 'pressao_hpa', 'ruido_max',
  'ocupacao', 'presenca', 'rssi_dbm', 'uptime_s',
];

type Limite = {
  parametro: string;
  minimo: number | null;
  maximo: number | null;
  alvo: number | null;
  delta_externo: boolean;
  so_ocupado: boolean;
  tol_atencao_pct: number;
  norma: string;
};

const cors = {
  'Access-Control-Allow-Origin': '*',
  'Access-Control-Allow-Headers': 'content-type, x-device-token',
  'Access-Control-Allow-Methods': 'POST, OPTIONS',
};

function json(body: unknown, status = 200) {
  return new Response(JSON.stringify(body), {
    status,
    headers: { ...cors, 'Content-Type': 'application/json' },
  });
}

async function sha256Hex(texto: string): Promise<string> {
  const buf = await crypto.subtle.digest('SHA-256', new TextEncoder().encode(texto));
  return [...new Uint8Array(buf)].map((b) => b.toString(16).padStart(2, '0')).join('');
}

// Comparação em tempo constante — impede descobrir o token byte a byte
// medindo o tempo de resposta.
function comparaSeguro(a: string, b: string): boolean {
  if (a.length !== b.length) return false;
  let diff = 0;
  for (let i = 0; i < a.length; i++) diff |= a.charCodeAt(i) ^ b.charCodeAt(i);
  return diff === 0;
}

/** Limite máximo efetivo — a NBR 17037 define o CO₂ como diferencial sobre o
 *  ar externo, então o teto depende da leitura externa do momento. */
function maximoEfetivo(lim: Limite, co2Ext: number | null): number | null {
  if (lim.maximo === null) return null;
  return lim.delta_externo ? lim.maximo + (co2Ext ?? 420) : lim.maximo;
}

/** Espelha amb_status_valor() do Postgres — as duas implementações precisam
 *  concordar, senão a dashboard e o alerta discordam sobre o mesmo valor.
 *  Em faixa fechada a margem de atenção é % da largura da faixa; em limite
 *  simples, % do próprio limite. */
function classifica(
  valor: number,
  minimo: number | null,
  maximo: number | null,
  tolPct: number,
): 'conforme' | 'atencao' | 'nao_conforme' {
  if (maximo !== null && valor > maximo) return 'nao_conforme';
  if (minimo !== null && valor < minimo) return 'nao_conforme';
  if (minimo !== null && maximo !== null) {
    const margem = (maximo - minimo) * tolPct / 100;
    return (valor >= maximo - margem || valor <= minimo + margem) ? 'atencao' : 'conforme';
  }
  if (maximo !== null && valor >= maximo * (1 - tolPct / 100)) return 'atencao';
  if (minimo !== null && valor <= minimo * (1 + tolPct / 100)) return 'atencao';
  return 'conforme';
}

/** Quanto o valor excedeu o limite violado, em %. Zero se dentro da faixa. */
function desvioPct(valor: number, minimo: number | null, maximo: number | null): number {
  if (maximo !== null && valor > maximo && maximo !== 0) {
    return Math.round(((valor - maximo) / maximo) * 1000) / 10;
  }
  if (minimo !== null && valor < minimo && minimo !== 0) {
    return Math.round(((minimo - valor) / minimo) * 1000) / 10;
  }
  return 0;
}

Deno.serve(async (req) => {
  if (req.method === 'OPTIONS') return new Response('ok', { headers: cors });
  if (req.method !== 'POST') return json({ erro: 'Use POST' }, 405);

  const token = req.headers.get('x-device-token');
  if (!token) return json({ erro: 'Header x-device-token ausente' }, 401);

  let payload: Record<string, unknown>;
  try {
    payload = await req.json();
  } catch {
    return json({ erro: 'JSON inválido' }, 400);
  }

  const deviceId = String(payload.device_id ?? '').trim();
  if (!deviceId) return json({ erro: 'device_id obrigatório' }, 400);

  const db = createClient(SUPABASE_URL, SERVICE_KEY, {
    auth: { persistSession: false },
  });

  // ── 1. Autenticação do dispositivo ──────────────────────────────────────
  const { data: disp, error: errDisp } = await db
    .from('amb_dispositivos')
    .select('id, device_id, ambiente_id, ativo, token_hash')
    .eq('device_id', deviceId)
    .maybeSingle();

  if (errDisp) return json({ erro: 'Falha ao consultar dispositivo' }, 500);
  if (!disp || !disp.ativo) return json({ erro: 'Dispositivo não autorizado' }, 401);
  if (!disp.ambiente_id) return json({ erro: 'Dispositivo sem ambiente vinculado' }, 409);

  const hash = await sha256Hex(token);
  if (!comparaSeguro(hash, String(disp.token_hash).toLowerCase())) {
    return json({ erro: 'Token inválido' }, 401);
  }

  // ── 2. Grava a leitura ──────────────────────────────────────────────────
  const leitura: Record<string, unknown> = {
    ambiente_id: disp.ambiente_id,
    device_id: deviceId,
    lido_em: new Date().toISOString(),
    bruto: payload.bruto ?? null,
  };
  for (const col of COLUNAS_LEITURA) {
    const v = payload[col];
    if (v === undefined || v === null) continue;
    if (col === 'presenca') {
      leitura[col] = Boolean(v);
    } else if (Number.isFinite(Number(v))) {
      leitura[col] = Number(v);
    }
  }

  const { data: linha, error: errIns } = await db
    .from('amb_leituras')
    .insert(leitura)
    .select('id, lido_em')
    .single();

  if (errIns) return json({ erro: 'Falha ao gravar leitura', detalhe: errIns.message }, 500);

  await db
    .from('amb_dispositivos')
    .update({
      ultimo_contato: linha.lido_em,
      rssi_dbm: leitura.rssi_dbm ?? null,
      uptime_s: leitura.uptime_s ?? null,
      fw_versao: payload.fw_versao ?? undefined,
      ip: payload.ip ?? undefined,
    })
    .eq('id', disp.id);

  // ── 3. Avalia contra a norma ────────────────────────────────────────────
  const { data: ambiente } = await db
    .from('amb_ambientes')
    .select('id, codigo, nome, tipo')
    .eq('id', disp.ambiente_id)
    .single();

  const { data: limites } = await db
    .from('amb_limites')
    .select('parametro, minimo, maximo, alvo, delta_externo, so_ocupado, tol_atencao_pct, norma')
    .eq('tipo_ambiente', ambiente!.tipo);

  const { data: nomes } = await db
    .from('amb_parametros')
    .select('chave, nome, unidade, casas');
  const meta = new Map((nomes ?? []).map((p) => [p.chave, p]));

  const { data: abertos } = await db
    .from('amb_alertas')
    .select('id, parametro, severidade, valor')
    .eq('ambiente_id', disp.ambiente_id)
    .is('fechado_em', null);
  const alertasAbertos = new Map((abertos ?? []).map((a) => [a.parametro, a]));

  // Histórico curto para a histerese de abertura.
  const { data: recentes } = await db
    .from('amb_leituras')
    .select(PARAMETROS.join(',') + ',co2_ext,lido_em')
    .eq('ambiente_id', disp.ambiente_id)
    .order('lido_em', { ascending: false })
    .limit(AMOSTRAS_PARA_ABRIR);

  const co2Ext = (leitura.co2_ext as number | undefined) ?? null;
  const avaliacao: Record<string, unknown>[] = [];
  const novosAlertas: string[] = [];

  // A presença pode vir de OUTRO nó da mesma sala: numa instalação com dois
  // módulos, quem detecta presença é o de teto e quem mede ruído é o de
  // parede. Sem consultar o contexto da sala, o nó de parede avaliaria
  // ruído e iluminância como se a sala estivesse sempre em uso.
  let ocupado = leitura.presenca as boolean | undefined;
  if (ocupado === undefined) {
    const { data: ctx } = await db
      .from('amb_v_contexto_atual')
      .select('presenca')
      .eq('ambiente_id', disp.ambiente_id)
      .maybeSingle();
    if (ctx?.presenca !== null && ctx?.presenca !== undefined) ocupado = ctx.presenca;
  }

  for (const lim of (limites ?? []) as Limite[]) {
    const valor = leitura[lim.parametro] as number | undefined;
    if (valor === undefined) continue;

    // Sala vazia não gera alerta de iluminância, ruído, flicker, temperatura
    // de cor ou vazão por pessoa — escuro e silencioso fora do expediente é o
    // comportamento esperado, não uma não conformidade.
    if (lim.so_ocupado && ocupado === false) continue;

    const max = maximoEfetivo(lim, co2Ext);
    const min = lim.minimo;
    const status = classifica(valor, min, max, lim.tol_atencao_pct);
    const info = meta.get(lim.parametro);

    avaliacao.push({
      parametro: lim.parametro,
      valor,
      limite_min: min,
      limite_max: max,
      status,
    });

    const jaAberto = alertasAbertos.get(lim.parametro);

    // ── Fecha alerta: valor voltou para dentro da faixa com folga ─────────
    if (jaAberto && status === 'conforme') {
      const folgaOk =
        (max === null || valor <= max * (1 - DEADBAND_PCT / 100)) &&
        (min === null || valor >= min * (1 + DEADBAND_PCT / 100));
      if (folgaOk) {
        await db
          .from('amb_alertas')
          .update({ fechado_em: new Date().toISOString() })
          .eq('id', jaAberto.id);
      }
      continue;
    }

    if (status === 'conforme' || jaAberto) continue;

    // ── Abre alerta: exige AMOSTRAS_PARA_ABRIR leituras seguidas fora ─────
    const historico = (recentes ?? []) as Record<string, number | null>[];
    if (historico.length < AMOSTRAS_PARA_ABRIR) continue;

    const todasFora = historico.every((h) => {
      const v = h[lim.parametro];
      if (v === null || v === undefined) return false;
      const m = maximoEfetivo(lim, (h.co2_ext as number | null) ?? co2Ext);
      return classifica(Number(v), min, m, lim.tol_atencao_pct) !== 'conforme';
    });
    if (!todasFora) continue;

    const severidade = status === 'nao_conforme' ? 'critico' : 'atencao';
    const casas = info?.casas ?? 0;
    const un = info?.unidade ?? '';
    const faixa = min !== null && max !== null
      ? `${min} a ${max} ${un}`
      : max !== null
        ? `≤ ${max} ${un}`
        : `≥ ${min} ${un}`;

    const mensagem =
      `${ambiente!.nome} (${ambiente!.codigo})\n` +
      `${info?.nome ?? lim.parametro}: ${valor.toFixed(casas)} ${un}\n` +
      `Faixa normativa: ${faixa}\n` +
      `Norma: ${lim.norma}`;

    const { data: novo } = await db
      .from('amb_alertas')
      .insert({
        ambiente_id: disp.ambiente_id,
        parametro: lim.parametro,
        severidade,
        valor,
        limite_min: min,
        limite_max: max,
        desvio_pct: desvioPct(valor, min, max),
        norma: lim.norma,
        mensagem,
      })
      .select('id')
      .single();

    if (novo) novosAlertas.push(novo.id);
  }

  // ── 4. Dispara as notificações (sem bloquear a resposta ao ESP32) ───────
  if (novosAlertas.length > 0) {
    const disparo = fetch(`${SUPABASE_URL}/functions/v1/alerta-whatsapp`, {
      method: 'POST',
      headers: {
        'Content-Type': 'application/json',
        Authorization: `Bearer ${SERVICE_KEY}`,
      },
      body: JSON.stringify({ alerta_ids: novosAlertas }),
    }).catch((e) => console.error('[ingest] falha ao chamar alerta-whatsapp:', e));

    // waitUntil mantém a chamada viva depois que a resposta é devolvida.
    // @ts-ignore — disponível no runtime de Edge Functions
    if (typeof EdgeRuntime !== 'undefined') EdgeRuntime.waitUntil(disparo);
    else await disparo;
  }

  return json({
    ok: true,
    leitura_id: linha.id,
    ambiente: ambiente!.codigo,
    avaliacao,
    alertas_abertos: novosAlertas.length,
  });
});
