import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

const OFFLINE_THRESHOLD_S  = 20;  // considera offline após 20 s sem ping
const DEBOUNCE_MIN         = 10;  // não notifica novamente em menos de 10 min

// POST /api/esp32/notify — chamado pelo dashboard ao detectar ESP32 offline
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { painel } = await req.json();
  if (!painel) return Response.json({ error: "Informe painel." }, { status: 400 });

  const { data } = await supabase
    .from("esp32_status")
    .select("ultimo_ping, notificado_em, ip")
    .eq("painel", Number(painel))
    .maybeSingle();

  if (!data) return Response.json({ enviado: false, motivo: "sem_registro" });

  const agora      = Date.now();
  const diffS      = (agora - new Date(data.ultimo_ping).getTime()) / 1000;
  const diffNotif  = data.notificado_em
    ? (agora - new Date(data.notificado_em).getTime()) / 60000
    : Infinity;

  // Só notifica se realmente offline e fora do debounce
  if (diffS < OFFLINE_THRESHOLD_S) {
    return Response.json({ enviado: false, motivo: "ainda_online" });
  }
  if (diffNotif < DEBOUNCE_MIN) {
    return Response.json({ enviado: false, motivo: "debounce" });
  }

  const resultado = await enviarWhatsApp(
    `⚠️ *BOTOEIRA DESCONECTADA*\n` +
    `Painel: ${painel}\n` +
    `IP anterior: ${data.ip || "desconhecido"}\n` +
    `Último contato: ${new Date(data.ultimo_ping).toLocaleString("pt-BR")}\n` +
    `Verifique a conexão do ESP32.`
  );

  if (resultado.enviado) {
    await supabase
      .from("esp32_status")
      .update({ notificado_em: new Date().toISOString() })
      .eq("painel", Number(painel));
  }

  return Response.json({ enviado: resultado.enviado, motivo: resultado.motivo });
}
