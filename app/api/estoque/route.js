import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

function statusEstoque(p) {
  if (p.quantidade < p.qtd_minima) return "ESTOQUE BAIXO";
  if (p.quantidade > p.qtd_maxima) return "ESTOQUE ALTO";
  return "NORMAL";
}

// GET /api/estoque → todos os produtos com status calculado
export async function GET() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { data, error } = await supabase
    .from("produtos")
    .select("id, nome, categoria, quantidade, qtd_minima, qtd_maxima")
    .order("nome");

  if (error) return Response.json({ error: error.message }, { status: 500 });

  const produtos = (data || []).map((p) => ({ ...p, status: statusEstoque(p) }));
  const total = produtos.reduce((s, p) => s + p.quantidade, 0);
  const baixos = produtos.filter((p) => p.status === "ESTOQUE BAIXO").length;
  const altos = produtos.filter((p) => p.status === "ESTOQUE ALTO").length;

  return Response.json({ total, baixos, altos, produtos });
}
