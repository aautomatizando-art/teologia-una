import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/recalcular-estoque → soma ao estoque os pedidos finalizados não entregues
export async function GET() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  try {
    // Busca todos os pedidos finalizados que ainda não foram entregues ao estoque
    const { data: pedidosNaoEntregues } = await supabase
      .from("pedidos_op")
      .select("id, codigo_pedido, quantidade_produzida, entregue_ao_estoque")
      .eq("entregue_ao_estoque", false)
      .gte("quantidade_produzida", 1);

    let somados = 0;

    for (const pedido of pedidosNaoEntregues || []) {
      try {
        // Busca o produto via código do pedido (PC-{id})
        const pedidoId = Number(pedido.codigo_pedido.replace("PC-", ""));
        const { data: pedidoCompra } = await supabase
          .from("pedidos_compra")
          .select("produto_id")
          .eq("id", pedidoId)
          .single();

        if (pedidoCompra?.produto_id) {
          // Busca quantidade atual do produto
          const { data: produto } = await supabase
            .from("produtos")
            .select("quantidade")
            .eq("id", pedidoCompra.produto_id)
            .single();

          // Aumenta o estoque do produto
          if (produto) {
            await supabase
              .from("produtos")
              .update({ quantidade: (produto.quantidade || 0) + pedido.quantidade_produzida })
              .eq("id", pedidoCompra.produto_id);

            // Marca como entregue ao estoque
            await supabase
              .from("pedidos_op")
              .update({ entregue_ao_estoque: true })
              .eq("id", pedido.id);

            somados++;
          }
        }
      } catch (e) {
        console.error(`Erro ao processar pedido ${pedido.id}:`, e.message);
      }
    }

    return Response.json({ ok: true, somados, total: (pedidosNaoEntregues || []).length });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
