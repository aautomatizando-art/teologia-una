import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

// POST /api/entrega/finalizar — Marcar entrega como finalizada
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { pedido_id } = await req.json();
  if (!pedido_id) {
    return Response.json({ error: "pedido_id é obrigatório" }, { status: 400 });
  }

  try {
    // Busca informações do pedido
    const { data: pedido } = await supabase
      .from("pedidos_compra")
      .select("id, produtos(nome), solicitante, quantidade")
      .eq("id", pedido_id)
      .single();

    if (!pedido) {
      return Response.json({ error: "Pedido não encontrado" }, { status: 404 });
    }

    // Atualiza status de rastreio para ENTREGUE/FINALIZADO (6)
    const { error: eUpdate } = await supabase
      .from("pedidos_compra")
      .update({ status_rastreio: 6 })
      .eq("id", pedido_id);

    if (eUpdate) {
      return Response.json({ error: eUpdate.message }, { status: 500 });
    }

    // Envia notificação WhatsApp
    await enviarWhatsApp(
      `✅ *ENTREGA FINALIZADA*\n` +
      `Pedido: #${pedido.id}\n` +
      `Produto: ${pedido.produtos?.nome}\n` +
      `Quantidade: ${pedido.quantidade} un.\n` +
      `Solicitante: ${pedido.solicitante}\n` +
      `Status: 🎉 ENTREGUE / FINALIZADO`
    );

    return Response.json({ ok: true });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
