import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/debug?op=OP-1011 — Debug de uma OP específica
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const op = new URL(req.url).searchParams.get("op");
  if (!op) return Response.json({ error: "Informe ?op=OP-XXXX" }, { status: 400 });

  try {
    // Busca a OP
    const { data: ordem } = await supabase
      .from("ordens_producao")
      .select("id, numero, status, finalizado_em")
      .eq("numero", op)
      .single();

    if (!ordem) return Response.json({ error: "OP não encontrada" }, { status: 404 });

    // Busca os pedidos da OP
    const { data: pedidosOp } = await supabase
      .from("pedidos_op")
      .select("id, codigo_pedido, qtd_planejada, quantidade_produzida, entregue_ao_estoque")
      .eq("ordem_id", ordem.id);

    // Para cada pedido, busca informações do produto
    const detalhes = await Promise.all(
      (pedidosOp || []).map(async (p) => {
        const pedidoId = Number(p.codigo_pedido.replace("PD-", ""));
        const { data: pedidoCompra } = await supabase
          .from("pedidos_compra")
          .select("id, produto_id, produtos(nome)")
          .eq("id", pedidoId)
          .single();

        const { data: produto } = await supabase
          .from("produtos")
          .select("quantidade")
          .eq("id", pedidoCompra?.produto_id)
          .single();

        return {
          pedido_codigo: p.codigo_pedido,
          produto_nome: pedidoCompra?.produtos?.nome,
          produto_id: pedidoCompra?.produto_id,
          quantidade_planejada: p.qtd_planejada,
          quantidade_produzida: p.quantidade_produzida,
          estoque_atual: produto?.quantidade || 0,
          entregue_ao_estoque: p.entregue_ao_estoque,
          deveria_ter_aumentado: (p.quantidade_produzida || 0) >= p.qtd_planejada,
        };
      })
    );

    return Response.json({
      op: ordem.numero,
      status: ordem.status,
      finalizado_em: ordem.finalizado_em,
      pedidos: detalhes,
    });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
