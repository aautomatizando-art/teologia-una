import { enviarWhatsApp } from "@/lib/whatsapp";

// GET /api/debug/whatsapp — verifica config e envia mensagem de teste
// Protegido por query param ?key=debugfrit
export async function GET(req) {
  const key = new URL(req.url).searchParams.get("key");
  if (key !== "debugfrit") {
    return Response.json({ error: "Não autorizado" }, { status: 401 });
  }

  const provider   = process.env.WHATSAPP_PROVIDER || "(não definido)";
  const evUrl      = process.env.EVOLUTION_URL      ? "✅ definido" : "❌ ausente";
  const evInstance = process.env.EVOLUTION_INSTANCE ? "✅ definido" : "❌ ausente";
  const evApikey   = process.env.EVOLUTION_APIKEY   ? "✅ definido" : "❌ ausente";
  const evGroup    = process.env.EVOLUTION_GROUP_JID ? `✅ ${process.env.EVOLUTION_GROUP_JID}` : "❌ ausente";
  const cbPhone    = process.env.CALLMEBOT_PHONE    ? "✅ definido" : "❌ ausente";
  const cbApikey   = process.env.CALLMEBOT_APIKEY   ? "✅ definido" : "❌ ausente";

  const config = { provider, evUrl, evInstance, evApikey, evGroup, cbPhone, cbApikey };

  // Verifica status de conexão da instância Evolution
  let instanciaStatus = null;
  if (provider === "evolution") {
    const url      = process.env.EVOLUTION_URL;
    const instance = process.env.EVOLUTION_INSTANCE;
    const apikey   = process.env.EVOLUTION_APIKEY;
    if (url && instance && apikey) {
      try {
        const r = await fetch(
          `${url.replace(/\/$/, "")}/instance/connectionState/${instance}`,
          { headers: { apikey }, signal: AbortSignal.timeout(5000) }
        );
        const j = await r.json();
        instanciaStatus = { httpStatus: r.status, ...j };
      } catch (e) {
        instanciaStatus = { erro: e.message };
      }
    }
  }

  const send = new URL(req.url).searchParams.get("send");
  let resultado = null;
  if (send === "1") {
    resultado = await enviarWhatsApp("Teste de diagnostico - dashboard producao");
  }

  return Response.json({ config, instanciaStatus, resultado, instrucao: "Adicione &send=1 para enviar mensagem de teste" });
}
