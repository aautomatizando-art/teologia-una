import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";

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
    supabase
      .from("producao_registros")
      .select("paletes, registrado_em")
      .eq("ordem_id", ordem.id)
      .order("registrado_em", { ascending: true }),
    supabase.from("perdas_embalagem").select("tipo, quantidade").eq("ordem_id", ordem.id),
    supabase.from("problemas_qualidade").select("tipo, quantidade").eq("ordem_id", ordem.id),
  ]);

  const agrupar = (linhas) => {
    const m = {};
    for (const l of linhas || []) m[l.tipo] = (m[l.tipo] || 0) + l.quantidade;
    return Object.entries(m).map(([tipo, quantidade]) => ({ tipo, quantidade }));
  };

  // Soma produção de paletes diretos + produção de pedidos (em caixas, consideradas como unidade de palete)
  const produzidoPaletes = (registros.data || []).reduce((s, r) => s + r.paletes, 0);
  const produzidoPedidos = (pedidos.data || []).reduce((s, p) => s + (p.quantidade_produzida || 0), 0);
  const produzido = produzidoPaletes + produzidoPedidos;

  return Response.json({
    ordem: {
      numero: ordem.numero,
      produto: ordem.produtos?.nome || null,
      meta_paletes: ordem.meta_paletes,
      status: ordem.status,
      produzido,
      caixasPorPalete: ordem.produtos?.caixas_por_palete || 1,
      percentual: ordem.meta_paletes ? Math.min(100, Math.round((produzido / ordem.meta_paletes) * 100)) : 0,
    },
    pedidos: pedidos.data || [],
    registros: registros.data || [],
    perdas: agrupar(perdas.data),
    problemas: agrupar(problemas.data),
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
  const { op } = body;
  const { data: ordem } = await supabase
    .from("ordens_producao")
    .select("id")
    .eq("numero", (op || "").trim())
    .maybeSingle();
  if (!ordem) return Response.json({ error: `Ordem "${op}" não encontrada.` }, { status: 404 });

  let error = null;
  if (acao === "paletes") {
    ({ error } = await supabase
      .from("producao_registros")
      .insert({ ordem_id: ordem.id, paletes: Number(body.paletes) || 0 }));
  } else if (acao === "perda") {
    ({ error } = await supabase
      .from("perdas_embalagem")
      .insert({ ordem_id: ordem.id, tipo: body.tipo || "Outros", quantidade: Number(body.quantidade) || 0 }));
  } else if (acao === "problema") {
    ({ error } = await supabase
      .from("problemas_qualidade")
      .insert({ ordem_id: ordem.id, tipo: body.tipo || "Outros", quantidade: Number(body.quantidade) || 0 }));
  } else {
    return Response.json({ error: "Ação inválida." }, { status: 400 });
  }

  if (error) return Response.json({ error: error.message }, { status: 500 });
  return Response.json({ ok: true });
}
