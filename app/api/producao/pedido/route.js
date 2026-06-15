import { createHash } from "crypto";
import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { enviarWhatsApp } from "@/lib/whatsapp";

// Senha de produção (SHA-256) — padrão: "producao123"
const SENHA_HASH = "247c5a87c4292ac36590492c216e8f565e56b85584892b89fb45dd3a9b6fd2ab";

const COLS_INSUMOS = "kg_batata_por_caixa, caixa_por_caixa, kg_filme_bopp_por_caixa, kg_condimento_por_caixa, kg_oleo_por_caixa, cm_fita_adesiva_por_caixa";
const INSUMOS_VAZIOS = {
  kg_batata_por_caixa: null,
  caixa_por_caixa: null,
  kg_filme_bopp_por_caixa: null,
  kg_condimento_por_caixa: null,
  kg_oleo_por_caixa: null,
  cm_fita_adesiva_por_caixa: null,
};

// Busca produto incluindo insumos; cai para sem insumos se a migration ainda não foi aplicada
async function buscarProdutoComInsumos(supabase, produtoId) {
  const { data, error } = await supabase
    .from("produtos")
    .select(`quantidade, ${COLS_INSUMOS}`)
    .eq("id", produtoId)
    .single();

  if (!error) return data;

  const { data: dataBasica } = await supabase
    .from("produtos")
    .select("quantidade")
    .eq("id", produtoId)
    .single();

  return dataBasica ? { ...dataBasica, ...INSUMOS_VAZIOS } : null;
}

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

  // Para cada pedido, busca o estoque disponível e os insumos do produto
  const pedidosComEstoque = await Promise.all(
    (data || []).map(async (p) => {
      // Busca o produto_id via código_pedido
      const pedidoId = Number(p.codigo_pedido.replace("PD-", ""));
      const { data: pedidoCompra } = await supabase
        .from("pedidos_compra")
        .select("produto_id")
        .eq("id", pedidoId)
        .single();

      let estoque = 0;
      let insumos = { ...INSUMOS_VAZIOS };
      if (pedidoCompra?.produto_id) {
        const produto = await buscarProdutoComInsumos(supabase, pedidoCompra.produto_id);
        estoque = produto?.quantidade || 0;
        if (produto) {
          const { quantidade, ...resto } = produto;
          insumos = resto;
        }
      }

      const produzido = p.quantidade_produzida || 0;
      const totalDisponivel = produzido + estoque;
      const percentual = p.qtd_planejada ? Math.min(100, Math.round((totalDisponivel / p.qtd_planejada) * 100)) : 0;

      return {
        id: p.id,
        codigo: p.codigo_pedido,
        qtd_planejada: p.qtd_planejada,
        produzido,
        estoque,
        totalDisponivel,
        percentual,
        status: totalDisponivel >= p.qtd_planejada ? "FINALIZADO" : "PRODUZINDO",
        insumos,
      };
    })
  );

  return Response.json({ pedidos: pedidosComEstoque });
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

  // Busca o produto para considerar estoque na decisão de "finalizado"
  const pedidoIdTemp = Number(pedido.codigo_pedido.replace("PD-", ""));
  const { data: pedidoCompraTemp } = await supabase
    .from("pedidos_compra")
    .select("produto_id")
    .eq("id", pedidoIdTemp)
    .single();

  let estoqueDisponivel = 0;
  if (pedidoCompraTemp?.produto_id) {
    const { data: produtoTemp } = await supabase
      .from("produtos")
      .select("quantidade")
      .eq("id", pedidoCompraTemp.produto_id)
      .single();
    estoqueDisponivel = produtoTemp?.quantidade || 0;
  }

  // Finalizado quando: produzido + estoque >= planejado
  const totalDisponivel = novaQtd + estoqueDisponivel;
  const finalizado = totalDisponivel >= pedido.qtd_planejada;
  const status = finalizado ? "✅ FINALIZADO" : "🔄 PRODUZINDO";

  let estoqueAtualizado = false;

  // Se foi finalizado, adiciona ao estoque
  if (finalizado) {
    try {
      // Busca o produto via código do pedido (PD-{id})
      const pedidoId = Number(pedido.codigo_pedido.replace("PD-", ""));
      const { data: pedidoCompra } = await supabase
        .from("pedidos_compra")
        .select("id, produto_id, produtos(nome), solicitante, quantidade")
        .eq("id", pedidoId)
        .single();

      if (pedidoCompra?.produto_id) {
        // Salva localização do pedido
        const { data: localizacao } = await supabase
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
        if (produto !== null && produto !== undefined) {
          const quantidadeAtual = produto.quantidade || 0;
          const novaQuantidade = quantidadeAtual + novaQtd;

          const { error: eEstoque } = await supabase
            .from("produtos")
            .update({ quantidade: novaQuantidade })
            .eq("id", pedidoCompra.produto_id);

          if (!eEstoque) {
            estoqueAtualizado = true;
          }
        }

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
    } catch (error) {
      console.error("Erro ao atualizar estoque:", error);
    }
  }

  // Verificar se a OP inteira foi finalizada (todos os pedidos em 100%)
  const { data: pedidoFull, error: ePedidoFull } = await supabase
    .from("pedidos_op")
    .select("ordem_id, codigo_pedido, quantidade_produzida, qtd_planejada")
    .eq("id", Number(pedido_id))
    .single();

  if (pedidoFull?.ordem_id) {
    const { data: todosPedidos } = await supabase
      .from("pedidos_op")
      .select("codigo_pedido, quantidade_produzida, qtd_planejada")
      .eq("ordem_id", pedidoFull.ordem_id);

    // Para cada pedido, busca o estoque e calcula total disponível
    const pedidosComEstoque = await Promise.all(
      (todosPedidos || []).map(async (p) => {
        const pedidoIdTemp = Number(p.codigo_pedido.replace("PD-", ""));
        const { data: pedidoCompraTemp } = await supabase
          .from("pedidos_compra")
          .select("produto_id")
          .eq("id", pedidoIdTemp)
          .single();

        let estoqueTemp = 0;
        if (pedidoCompraTemp?.produto_id) {
          const { data: produtoTemp } = await supabase
            .from("produtos")
            .select("quantidade")
            .eq("id", pedidoCompraTemp.produto_id)
            .single();
          estoqueTemp = produtoTemp?.quantidade || 0;
        }

        const produzidoTemp = p.quantidade_produzida || 0;
        const totalDisponivel = produzidoTemp + estoqueTemp;

        return {
          totalDisponivel,
          qtd_planejada: p.qtd_planejada,
        };
      })
    );

    const todosFinalizados = pedidosComEstoque.every((p) => p.totalDisponivel >= p.qtd_planejada);
    if (todosFinalizados) {
      await supabase
        .from("ordens_producao")
        .update({ finalizado_em: new Date().toISOString(), status: "CONCLUIDA" })
        .eq("id", pedidoFull.ordem_id)
        .is("finalizado_em", null);
    }
  }

  return Response.json({ ok: true, novaQtd, status, estoqueAtualizado });
}
