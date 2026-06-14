import { enviarWhatsApp } from "@/lib/whatsapp";

// POST /api/whatsapp-teste → envia mensagem de teste para o grupo configurado
export async function POST() {
  const r = await enviarWhatsApp("✅ Teste de notificação — Dashboard Produção & Estoque funcionando!");
  return Response.json(r);
}
