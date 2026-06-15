import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

// Faz um select com fallback caso a coluna "linhas" ainda não exista (migration pendente)
async function selecionarComLinhas(supabase, tabela, colunas, ordem_id, orderBy) {
  let q = supabase.from(tabela).select(`${colunas}, linhas`).eq("ordem_id", ordem_id);
  if (orderBy) q = q.order(orderBy, { ascending: true });
  let { data, error } = await q;
  if (error) {
    let q2 = supabase.from(tabela).select(colunas).eq("ordem_id", ordem_id);
    if (orderBy) q2 = q2.order(orderBy, { ascending: true });
    ({ data } = await q2);
  }
  return data || [];
}

// Faz um insert com fallback caso a coluna "linhas" ainda não exista (migration pendente)
async function inserirComLinhas(supabase, tabela, dados) {
  let { error } = await supabase.from(tabela).insert(dados);
  if (error && /linhas/.test(error.message)) {
    const { linhas, ...semLinhas } = dados;
    ({ error } = await supabase.from(tabela).insert(semLinhas));
  }
  return error;
}

// GET /api/producao?op=OP-1001  → dados completos da ordem de produção
export async function GET(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const numero = new URL(req.url).searchParams.get("op");
  if (!numero) {
    // sem OP: lista as ordens existentes para sugestão
    const { data } = await supabase
      .from("ordens_producao")
      .select("numero, status, criado_em")
      .order("criado_em", { ascending: false })
      .limit(20);
    return Response.json({ ordens: data || [] });
  }

  let { data: ordem, error } = await supabase
    .from("ordens_producao")
    .select("id, numero, meta_paletes, status, criado_em, produtos(nome, caixas_por_palete)")
    .eq("numero", numero.trim())
    .maybeSingle();

  // Coluna caixas_por_palete pode não existir ainda (migration pendente)
  if (error) {
    ({ data: ordem, error } = await supabase
      .from("ordens_producao")
      .select("id, numero, meta_paletes, status, criado_em, produtos(nome)")
      .eq("numero", numero.trim())
      .maybeSingle());
  }

  if (error) return Response.json({ error: error.message }, { status: 500 });
  if (!ordem) return Response.json({ error: `Ordem "${numero}" não encontrada.` }, { status: 404 });

  const [pedidos, registros, perdas, problemas] = await Promise.all([
    supabase.from("pedidos_op").select("codigo_pedido, qtd_planejada, quantidade_produzida").eq("ordem_id", ordem.id),
    selecionarComLinhas(supabase, "producao_registros", "paletes, registrado_em", ordem.id, "registrado_em"),
    selecionarComLinhas(supabase, "perdas_embalagem", "tipo, quantidade, registrado_em", ordem.id),
    selecionarComLinhas(supabase, "problemas_qualidade", "tipo, quantidade, registrado_em", ordem.id),
  ]);

  const agrupar = (linhas) => {
    const m = {};
    for (const l of linhas || []) m[l.tipo] = (m[l.tipo] || 0) + l.quantidade;
    return Object.entries(m).map(([tipo, quantidade]) => ({ tipo, quantidade }));
  };

  // Soma produção de paletes diretos (já em paletes) + produção de pedidos (em caixas, convertida para paletes)
  const caixasPorPalete = ordem.produtos?.caixas_por_palete || 1;
  const produzidoPaletes = registros.reduce((s, r) => s + r.paletes, 0);
  const produzidoPedidos = (pedidos.data || []).reduce((s, p) => s + (p.quantidade_produzida || 0), 0);
  const produzido = produzidoPaletes + produzidoPedidos / caixasPorPalete;
  const produzidoCx = produzido * caixasPorPalete;

  // Últimos lançamentos (paletes + perdas + problemas) com as linhas que produziram
  const lancamentos = [
    ...registros.map((r) => ({ tipo: "Paletes", detalhe: `${r.paletes} paletes`, linhas: r.linhas || null, quando: r.registrado_em })),
    ...perdas.map((r) => ({ tipo: "Perda", detalhe: `${r.tipo} (${r.quantidade}x)`, linhas: r.linhas || null, quando: r.registrado_em })),
    ...problemas.map((r) => ({ tipo: "Problema", detalhe: `${r.tipo} (${r.quantidade}x)`, linhas: r.linhas || null, quando: r.registrado_em })),
  ]
    .sort((a, b) => new Date(b.quando) - new Date(a.quando))
    .slice(0, 15);

  return Response.json({
    ordem: {
      numero: ordem.numero,
      produto: ordem.produtos?.nome || null,
      meta_paletes: ordem.meta_paletes,
      status: ordem.status,
      produzido,
      caixasPorPalete,
      percentual: ordem.meta_paletes ? Math.min(100, Math.round((produzidoCx / ordem.meta_paletes) * 100)) : 0,
    },
    pedidos: pedidos.data || [],
    registros,
    perdas: agrupar(perdas),
    problemas: agrupar(problemas),
    lancamentos,
  });
}

// POST /api/producao — registrar produção, perda, problema ou criar OP
export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const body = await req.json();
  const { acao } = body;

  if (acao === "criar_op") {
    const { numero, meta_paletes, produto_id, pedidos } = body;
    if (!numero) return Response.json({ error: "Informe o número da OP." }, { status: 400 });
    const { data: nova, error } = await supabase
      .from("ordens_producao")
      .insert({ numero: numero.trim(), meta_paletes: meta_paletes || 100, produto_id: produto_id || null })
      .select("id")
      .single();
    if (error) return Response.json({ error: error.message }, { status: 500 });
    if (Array.isArray(pedidos) && pedidos.length) {
      await supabase.from("pedidos_op").insert(
        pedidos
          .filter((p) => p.codigo_pedido)
          .map((p) => ({ ordem_id: nova.id, codigo_pedido: p.codigo_pedido, qtd_planejada: p.qtd_planejada || 0 }))
      );
    }
    return Response.json({ ok: true });
  }

  // registros vinculados a uma OP existente
  const { op, linhas } = body;
  const { data: ordem } = await supabase
    .from("ordens_producao")
    .select("id")
    .eq("numero", (op || "").trim())
    .maybeSingle();
  if (!ordem) return Response.json({ error: `Ordem "${op}" não encontrada.` }, { status: 404 });

  let error = null;
  if (acao === "paletes") {
    error = await inserirComLinhas(supabase, "producao_registros", {
      ordem_id: ordem.id,
      paletes: Number(body.paletes) || 0,
      linhas: linhas || null,
    });
  } else if (acao === "perda") {
    error = await inserirComLinhas(supabase, "perdas_embalagem", {
      ordem_id: ordem.id,
      tipo: body.tipo || "Outros",
      quantidade: Number(body.quantidade) || 0,
      linhas: linhas || null,
    });
  } else if (acao === "problema") {
    error = await inserirComLinhas(supabase, "problemas_qualidade", {
      ordem_id: ordem.id,
      tipo: body.tipo || "Outros",
      quantidade: Number(body.quantidade) || 0,
      linhas: linhas || null,
    });
  } else {
    return Response.json({ error: "Ação inválida." }, { status: 400 });
  }

  if (error) return Response.json({ error: error.message }, { status: 500 });
  return Response.json({ ok: true });
}
