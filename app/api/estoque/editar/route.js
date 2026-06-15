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
    quantidade,
    rua,
    prateleira,
    lote,
  } = await req.json();

  if (!produto_id || !nome_produto || !quantidade_maxima || !quantidade_minima || !caixas_por_palete) {
    return Response.json({ error: "Todos os campos são obrigatórios" }, { status: 400 });
  }

  try {
    const campos = {
      nome: nome_produto,
      qtd_maxima: quantidade_maxima,
      qtd_minima: quantidade_minima,
      caixas_por_palete,
      rua: rua || null,
      prateleira: prateleira || null,
      lote: lote || null,
    };
    if (quantidade !== undefined && quantidade !== null) campos.quantidade = quantidade;

    // Atualizar produto
    let { error: eUpdate } = await supabase.from("produtos").update(campos).eq("id", produto_id);

    // Colunas de localização podem não existir ainda (migration pendente)
    if (eUpdate?.code === "PGRST204" || /rua|prateleira|lote/.test(eUpdate?.message || "")) {
      const { rua: _rua, prateleira: _prateleira, lote: _lote, ...semLocalizacao } = campos;
      ({ error: eUpdate } = await supabase.from("produtos").update(semLocalizacao).eq("id", produto_id));
    }

    // Coluna caixas_por_palete pode não existir ainda (migration pendente)
    if (eUpdate?.code === "PGRST204" || eUpdate?.message?.includes("caixas_por_palete")) {
      const { caixas_por_palete: _cpp, rua: _rua2, prateleira: _prateleira2, lote: _lote2, ...semExtras } = campos;
      ({ error: eUpdate } = await supabase.from("produtos").update(semExtras).eq("id", produto_id));
    }

    if (eUpdate) {
      return Response.json({ error: eUpdate.message }, { status: 500 });
    }

    return Response.json({ ok: true });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
