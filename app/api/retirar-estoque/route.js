import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";
import { SENHA_PEDIDOS } from "@/lib/senhas";

// POST /api/retirar-estoque — retirar pedidos direto do estoque (sem produção)
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { pedido_ids, solicitante, data, hora, senha } = await req.json();
  if (senha !== SENHA_PEDIDOS) {
    return Response.json({ error: "Senha de pedidos inválida." }, { status: 403 });
  }
  if (!Array.isArray(pedido_ids) || pedido_ids.length === 0) {
    return Response.json({ error: "Informe ao menos um pedido." }, { status: 400 });
  }

  try {
    let retirados = 0;
    const detalhes = [];

    for (const pedidoId of pedido_ids) {
      const { data: pedido, error: eGet } = await supabase
        .from("pedidos_compra")
        .select("id, produto_id, quantidade, produtos(nome), solicitante, criticidade")
        .eq("id", Number(pedidoId))
        .single();

      if (!pedido || eGet) continue;

      // Busca estoque atual
      const { data: produto } = await supabase
        .from("produtos")
        .select("quantidade")
        .eq("id", pedido.produto_id)
        .single();

      if (!produto) continue;

      const novoEstoque = Math.max(0, (produto.quantidade || 0) - (pedido.quantidade || 0));

      // Atualiza estoque
      await supabase
        .from("produtos")
        .update({ quantidade: novoEstoque })
        .eq("id", pedido.produto_id);

      // Atualiza status para ESTOQUE (3) com timestamp
      const { data: tsAtualRe } = await supabase.from("pedidos_compra").select("rastreio_timestamps").eq("id", Number(pedidoId)).maybeSingle();
      const tsRe = { ...(tsAtualRe?.rastreio_timestamps || {}), 3: new Date().toISOString() };
      const { error: eTsRe } = await supabase.from("pedidos_compra").update({ status_rastreio: 3, rastreio_timestamps: tsRe }).eq("id", Number(pedidoId));
      if (eTsRe) await supabase.from("pedidos_compra").update({ status_rastreio: 3 }).eq("id", Number(pedidoId));

      retirados++;
      detalhes.push(`${pedido.produtos?.nome} (${pedido.quantidade} un.)`);
    }

    // Envia WhatsApp
    if (retirados > 0) {
      const emoji = { EMERGENCIAL: "🟣", URGENTE: "🔴", MODERADO: "🟢" }["MODERADO"];
      await enviarWhatsApp(
        `📦 *RETIRADA DO ESTOQUE*\n` +
        `Solicitante: ${solicitante}\n` +
        `Pedidos retirados: ${retirados}\n` +
        `Produtos:\n${detalhes.map((d) => `• ${d}`).join("\n")}\n` +
        `Data: ${data || new Date().toISOString().slice(0, 10)} ${hora || new Date().toTimeString().slice(0, 5)}`
      );
    }

    return Response.json({ ok: true, retirados });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
