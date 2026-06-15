import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

// GET /api/ordens → lista ordens de produção com progresso e pedidos vinculados
export async function GET() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const [ordens, pedidosOp, registros] = await Promise.all([
    supabase
      .from("ordens_producao")
      .select("id, numero, meta_paletes, status, criado_em, finalizado_em, produtos(nome)")
      .order("criado_em", { ascending: false }),
    supabase.from("pedidos_op").select("ordem_id, codigo_pedido, qtd_planejada, quantidade_produzida"),
    supabase.from("producao_registros").select("ordem_id, paletes"),
  ]);
  if (ordens.error) return Response.json({ error: ordens.error.message }, { status: 500 });

  const lista = (ordens.data || []).map((o) => {
    const peds = (pedidosOp.data || []).filter((p) => p.ordem_id === o.id);
    const produzidoPaletes = (registros.data || [])
      .filter((r) => r.ordem_id === o.id)
      .reduce((s, r) => s + r.paletes, 0);
    const produzidoPedidos = peds.reduce((s, p) => s + (p.quantidade_produzida || 0), 0);
    const produzido = produzidoPaletes + produzidoPedidos;
    return {
      id: o.id,
      numero: o.numero,
      produto: o.produtos?.nome || null,
      meta_paletes: o.meta_paletes,
      status: o.status,
      criado_em: o.criado_em,
      finalizado_em: o.finalizado_em,
      produzido,
      percentual: o.meta_paletes ? Math.min(100, Math.round((produzido / o.meta_paletes) * 100)) : 0,
      pedidos: peds.map((p) => ({ codigo: p.codigo_pedido, qtd: p.qtd_planejada })),
    };
  });

  return Response.json({ ordens: lista });
}

// POST /api/ordens → cria OP a partir de pedidos de compra e avisa no WhatsApp
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { numero, data, hora, solicitante, meta_paletes, pedido_ids } = await req.json();
  if (!solicitante || !Array.isArray(pedido_ids) || pedido_ids.length === 0) {
    return Response.json({ error: "Selecione ao menos um pedido e informe o solicitante." }, { status: 400 });
  }

  const { data: pedidos, error: ePed } = await supabase
    .from("pedidos_compra")
    .select("id, quantidade, solicitante, criticidade, produtos(nome), produto_id")
    .in("id", pedido_ids);
  if (ePed) return Response.json({ error: ePed.message }, { status: 500 });
  if (!pedidos?.length) return Response.json({ error: "Pedidos não encontrados." }, { status: 404 });

  // numero automático se não informado: maior OP-<n> existente + 1
  let num = (numero || "").trim();
  if (!num) {
    const { data: ult } = await supabase
      .from("ordens_producao")
      .select("numero")
      .like("numero", "OP-%")
      .order("id", { ascending: false })
      .limit(50);
    const maior = (ult || [])
      .map((o) => parseInt(o.numero.replace("OP-", ""), 10))
      .filter((n) => !isNaN(n))
      .reduce((a, b) => Math.max(a, b), 1000);
    num = `OP-${maior + 1}`;
  }

  const meta = Number(meta_paletes) || pedidos.reduce((s, p) => s + (p.quantidade || 0), 0) || 100;

  const { data: nova, error: eOrd } = await supabase
    .from("ordens_producao")
    .insert({ numero: num, produto_id: pedidos[0].produto_id, meta_paletes: meta })
    .select("id")
    .single();
  if (eOrd) {
    const msg = eOrd.message.includes("duplicate")
      ? `Já existe uma ordem com o número "${num}".`
      : eOrd.message;
    return Response.json({ error: msg }, { status: 500 });
  }

  const { error: eVinc } = await supabase.from("pedidos_op").insert(
    pedidos.map((p) => ({ ordem_id: nova.id, codigo_pedido: `PD-${p.id}`, qtd_planejada: p.quantidade || 0 }))
  );
  if (eVinc) return Response.json({ error: eVinc.message }, { status: 500 });

  // garante etapa 1 (GERADO ORDEM DE PRODUÇÃO) no rastreio dos pedidos
  await supabase.from("pedidos_compra").update({ status_rastreio: 1 }).in("id", pedido_ids);

  const emoji = { EMERGENCIAL: "🟣", URGENTE: "🔴", MODERADO: "🟢" };
  const agora = new Date();
  const linhasPedidos = pedidos
    .map((p) => `${emoji[p.criticidade] || "•"} Pedido #${p.id}: ${p.produtos?.nome} — ${p.quantidade} un. (${p.solicitante})`)
    .join("\n");
  const whatsapp = await enviarWhatsApp(
    `🏭 *NOVA ORDEM DE PRODUÇÃO ${num}*\n` +
      `Produto: ${pedidos[0].produtos?.nome}\n` +
      `Meta: ${meta} paletes\n` +
      `Solicitante: ${solicitante}\n` +
      `Data: ${data || agora.toISOString().slice(0, 10)} ${hora || agora.toTimeString().slice(0, 5)}\n\n` +
      `Pedidos incluídos:\n${linhasPedidos}`
  );

  return Response.json({ ok: true, numero: num, id: nova.id, whatsapp });
}

// PATCH /api/ordens → muda status da ordem (ABERTA | CONCLUIDA | CANCELADA)
export async function PATCH(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { id, status } = await req.json();
  if (!id || !["ABERTA", "CONCLUIDA", "CANCELADA"].includes(status)) {
    return Response.json({ error: "Informe id e status válido." }, { status: 400 });
  }

  const { data: ordem, error } = await supabase
    .from("ordens_producao")
    .update({ status })
    .eq("id", id)
    .select("numero, produtos(nome)")
    .single();
  if (error) return Response.json({ error: error.message }, { status: 500 });

  const rotulo = { ABERTA: "🔄 REABERTA", CONCLUIDA: "✅ CONCLUÍDA", CANCELADA: "❌ CANCELADA" }[status];
  await enviarWhatsApp(
    `${rotulo} — Ordem de produção *${ordem.numero}*\nProduto: ${ordem.produtos?.nome || "—"}`
  );

  return Response.json({ ok: true });
}
