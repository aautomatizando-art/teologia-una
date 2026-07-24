import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// GET /api/producao/status — retorna contagem de registros (polling leve)
// O cliente usa isso para saber se precisa buscar os dados completos.
export async function GET() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { count } = await supabase
    .from("producao_registros")
    .select("*", { count: "exact", head: true });

  return Response.json({ total: count ?? 0 });
}
