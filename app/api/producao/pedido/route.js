import { createHash } from "crypto";
import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

// Senha de produção (SHA-256) — padrão: "producao123"
const SENHA_HASH = "247c5a87c4292ac36590492c216e8f565e56b85584892b89fb45dd3a9b6fd2ab";

// GET /api/producao/pedido?ordem_id=<id> → lista pedidos da OP com progresso
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const ordemId = new URL(req.url).searchParams.get("ordem_id");
  if (!ordemId) return Response.json({ error: "Informe ordem_id." }, { status: 400 });

  const { data, error } = await supabase
    .from("pedidos_op")
    .select("id, codigo_pedido, qtd_planejada, quantidade_produzida")
    .eq("ordem_id", Number(ordemId));
  if (error) return Response.json({ error: error.message }, { status: 500 });

  const pedidos = (data || []).map((p) => ({
    id: p.id,
    codigo: p.codigo_pedido,
    qtd_planejada: p.qtd_planejada,
    produzido: p.quantidade_produzida,
    percentual: p.qtd_planejada ? Math.min(100, Math.round((p.quantidade_produzida / p.qtd_planejada) * 100)) : 0,
    status: p.quantidade_produzida >= p.qtd_planejada ? "FINALIZADO" : "PRODUZINDO",
  }));

  return Response.json({ pedidos });
}

// POST /api/producao/pedido — registra caixas produzidas de um pedido (requer senha)
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { pedido_id, quantidade, senha, numero_lote, rua, prateleira } = await req.json();
  if (!pedido_id || !quantidade || !senha) {
    return Response.json({ error: "Informe pedido_id, quantidade e senha." }, { status: 400 });
  }
  if (!numero_lote || !rua || !prateleira) {
    return Response.json({ error: "Informe numero_lote, rua e prateleira." }, { status: 400 });
  }

  const hash = createHash("sha256").update(String(senha)).digest("hex");
  if (hash !== SENHA_HASH) {
    return Response.json({ error: "Senha de produção inválida." }, { status: 403 });
  }

  const { data: pedido, error: eGet } = await supabase
    .from("pedidos_op")
    .select("id, codigo_pedido, quantidade_produzida, qtd_planejada, entregue_ao_estoque")
    .eq("id", Number(pedido_id))
    .single();
  if (eGet) return Response.json({ error: eGet.message }, { status: 500 });
  if (!pedido) return Response.json({ error: "Pedido não encontrado." }, { status: 404 });

  const novaQtd = (pedido.quantidade_produzida || 0) + Number(quantidade);
  if (novaQtd > pedido.qtd_planejada) {
    return Response.json(
      { error: `Excede a quantidade planejada (${pedido.qtd_planejada}). Já produzido: ${pedido.quantidade_produzida}.` },
      { status: 400 }
    );
  }

  const { error: eUpd } = await supabase
    .from("pedidos_op")
    .update({ quantidade_produzida: novaQtd })
    .eq("id", Number(pedido_id));
  if (eUpd) return Response.json({ error: eUpd.message }, { status: 500 });

  const finalizado = novaQtd >= pedido.qtd_planejada;
  const status = finalizado ? "✅ FINALIZADO" : "🔄 PRODUZINDO";

  // Se foi finalizado e ainda não foi entregue ao estoque, adiciona ao estoque
  if (finalizado && !pedido.entregue_ao_estoque) {
    // Busca o produto via código do pedido (PC-{id})
    const pedidoId = Number(pedido.codigo_pedido.replace("PC-", ""));
    const { data: pedidoCompra } = await supabase
      .from("pedidos_compra")
      .select("id, produto_id, produtos(nome), solicitante, quantidade")
      .eq("id", pedidoId)
      .single();

    if (pedidoCompra?.produto_id) {
      // Salva localização do pedido
      const { data: localizacao, error: eLocalizacao } = await supabase
        .from("localizacao_estoque")
        .insert({
          pedido_op_id: Number(pedido_id),
          numero_lote,
          rua,
          prateleira,
        })
        .select("id")
        .single();

      // Atualiza pedidos_op com ID da localização
      if (localizacao?.id) {
        await supabase
          .from("pedidos_op")
          .update({ localizacao_id: localizacao.id })
          .eq("id", Number(pedido_id));
      }

      // Busca quantidade atual do produto
      const { data: produto } = await supabase
        .from("produtos")
        .select("quantidade")
        .eq("id", pedidoCompra.produto_id)
        .single();

      // Aumenta o estoque do produto
      if (produto) {
        await supabase
          .from("produtos")
          .update({ quantidade: (produto.quantidade || 0) + novaQtd })
          .eq("id", pedidoCompra.produto_id);
      }

      // Marca como entregue ao estoque
      await supabase
        .from("pedidos_op")
        .update({ entregue_ao_estoque: true })
        .eq("id", Number(pedido_id));

      // Atualiza status de rastreio para ESTOQUE (3)
      await supabase
        .from("pedidos_compra")
        .update({ status_rastreio: 3 })
        .eq("id", pedidoCompra.id);

      // Envia notificação WhatsApp
      const nomeProduto = pedidoCompra.produtos?.nome || "Produto";
      await enviarWhatsApp(
        `✅ *PEDIDO #${pedidoCompra.id} FINALIZADO*\n` +
        `Produto: ${nomeProduto}\n` +
        `Quantidade: ${novaQtd} un.\n` +
        `Localização: Lote ${numero_lote} | Rua ${rua} | Prat. ${prateleira}\n` +
        `Solicitante: ${pedidoCompra.solicitante}\n` +
        `Status: 📦 ENTRADO NO ESTOQUE`
      );
    }
  }

  // Verificar se a OP inteira foi finalizada (todos os pedidos em 100%)
  const { data: pedidoFull, error: ePedidoFull } = await supabase
    .from("pedidos_op")
    .select("ordem_id")
    .eq("id", Number(pedido_id))
    .single();

  if (pedidoFull?.ordem_id) {
    const { data: todosPedidos } = await supabase
      .from("pedidos_op")
      .select("quantidade_produzida, qtd_planejada")
      .eq("ordem_id", pedidoFull.ordem_id);

    const todosFinalizados = (todosPedidos || []).every((p) => (p.quantidade_produzida || 0) >= p.qtd_planejada);
    if (todosFinalizados) {
      await supabase
        .from("ordens_producao")
        .update({ finalizado_em: new Date().toISOString(), status: "CONCLUIDA" })
        .eq("id", pedidoFull.ordem_id)
        .is("finalizado_em", null);
    }
  }

  return Response.json({ ok: true, novaQtd, status });
}
