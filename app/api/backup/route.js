import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

const BUCKET = "backups-producao";
const TABELAS = [
  "produtos",
  "ordens_producao",
  "pedidos_op",
  "producao_registros",
  "perdas_embalagem",
  "problemas_qualidade",
  "reservas",
  "pedidos_compra",
  "app_users",
];

// GET /api/backup → lista os backups existentes
export async function GET() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { data, error } = await supabase.storage.from(BUCKET).list("", {
    limit: 100,
    sortBy: { column: "created_at", order: "desc" },
  });
  if (error) return Response.json({ error: error.message }, { status: 500 });

  const backups = (data || [])
    .filter((f) => f.name.endsWith(".json"))
    .map((f) => ({
      arquivo: f.name,
      criado_em: f.created_at,
      tamanho_kb: Math.round((f.metadata?.size || 0) / 102.4) / 10,
    }));
  return Response.json({ backups });
}

// POST /api/backup → gera um novo backup completo (todas as tabelas) no Storage
export async function POST() {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const dump = { gerado_em: new Date().toISOString(), sistema: "dashboard-producao", tabelas: {} };
  let total = 0;

  for (const t of TABELAS) {
    const { data, error } = await supabase.from(t).select("*").order("id");
    if (error) return Response.json({ error: `Tabela ${t}: ${error.message}` }, { status: 500 });
    dump.tabelas[t] = data || [];
    total += dump.tabelas[t].length;
  }
  dump.total_registros = total;

  const agora = new Date();
  const nome = `backup-${agora.toISOString().slice(0, 10)}-${String(agora.getHours()).padStart(2, "0")}h${String(agora.getMinutes()).padStart(2, "0")}.json`;

  const { error: eUp } = await supabase.storage
    .from(BUCKET)
    .upload(nome, JSON.stringify(dump, null, 2), { contentType: "application/json", upsert: true });
  if (eUp) return Response.json({ error: eUp.message }, { status: 500 });

  return Response.json({ ok: true, arquivo: nome, total_registros: total });
}

// DELETE /api/backup?arquivo=... → exclui um backup
export async function DELETE(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const arquivo = new URL(req.url).searchParams.get("arquivo");
  if (!arquivo || arquivo.includes("/") || arquivo.includes("..")) {
    return Response.json({ error: "Arquivo inválido." }, { status: 400 });
  }
  const { error } = await supabase.storage.from(BUCKET).remove([arquivo]);
  if (error) return Response.json({ error: error.message }, { status: 500 });
  return Response.json({ ok: true });
}
