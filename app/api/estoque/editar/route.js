import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// POST /api/estoque/editar — Editar configurações de produto
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const {
    produto_id,
    nome_produto,
    quantidade_maxima,
    quantidade_minima,
    caixas_por_palete,
  } = await req.json();

  if (!produto_id || !nome_produto || !quantidade_maxima || !quantidade_minima || !caixas_por_palete) {
    return Response.json({ error: "Todos os campos são obrigatórios" }, { status: 400 });
  }

  try {
    // Atualizar produto
    let { error: eUpdate } = await supabase
      .from("produtos")
      .update({
        nome: nome_produto,
        qtd_maxima: quantidade_maxima,
        qtd_minima: quantidade_minima,
        caixas_por_palete: caixas_por_palete,
      })
      .eq("id", produto_id);

    // Coluna caixas_por_palete pode não existir ainda (migration pendente)
    if (eUpdate?.code === "PGRST204" || eUpdate?.message?.includes("caixas_por_palete")) {
      ({ error: eUpdate } = await supabase
        .from("produtos")
        .update({
          nome: nome_produto,
          qtd_maxima: quantidade_maxima,
          qtd_minima: quantidade_minima,
        })
        .eq("id", produto_id));
    }

    if (eUpdate) {
      return Response.json({ error: eUpdate.message }, { status: 500 });
    }

    return Response.json({ ok: true });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
