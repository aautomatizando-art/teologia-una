import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";
import { SENHA_PEDIDOS } from "@/lib/senhas";

// GET /api/compras → pedidos de compra agrupáveis por criticidade
export async function GET() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  let { data, error } = await supabase
    .from("pedidos_compra")
    .select("id, produto_id, quantidade, data, hora, solicitante, criticidade, status_rastreio, criado_em, rastreio_timestamps, recebido_por, produtos(nome)")
    .order("criado_em", { ascending: false });

  // Colunas rastreio_timestamps/recebido_por podem não existir ainda (migration pendente)
  if (error) {
    ({ data, error } = await supabase
      .from("pedidos_compra")
      .select("id, produto_id, quantidade, data, hora, solicitante, criticidade, status_rastreio, criado_em, produtos(nome)")
      .order("criado_em", { ascending: false }));
  }

  if (error) return Response.json({ error: error.message }, { status: 500 });
  return Response.json({ pedidos: data || [] });
}

// POST /api/compras → criar pedido de compra e notificar WhatsApp
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { produto_id, quantidade, data, hora, solicitante, criticidade, nome_cliente, regiao, comprador, vendedor, senha } = await req.json();
  if (senha !== SENHA_PEDIDOS) {
    return Response.json({ error: "Senha de pedidos inválida." }, { status: 403 });
  }
  if (!produto_id || !solicitante || !criticidade) {
    return Response.json({ error: "Preencha produto, nome e criticidade." }, { status: 400 });
  }
  if (!["EMERGENCIAL", "URGENTE", "MODERADO"].includes(criticidade)) {
    return Response.json({ error: "Criticidade inválida." }, { status: 400 });
  }

  const agora = new Date();
  const { data: novo, error } = await supabase
    .from("pedidos_compra")
    .insert({
      produto_id,
      quantidade: Number(quantidade) || 0,
      data: data || agora.toISOString().slice(0, 10),
      hora: hora || agora.toTimeString().slice(0, 5),
      solicitante,
      criticidade,
      nome_cliente: nome_cliente || null,
      regiao: regiao || null,
      comprador: comprador || null,
      vendedor: vendedor || null,
    })
    .select("id, produtos(nome)")
    .single();
  if (error) return Response.json({ error: error.message }, { status: 500 });

  const emoji = { EMERGENCIAL: "🟣", URGENTE: "🔴", MODERADO: "🟢" }[criticidade];
  const whatsapp = await enviarWhatsApp(
    `${emoji} *NOVO PEDIDO DE VENDA #${novo.id}*\n` +
      `Produto: ${novo.produtos?.nome}\n` +
      `Cliente: ${nome_cliente || "—"}\n` +
      `Região: ${regiao || "—"}\n` +
      `Qtd: ${Number(quantidade) || 0}\n` +
      `Solicitante: ${solicitante}\n` +
      `Data: ${data || agora.toISOString().slice(0, 10)} ${hora || agora.toTimeString().slice(0, 5)}`
  );

  return Response.json({ ok: true, id: novo.id, whatsapp });
}
