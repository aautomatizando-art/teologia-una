import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/producao/hoje → total caixas e kg de batata produzidos hoje em todos os painéis
export async function GET() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const agora = new Date();
  const inicio = new Date(agora);
  inicio.setHours(0, 0, 0, 0);
  const fim = new Date(agora);
  fim.setHours(23, 59, 59, 999);

  const { data, error } = await supabase
    .from("producao_registros")
    .select("paletes, ordens_producao(produto_id, produtos(caixas_por_palete, kg_batata_por_caixa))")
    .gte("registrado_em", inicio.toISOString())
    .lte("registrado_em", fim.toISOString());

  if (error) return Response.json({ total_caixas: 0, total_kg_batata: 0 });

  let total_caixas = 0;
  let total_kg_batata = 0;

  for (const r of data || []) {
    const cxPalete = r.ordens_producao?.produtos?.caixas_por_palete || 1;
    const kgPorCx  = r.ordens_producao?.produtos?.kg_batata_por_caixa || 0;
    const caixas   = (r.paletes || 0) * cxPalete;
    total_caixas    += caixas;
    total_kg_batata += caixas * kgPorCx;
  }

  return Response.json({ total_caixas, total_kg_batata });
}
