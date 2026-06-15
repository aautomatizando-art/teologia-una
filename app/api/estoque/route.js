import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

const COLS_BASE = "id, nome, categoria, quantidade, qtd_minima, qtd_maxima";
const COLS_LOC = "rua, prateleira, lote";
const COLS_INSUMOS = "kg_batata_por_caixa, caixa_por_caixa, kg_filme_bopp_por_caixa, kg_condimento_por_caixa, kg_oleo_por_caixa, cm_fita_adesiva_por_caixa";
const COLS_PALETE = "caixas_por_palete";
const REGEX_LOC = /rua|prateleira|lote/;
const REGEX_INSUMOS = /kg_batata_por_caixa|caixa_por_caixa|kg_filme_bopp_por_caixa|kg_condimento_por_caixa|kg_oleo_por_caixa|cm_fita_adesiva_por_caixa/;
const REGEX_PALETE = /caixas_por_palete/;

function statusEstoque(p) {
  if (p.quantidade < p.qtd_minima) return "ESTOQUE BAIXO";
  if (p.quantidade > p.qtd_maxima) return "ESTOQUE ALTO";
  return "NORMAL";
}

// Busca produtos lidando com colunas de localização/insumos que podem não existir ainda (migrations pendentes)
async function buscarProdutos(supabase) {
  let incluirLoc = true;
  let incluirInsumos = true;
  let incluirPalete = true;

  for (let tentativa = 0; tentativa < 4; tentativa++) {
    const cols = [COLS_BASE, incluirLoc && COLS_LOC, incluirInsumos && COLS_INSUMOS, incluirPalete && COLS_PALETE].filter(Boolean).join(", ");
    const { data, error } = await supabase.from("produtos").select(cols).order("nome");
    if (!error) return { data, error: null };

    const msg = error.message || "";
    let mudou = false;
    if (incluirInsumos && (error.code === "PGRST204" || REGEX_INSUMOS.test(msg))) { incluirInsumos = false; mudou = true; }
    if (incluirLoc && REGEX_LOC.test(msg)) { incluirLoc = false; mudou = true; }
    if (incluirPalete && (error.code === "PGRST204" || REGEX_PALETE.test(msg))) { incluirPalete = false; mudou = true; }
    if (!mudou) return { data: null, error };
  }
  return { data: null, error: { message: "Erro ao buscar produtos" } };
}

// GET /api/estoque → todos os produtos com status calculado
export async function GET() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { data, error } = await buscarProdutos(supabase);
  if (error) return Response.json({ error: error.message }, { status: 500 });

  const produtos = (data || []).map((p) => ({ ...p, status: statusEstoque(p) }));
  const total = produtos.reduce((s, p) => s + p.quantidade, 0);
  const baixos = produtos.filter((p) => p.status === "ESTOQUE BAIXO").length;
  const altos = produtos.filter((p) => p.status === "ESTOQUE ALTO").length;

  return Response.json({ total, baixos, altos, produtos });
}
