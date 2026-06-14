import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

const BUCKET = "backups-producao";

// GET /api/backup/download?arquivo=... → baixa o arquivo JSON do backup
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const arquivo = new URL(req.url).searchParams.get("arquivo");
  if (!arquivo || arquivo.includes("/") || arquivo.includes("..")) {
    return Response.json({ error: "Arquivo inválido." }, { status: 400 });
  }

  const { data, error } = await supabase.storage.from(BUCKET).download(arquivo);
  if (error) return Response.json({ error: error.message }, { status: 500 });

  return new Response(data, {
    headers: {
      "Content-Type": "application/json",
      "Content-Disposition": `attachment; filename="${arquivo}"`,
    },
  });
}
