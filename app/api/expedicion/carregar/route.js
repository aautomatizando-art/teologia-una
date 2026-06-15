import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

// POST /api/expedicion/carregar — Finalizar carregamento
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const body = await req.json();
  const { codigo_pedido, quantidade_carregada, responsavel_expedicao, nome_motorista, nome_ajudante, veiculo } = body;

  if (!codigo_pedido) {
    return Response.json({ error: "codigo_pedido é obrigatório. Recebido: " + JSON.stringify(body) }, { status: 400 });
  }
  if (!quantidade_carregada) {
    return Response.json({ error: "quantidade_carregada é obrigatória." }, { status: 400 });
  }
  if (!responsavel_expedicao) {
    return Response.json({ error: "responsavel_expedicao é obrigatório." }, { status: 400 });
  }
  if (!nome_motorista) {
    return Response.json({ error: "nome_motorista é obrigatório." }, { status: 400 });
  }
  if (!nome_ajudante) {
    return Response.json({ error: "nome_ajudante é obrigatório." }, { status: 400 });
  }
  if (!veiculo) {
    return Response.json({ error: "veiculo é obrigatório." }, { status: 400 });
  }

  try {
    // Busca o pedido_op para registrar carregamento
    const { data: pedidoOp } = await supabase
      .from("pedidos_op")
      .select("id")
      .eq("codigo_pedido", codigo_pedido)
      .single();

    if (pedidoOp?.id) {
      // Registrar carregamento
      await supabase.from("expedicao_carregamentos").insert({
        pedido_op_id: pedidoOp.id,
        quantidade_carregada: Number(quantidade_carregada),
        responsavel_expedicao,
        nome_motorista,
        nome_ajudante,
        veiculo,
      });
    }

    // Busca dados do pedido para WhatsApp
    const pedidoId = Number(codigo_pedido.replace("PD-", ""));
    const { data: pedidoCompra } = await supabase
      .from("pedidos_compra")
      .select("id, produtos(nome), solicitante")
      .eq("id", pedidoId)
      .single();

    // Atualizar status de rastreio para EM ROTA (5)
    await supabase
      .from("pedidos_compra")
      .update({ status_rastreio: 5 })
      .eq("id", pedidoCompra?.id);

    // Enviar WhatsApp
    await enviarWhatsApp(
      `🚚 *CARREGAMENTO FINALIZADO - EM ROTA*\n` +
      `Pedido: #${pedidoCompra?.id}\n` +
      `Produto: ${pedidoCompra?.produtos?.nome}\n` +
      `Quantidade: ${quantidade_carregada} un.\n` +
      `Motorista: ${nome_motorista}\n` +
      `Ajudante: ${nome_ajudante}\n` +
      `Veículo: ${veiculo}\n` +
      `Status: 🚛 EM ROTA PARA ENTREGA`
    );

    return Response.json({ ok: true });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
