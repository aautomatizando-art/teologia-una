import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/expedicion/carregamento-info?codigo_pedido=PC-9 — Retorna data/hora do carregamento
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const codigo_pedido = new URL(req.url).searchParams.get("codigo_pedido");
  if (!codigo_pedido) {
    return Response.json({ error: "codigo_pedido é obrigatório." }, { status: 400 });
  }

  try {
    // Busca o pedido_op para obter ID
    const { data: pedidoOp } = await supabase
      .from("pedidos_op")
      .select("id")
      .eq("codigo_pedido", codigo_pedido)
      .single();

    if (!pedidoOp?.id) {
      return Response.json({ data: null, hora: null });
    }

    // Busca o carregamento mais recente
    const { data: carregamento } = await supabase
      .from("expedicao_carregamentos")
      .select("criado_em")
      .eq("pedido_op_id", pedidoOp.id)
      .order("criado_em", { ascending: false })
      .limit(1)
      .single();

    if (!carregamento?.criado_em) {
      return Response.json({ data: null, hora: null });
    }

    const dataParts = carregamento.criado_em.split("T");
    const data = dataParts[0];
    const hora = dataParts[1]?.slice(0, 5) || null;

    return Response.json({ data, hora });
  } catch (error) {
    return Response.json({ data: null, hora: null });
  }
}
