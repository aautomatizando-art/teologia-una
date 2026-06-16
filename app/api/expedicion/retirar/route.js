import { createHash } from "crypto";
import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

// POST /api/expedicion/retirar — Retirada de estoque para expedição
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const body = await req.json();
  const { codigo_pedido, quantidade, nome, senha } = body;

  if (!codigo_pedido || !quantidade || !nome || !senha) {
    return Response.json({ error: "Informe todos os campos." }, { status: 400 });
  }

  // Validar senha (padrão: "Expedicaofrit")
  if (senha !== "Expedicaofrit") {
    return Response.json({ error: "Senha de expedição inválida." }, { status: 403 });
  }

  try {
    // Extrai ID do pedido do código (PC-11 → 11)
    const pedidoId = Number(codigo_pedido.replace("PD-", ""));

    // Busca dados do pedido_compra
    const { data: pedidoCompra } = await supabase
      .from("pedidos_compra")
      .select("id, produto_id, produtos(nome), solicitante, nome_cliente, regiao")
      .eq("id", pedidoId)
      .single();

    if (!pedidoCompra) {
      return Response.json({ error: "Pedido não encontrado." }, { status: 404 });
    }

    // Busca o pedido_op para rastrear
    const { data: pedidoOp } = await supabase
      .from("pedidos_op")
      .select("id")
      .eq("codigo_pedido", codigo_pedido)
      .single();

    const pedidoOpId = pedidoOp?.id;

    if (!pedidoCompra?.produto_id) {
      return Response.json({ error: "Produto não encontrado." }, { status: 404 });
    }

    // Validar quantidade contra estoque
    const { data: produto } = await supabase
      .from("produtos")
      .select("quantidade")
      .eq("id", pedidoCompra.produto_id)
      .single();

    if (!produto || produto.quantidade < Number(quantidade)) {
      return Response.json({ error: "Quantidade insuficiente em estoque." }, { status: 400 });
    }

    // Registrar retirada
    if (pedidoOpId) {
      await supabase.from("expedicao_retiradas").insert({
        pedido_op_id: pedidoOpId,
        data: new Date().toISOString().slice(0, 10),
        hora: new Date().toTimeString().slice(0, 5),
        nome,
        quantidade: Number(quantidade),
        retirado_por: nome,
      });
    }

    // Atualizar estoque do produto
    await supabase
      .from("produtos")
      .update({ quantidade: produto.quantidade - Number(quantidade) })
      .eq("id", pedidoCompra.produto_id);

    // Atualizar status de rastreio para EXPEDIÇÃO (4) com timestamp
    const { data: tsAtual4 } = await supabase.from("pedidos_compra").select("rastreio_timestamps").eq("id", pedidoCompra.id).maybeSingle();
    const ts4 = { ...(tsAtual4?.rastreio_timestamps || {}), 4: new Date().toISOString() };
    const { error: eTs4 } = await supabase.from("pedidos_compra").update({ status_rastreio: 4, rastreio_timestamps: ts4 }).eq("id", pedidoCompra.id);
    if (eTs4) await supabase.from("pedidos_compra").update({ status_rastreio: 4 }).eq("id", pedidoCompra.id);

    // Enviar WhatsApp
    await enviarWhatsApp(
      `📦 *BAIXA DO ESTOQUE - PCP*\n` +
      `Pedido: #${pedidoCompra.id}\n` +
      `Produto: ${pedidoCompra.produtos?.nome}\n` +
      `Cliente: ${pedidoCompra.nome_cliente || "—"}\n` +
      `Região: ${pedidoCompra.regiao || "—"}\n` +
      `Quantidade: ${quantidade} un.\n` +
      `Baixado por: ${nome}\n` +
      `Status: 🚚 AGUARDANDO CARREGAMENTO`
    );

    return Response.json({ ok: true });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
