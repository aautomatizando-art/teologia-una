import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

// GET /api/reservas → últimas reservas
export async function GET() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { data, error } = await supabase
    .from("reservas")
    .select("id, quantidade, pedido, ordem_producao, solicitante, data, hora, produtos(nome)")
    .order("criado_em", { ascending: false })
    .limit(30);

  if (error) return Response.json({ error: error.message }, { status: 500 });
  return Response.json({ reservas: data || [] });
}

// POST /api/reservas → criar reserva, baixar estoque e notificar WhatsApp
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { produto_id, quantidade, pedido, ordem_producao, solicitante, data, hora } = await req.json();
  const qtd = Number(quantidade) || 0;
  if (!produto_id || qtd <= 0 || !solicitante) {
    return Response.json({ error: "Preencha produto, quantidade e solicitante." }, { status: 400 });
  }

  const { data: produto, error: e1 } = await supabase
    .from("produtos")
    .select("id, nome, quantidade, qtd_minima")
    .eq("id", produto_id)
    .single();
  if (e1) return Response.json({ error: e1.message }, { status: 500 });
  if (produto.quantidade < qtd) {
    return Response.json(
      { error: `Estoque insuficiente: há apenas ${produto.quantidade} un. de ${produto.nome}.` },
      { status: 409 }
    );
  }

  const agora = new Date();
  const { error: e2 } = await supabase.from("reservas").insert({
    produto_id,
    quantidade: qtd,
    pedido: pedido || null,
    ordem_producao: ordem_producao || null,
    solicitante,
    data: data || agora.toISOString().slice(0, 10),
    hora: hora || agora.toTimeString().slice(0, 5),
  });
  if (e2) return Response.json({ error: e2.message }, { status: 500 });

  const novaQtd = produto.quantidade - qtd;
  const { error: e3 } = await supabase
    .from("produtos")
    .update({ quantidade: novaQtd })
    .eq("id", produto_id);
  if (e3) return Response.json({ error: e3.message }, { status: 500 });

  let msg =
    `📦 *RESERVA DE ESTOQUE*\n` +
    `Produto: ${produto.nome}\n` +
    `Qtd reservada: ${qtd}\n` +
    (pedido ? `Pedido: ${pedido}\n` : "") +
    (ordem_producao ? `OP: ${ordem_producao}\n` : "") +
    `Solicitante: ${solicitante}\n` +
    `Saldo atual: ${novaQtd}`;
  if (novaQtd < produto.qtd_minima) {
    msg += `\n\n⚠️ *ATENÇÃO: ESTOQUE BAIXO!* (mínimo: ${produto.qtd_minima})`;
  }
  const whatsapp = await enviarWhatsApp(msg);

  return Response.json({ ok: true, nova_quantidade: novaQtd, whatsapp });
}
