import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

const ETAPAS = [
  "GERADO ORDEM DE PRODUÇÃO",
  "PRODUZINDO",
  "ENTRADO NO ESTOQUE",
  "NA EXPEDIÇÃO",
  "EM ROTA PARA ENTREGA",
];

// PATCH /api/rastreio → avançar/definir etapa do pedido e notificar grupo
export async function PATCH(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { id, status } = await req.json();
  const novo = Number(status);
  if (!id || !(novo >= 1 && novo <= 5)) {
    return Response.json({ error: "Informe id e status entre 1 e 5." }, { status: 400 });
  }

  const { data: pedido, error } = await supabase
    .from("pedidos_compra")
    .update({ status_rastreio: novo })
    .eq("id", id)
    .select("id, criticidade, solicitante, produtos(nome)")
    .single();
  if (error) return Response.json({ error: error.message }, { status: 500 });

  const whatsapp = await enviarWhatsApp(
    `🚚 *RASTREIO PEDIDO #${pedido.id}*\n` +
      `Produto: ${pedido.produtos?.nome}\n` +
      `Novo status: *${ETAPAS[novo - 1]}* (${novo}/5)\n` +
      `Solicitante: ${pedido.solicitante}`
  );

  return Response.json({ ok: true, whatsapp });
}
