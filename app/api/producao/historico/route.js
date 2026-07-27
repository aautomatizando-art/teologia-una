import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/producao/historico?dias=90 → kg de batata produzidos por dia (todos os painéis)
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const dias = Number(new URL(req.url).searchParams.get("dias")) || 90;
  const desde = new Date();
  desde.setDate(desde.getDate() - dias);

  const { data, error } = await supabase
    .from("producao_registros")
    .select("paletes, registrado_em, ordens_producao(produtos(caixas_por_palete, kg_batata_por_caixa))")
    .gte("registrado_em", desde.toISOString())
    .order("registrado_em", { ascending: true });

  if (error) return Response.json({ dias: [] });

  const map = {};
  for (const r of data || []) {
    const dia = r.registrado_em.split("T")[0];
    if (!map[dia]) map[dia] = 0;
    const cxPalete = r.ordens_producao?.produtos?.caixas_por_palete || 1;
    const kgPorCx  = r.ordens_producao?.produtos?.kg_batata_por_caixa || 0;
    map[dia] += (r.paletes || 0) * cxPalete * kgPorCx;
  }

  const resultado = Object.entries(map).map(([data, total_kg]) => ({
    data,
    total_kg: parseFloat(total_kg.toFixed(2)),
  }));

  return Response.json({ dias: resultado });
}
