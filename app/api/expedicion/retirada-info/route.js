import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/expedicion/retirada-info?codigo_pedido=PD-11 — Retorna data/hora da retirada
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

    // Busca a retirada mais recente
    const { data: retirada } = await supabase
      .from("expedicao_retiradas")
      .select("data, hora")
      .eq("pedido_op_id", pedidoOp.id)
      .order("criado_em", { ascending: false })
      .limit(1)
      .single();

    return Response.json({ data: retirada?.data, hora: retirada?.hora });
  } catch (error) {
    return Response.json({ data: null, hora: null });
  }
}
