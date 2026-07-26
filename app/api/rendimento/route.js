import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

function proximoMes(mes) {
  const [y, m] = mes.split("-").map(Number);
  return m === 12 ? `${y + 1}-01-01` : `${y}-${String(m + 1).padStart(2, "0")}-01`;
}

// GET /api/rendimento           → últimos 90 dias
// GET /api/rendimento?data=     → dia específico
// GET /api/rendimento?mes=      → mês específico (YYYY-MM)
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const url = new URL(req.url);
  const data = url.searchParams.get("data");
  const mes = url.searchParams.get("mes");

  let q = supabase
    .from("rendimento_romaneio")
    .select("*")
    .order("data", { ascending: false })
    .order("criado_em", { ascending: false });

  if (data) {
    q = q.eq("data", data);
  } else if (mes) {
    q = q.gte("data", `${mes}-01`).lt("data", proximoMes(mes));
  } else {
    const d = new Date();
    d.setDate(d.getDate() - 90);
    q = q.gte("data", d.toISOString().split("T")[0]);
  }

  const { data: rows, error } = await q;
  if (error) return Response.json({ error: error.message }, { status: 500 });
  return Response.json({ registros: rows || [] });
}

// POST /api/rendimento → salvar romaneio
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const body = await req.json();
  const { data, fornecedor, regiao, tipo_batata, peso_liquido, qtd_sacos, qtd_sacos_produzidos, media_tirada } = body;

  if (!fornecedor) return Response.json({ error: "Informe o fornecedor." }, { status: 400 });

  const { error } = await supabase.from("rendimento_romaneio").insert({
    data: data || new Date().toISOString().split("T")[0],
    fornecedor: fornecedor.trim(),
    regiao: (regiao || "").trim(),
    tipo_batata: (tipo_batata || "").trim(),
    peso_liquido: Number(peso_liquido) || 0,
    qtd_sacos: Number(qtd_sacos) || 0,
    qtd_sacos_produzidos: Number(qtd_sacos_produzidos) || 0,
    media_tirada: Number(media_tirada) || 0,
  });

  if (error) return Response.json({ error: error.message }, { status: 500 });
  return Response.json({ ok: true });
}
