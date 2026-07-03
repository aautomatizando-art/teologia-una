// Envio de notificações para grupo de WhatsApp.
// Suporta dois provedores, escolhido por WHATSAPP_PROVIDER:
//  - "evolution": Evolution API (self-hosted ou cloud) — envia direto para grupo
//  - "callmebot": CallMeBot (gratuito, envia para o número cadastrado)
// Se nada estiver configurado, apenas registra no log e segue sem erro.

export async function enviarWhatsApp(mensagem) {
  const provider = (process.env.WHATSAPP_PROVIDER || "").toLowerCase();
  try {
    if (provider === "evolution") {
      const url = process.env.EVOLUTION_URL;
      const instance = process.env.EVOLUTION_INSTANCE;
      const apikey = process.env.EVOLUTION_APIKEY;
      const grupo = process.env.EVOLUTION_GROUP_JID;
      if (!url || !instance || !apikey || !grupo) {
        console.log("[whatsapp] Evolution API não configurada. Mensagem:", mensagem);
        return { enviado: false, motivo: "nao_configurado" };
      }
      const res = await fetch(`${url.replace(/\/$/, "")}/message/sendText/${instance}`, {
        method: "POST",
        headers: { "Content-Type": "application/json", apikey },
        body: JSON.stringify({ number: grupo, textMessage: { text: mensagem } }),
      });
      let body = null;
      try { body = await res.json(); } catch { body = await res.text().catch(() => null); }
      if (!res.ok) console.error("[whatsapp] Evolution API erro:", res.status, JSON.stringify(body));
      return { enviado: res.ok, status: res.status, body };
    }

    if (provider === "callmebot") {
      const phone = process.env.CALLMEBOT_PHONE;
      const apikey = process.env.CALLMEBOT_APIKEY;
      if (!phone || !apikey) {
        console.log("[whatsapp] CallMeBot não configurado. Mensagem:", mensagem);
        return { enviado: false, motivo: "nao_configurado" };
      }
      const u = `https://api.callmebot.com/whatsapp.php?phone=${encodeURIComponent(
        phone
      )}&apikey=${encodeURIComponent(apikey)}&text=${encodeURIComponent(mensagem)}`;
      const res = await fetch(u);
      return { enviado: res.ok, status: res.status };
    }

    console.log("[whatsapp] Provedor não definido. Mensagem:", mensagem);
    return { enviado: false, motivo: "nao_configurado" };
  } catch (e) {
    console.error("[whatsapp] Falha ao enviar:", e.message);
    return { enviado: false, motivo: e.message };
  }
}
