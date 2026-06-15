import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/expedicion/listar?status=3 ou 4 — Lista pedidos para expedição
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const status = new URL(req.url).searchParams.get("status") || "3";

  try {
    // Busca pedidos_compra com status específico
    const { data: pedidos, error } = await supabase
      .from("pedidos_compra")
      .select("id, quantidade, criticidade, solicitante, produtos(nome), status_rastreio, nome_cliente, regiao")
      .eq("status_rastreio", Number(status))
      .order("criado_em", { ascending: false });

    if (error) throw error;

    // Para cada pedido, busca informações de localização
    const pedidosComLocalizacao = await Promise.all(
      (pedidos || []).map(async (p) => {
        // Busca pedido_op via codigo_pedido
        const { data: pedidosOp, error: ePedidoOp } = await supabase
          .from("pedidos_op")
          .select("id, quantidade_produzida, localizacao_id")
          .eq("codigo_pedido", `PD-${p.id}`);

        const pedidoOp = pedidosOp?.[0]; // Pega o primeiro se houver múltiplos

        let localizacao = null;
        if (pedidoOp?.localizacao_id) {
          const { data: loc } = await supabase
            .from("localizacao_estoque")
            .select("numero_lote, rua, prateleira")
            .eq("id", pedidoOp.localizacao_id)
            .single();
          localizacao = loc;
        }

        return {
          ...p,
          pedido_op_id: pedidoOp?.id || null,
          produzido: pedidoOp?.quantidade_produzida || 0,
          localizacao,
        };
      })
    );

    return Response.json({ pedidos: pedidosComLocalizacao });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
