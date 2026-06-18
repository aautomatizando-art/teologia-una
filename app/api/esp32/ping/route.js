import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

const TIMEOUT_CONECTADO_S = 12; // considera desconectado após 12 s sem ping

// POST /api/esp32/ping — ESP32 anuncia presença
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { painel, ip } = await req.json();
  if (!painel) return Response.json({ error: "Informe painel." }, { status: 400 });

  const { error } = await supabase
    .from("esp32_status")
    .upsert({ painel: Number(painel), ultimo_ping: new Date().toISOString(), ip: ip || null },
             { onConflict: "painel" });

  if (error) return Response.json({ error: error.message }, { status: 500 });
  return Response.json({ ok: true });
}

// GET /api/esp32/ping?painel=1 — dashboard consulta status
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const url = new URL(req.url);
  const painel = url.searchParams.get("painel");

  // Sem painel: retorna todos
  const query = supabase.from("esp32_status").select("painel, ultimo_ping, ip");
  if (painel) query.eq("painel", Number(painel));

  const { data, error } = await query;
  if (error) return Response.json({ dispositivos: [] }); // tabela pode não existir ainda

  const agora = Date.now();
  const dispositivos = (data || []).map((d) => {
    const diffS = (agora - new Date(d.ultimo_ping).getTime()) / 1000;
    return { painel: d.painel, conectado: diffS <= TIMEOUT_CONECTADO_S, ultimo_ping: d.ultimo_ping, ip: d.ip };
  });

  return Response.json({ dispositivos });
}
