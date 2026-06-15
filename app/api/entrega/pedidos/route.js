import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/entrega/pedidos?veiculo=Caminhão%201 — Pedidos de um veículo
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const veiculo = new URL(req.url).searchParams.get("veiculo");
  if (!veiculo) {
    return Response.json({ error: "veiculo é obrigatório" }, { status: 400 });
  }

  try {
    // Busca carregamentos com este veículo
    const { data: carregamentos } = await supabase
      .from("expedicao_carregamentos")
      .select("pedido_op_id, veiculo, nome_motorista, nome_ajudante");

    if (!carregamentos || carregamentos.length === 0) {
      return Response.json({ pedidos: [] });
    }

    // Filtra por veículo e apenas status 5 e 6 (Em Rota ou Entregue)
    const carregamentosVeiculo = carregamentos.filter((c) => c.veiculo === veiculo);

    // Para cada carregamento, busca informações do pedido
    const pedidos = await Promise.all(
      carregamentosVeiculo.map(async (c) => {
        // Busca o código do pedido via pedido_op
        const { data: pedidoOp } = await supabase
          .from("pedidos_op")
          .select("codigo_pedido")
          .eq("id", c.pedido_op_id)
          .single();

        // Extrai ID do pedido (PD-X → X)
        const pedidoId = pedidoOp ? Number(pedidoOp.codigo_pedido.replace("PD-", "")) : null;

        if (!pedidoId) return null;

        // Busca detalhes do pedido
        const { data: pedidoCompra } = await supabase
          .from("pedidos_compra")
          .select("id, quantidade, solicitante, criticidade, produtos(nome), nome_cliente, regiao, status_rastreio")
          .eq("id", pedidoId)
          .single();

        // Apenas retorna se status é 5 (Em Rota) ou 6 (Entregue)
        if (pedidoCompra?.status_rastreio === 5 || pedidoCompra?.status_rastreio === 6) {
          return {
            id: pedidoCompra?.id,
            quantidade: pedidoCompra?.quantidade,
            solicitante: pedidoCompra?.solicitante,
            criticidade: pedidoCompra?.criticidade,
            produtos: pedidoCompra?.produtos,
            nome_cliente: pedidoCompra?.nome_cliente,
            regiao: pedidoCompra?.regiao,
            veiculo: c.veiculo,
            nome_motorista: c.nome_motorista,
            nome_ajudante: c.nome_ajudante,
            status_rastreio: pedidoCompra?.status_rastreio,
          };
        }
        return null;
      })
    );

    return Response.json({ pedidos: pedidos.filter(Boolean) });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
