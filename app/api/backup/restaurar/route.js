import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

const BUCKET = "backups-producao";

// POST /api/backup/restaurar { arquivo }
// Restaura as QUANTIDADES DE ESTOQUE (e limites mín/máx) dos produtos a partir
// de um backup, casando pelo nome do produto. Não apaga histórico de reservas,
// pedidos ou produção — apenas devolve o estoque ao estado salvo.
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { arquivo } = await req.json();
  if (!arquivo || arquivo.includes("/") || arquivo.includes("..")) {
    return Response.json({ error: "Arquivo inválido." }, { status: 400 });
  }

  const { data: blob, error: eDown } = await supabase.storage.from(BUCKET).download(arquivo);
  if (eDown) return Response.json({ error: eDown.message }, { status: 500 });

  let dump;
  try {
    dump = JSON.parse(await blob.text());
  } catch {
    return Response.json({ error: "Backup corrompido (JSON inválido)." }, { status: 500 });
  }

  const produtos = dump?.tabelas?.produtos;
  if (!Array.isArray(produtos) || !produtos.length) {
    return Response.json({ error: "Backup não contém a tabela de produtos." }, { status: 400 });
  }

  let restaurados = 0;
  for (const p of produtos) {
    const { error } = await supabase
      .from("produtos")
      .update({ quantidade: p.quantidade, qtd_minima: p.qtd_minima, qtd_maxima: p.qtd_maxima })
      .eq("nome", p.nome);
    if (!error) restaurados++;
  }

  await enviarWhatsApp(
    `♻️ *ESTOQUE RESTAURADO DE BACKUP*\nArquivo: ${arquivo}\nProdutos restaurados: ${restaurados}\nBackup gerado em: ${dump.gerado_em?.slice(0, 16).replace("T", " ")}`
  );

  return Response.json({ ok: true, restaurados });
}
