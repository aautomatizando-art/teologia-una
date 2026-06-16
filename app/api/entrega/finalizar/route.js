import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

// POST /api/entrega/finalizar — Marcar entrega como finalizada
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { pedido_id, recebido_por } = await req.json();
  if (!pedido_id) {
    return Response.json({ error: "pedido_id é obrigatório" }, { status: 400 });
  }
  if (!recebido_por || !recebido_por.trim()) {
    return Response.json({ error: "Informe o nome de quem recebeu a mercadoria" }, { status: 400 });
  }

  try {
    // Busca informações do pedido (rastreio_timestamps pode não existir ainda)
    let { data: pedido } = await supabase
      .from("pedidos_compra")
      .select("id, produtos(nome), solicitante, quantidade, rastreio_timestamps")
      .eq("id", pedido_id)
      .single();

    if (!pedido) {
      ({ data: pedido } = await supabase
        .from("pedidos_compra")
        .select("id, produtos(nome), solicitante, quantidade")
        .eq("id", pedido_id)
        .single());
    }

    if (!pedido) {
      return Response.json({ error: "Pedido não encontrado" }, { status: 404 });
    }

    const timestamps = { ...(pedido.rastreio_timestamps || {}), 6: new Date().toISOString() };

    // Atualiza status de rastreio para ENTREGUE/FINALIZADO (6) e registra quem recebeu
    let { error: eUpdate } = await supabase
      .from("pedidos_compra")
      .update({ status_rastreio: 6, rastreio_timestamps: timestamps, recebido_por: recebido_por.trim() })
      .eq("id", pedido_id);

    // Colunas rastreio_timestamps/recebido_por podem não existir ainda (migration pendente)
    if (eUpdate) {
      ({ error: eUpdate } = await supabase
        .from("pedidos_compra")
        .update({ status_rastreio: 6 })
        .eq("id", pedido_id));
    }

    if (eUpdate) {
      return Response.json({ error: eUpdate.message }, { status: 500 });
    }

    // Atualiza status da OP vinculada para ENTREGUE
    const { data: pedidoOp } = await supabase
      .from("pedidos_op")
      .select("ordem_id")
      .eq("codigo_pedido", `PD-${pedido_id}`)
      .maybeSingle();
    if (pedidoOp?.ordem_id) {
      await supabase
        .from("ordens_producao")
        .update({ status: "ENTREGUE" })
        .eq("id", pedidoOp.ordem_id);
    }

    // Envia notificação WhatsApp
    await enviarWhatsApp(
      `✅ *ENTREGA FINALIZADA*\n` +
      `Pedido: #${pedido.id}\n` +
      `Produto: ${pedido.produtos?.nome}\n` +
      `Quantidade: ${pedido.quantidade} un.\n` +
      `Solicitante: ${pedido.solicitante}\n` +
      `Recebido por: ${recebido_por.trim()}\n` +
      `Status: 🎉 ENTREGUE / FINALIZADO`
    );

    return Response.json({ ok: true });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
