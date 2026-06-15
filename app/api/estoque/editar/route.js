import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

const GRUPOS_OPCIONAIS = [
  {
    regex: /kg_batata_por_caixa|caixa_por_caixa|kg_filme_bopp_por_caixa|kg_condimento_por_caixa|kg_oleo_por_caixa|cm_fita_adesiva_por_caixa/,
    chaves: ["kg_batata_por_caixa", "caixa_por_caixa", "kg_filme_bopp_por_caixa", "kg_condimento_por_caixa", "kg_oleo_por_caixa", "cm_fita_adesiva_por_caixa"],
  },
  { regex: /rua|prateleira|lote/, chaves: ["rua", "prateleira", "lote"] },
  { regex: /caixas_por_palete/, chaves: ["caixas_por_palete"] },
];

function numeroOuNulo(v) {
  return v === undefined || v === null || v === "" ? null : Number(v);
}

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
    kg_batata_por_caixa,
    caixa_por_caixa,
    kg_filme_bopp_por_caixa,
    kg_condimento_por_caixa,
    kg_oleo_por_caixa,
    cm_fita_adesiva_por_caixa,
  } = await req.json();

  if (!produto_id || !nome_produto || !quantidade_maxima || !quantidade_minima || !caixas_por_palete) {
    return Response.json({ error: "Todos os campos são obrigatórios" }, { status: 400 });
  }

  try {
    let campos = {
      nome: nome_produto,
      qtd_maxima: quantidade_maxima,
      qtd_minima: quantidade_minima,
      caixas_por_palete,
      rua: rua || null,
      prateleira: prateleira || null,
      lote: lote || null,
      kg_batata_por_caixa: numeroOuNulo(kg_batata_por_caixa),
      caixa_por_caixa: numeroOuNulo(caixa_por_caixa),
      kg_filme_bopp_por_caixa: numeroOuNulo(kg_filme_bopp_por_caixa),
      kg_condimento_por_caixa: numeroOuNulo(kg_condimento_por_caixa),
      kg_oleo_por_caixa: numeroOuNulo(kg_oleo_por_caixa),
      cm_fita_adesiva_por_caixa: numeroOuNulo(cm_fita_adesiva_por_caixa),
    };
    if (quantidade !== undefined && quantidade !== null) campos.quantidade = quantidade;

    // Tenta atualizar, removendo grupos de colunas que ainda não existem (migrations pendentes)
    let eUpdate;
    for (let tentativa = 0; tentativa <= GRUPOS_OPCIONAIS.length; tentativa++) {
      ({ error: eUpdate } = await supabase.from("produtos").update(campos).eq("id", produto_id));
      if (!eUpdate) break;

      const msg = eUpdate.message || "";
      const grupo = GRUPOS_OPCIONAIS.find((g) => g.chaves.some((c) => c in campos) && (eUpdate.code === "PGRST204" || g.regex.test(msg)));
      if (!grupo) break;

      campos = { ...campos };
      for (const c of grupo.chaves) delete campos[c];
    }

    if (eUpdate) {
      return Response.json({ error: eUpdate.message }, { status: 500 });
    }

    return Response.json({ ok: true });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
