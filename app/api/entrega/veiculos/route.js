import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/entrega/veiculos — Lista veículos distintos
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  try {
    // Busca todos os carregamentos com veículo definido
    const { data: carregamentos } = await supabase
      .from("expedicao_carregamentos")
      .select("veiculo")
      .neq("veiculo", null)
      .order("veiculo");

    // Remove duplicatas
    const veiculosSet = new Set((carregamentos || []).map((c) => c.veiculo).filter(Boolean));
    const veiculos = Array.from(veiculosSet).sort();

    return Response.json({ veiculos });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
