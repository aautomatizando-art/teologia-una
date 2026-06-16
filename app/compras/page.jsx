"use client";
import { useEffect, useState } from "react";
import TopBar from "@/components/TopBar";

const COLUNAS = [
  { chave: "EMERGENCIAL", classe: "emergencial", emoji: "🟣" },
  { chave: "URGENTE", classe: "urgente", emoji: "🔴" },
  { chave: "MODERADO", classe: "moderado", emoji: "🟢" },
];

const ETAPA_LABEL = ["", "📋 Gerado OP", "🏭 Produzindo", "📦 No estoque", "🚛 Em separação", "🛣️ Em rota", "✅ Entregue"];

function corTicket(status) {
  if (status === 6) return { background: "rgba(34,197,94,.22)", borderLeft: "4px solid #22c55e" };
  if (status === 5) return { background: "rgba(6,182,212,.12)", borderLeft: "4px solid #06b6d4" };
  if (status === 3 || status === 4) return { background: "rgba(34,197,94,.10)", borderLeft: "4px solid #4ade80" };
  return {};
}

const EMOJI_CRIT = { EMERGENCIAL: "🟣", URGENTE: "🔴", MODERADO: "🟢" };

function agoraData() { return new Date().toISOString().slice(0, 10); }
function agoraHora() { return new Date().toTimeString().slice(0, 5); }

export default function PaginaCompras() {
  const [pedidos, setPedidos] = useState(null);
  const [produtos, setProdutos] = useState([]);
  const [ordens, setOrdens] = useState([]);
  const [erro, setErro] = useState("");
  const [ok, setOk] = useState("");
  const [salvando, setSalvando] = useState(false);
  const [senhaPedidos, setSenhaPedidos] = useState("");
  const [form, setForm] = useState({
    produto_id: "",
    quantidade: "",
    data: agoraData(),
    hora: agoraHora(),
    solicitante: "",
    criticidade: "MODERADO",
    nome_cliente: "",
    regiao: "",
    comprador: "",
    vendedor: "",
  });

  // ── Gerar ordem de produção ──
  const [opForm, setOpForm] = useState({ numero: "", data: agoraData(), hora: agoraHora(), solicitante: "", meta: "" });
  const [opSelecionados, setOpSelecionados] = useState([]); // pedidos escolhidos
  const [criandoOp, setCriandoOp] = useState(false);
  const [okOp, setOkOp] = useState("");
  const [erroOp, setErroOp] = useState("");
  const [filtroData, setFiltroData] = useState("");
  const [filtroStatus, setFiltroStatus] = useState("");
  const [filtroOP, setFiltroOP] = useState("");

  async function carregar() {
    try {
      const [a, b, c] = await Promise.all([fetch("/api/compras"), fetch("/api/estoque"), fetch("/api/ordens")]);
      const [ja, jb, jc] = await Promise.all([a.json(), b.json(), c.json()]);
      if (!a.ok) setErro(ja.error || "Erro ao carregar pedidos.");
      else setPedidos(ja.pedidos);
      if (b.ok) setProdutos(jb.produtos || []);
      if (c.ok) setOrdens(jc.ordens || []);
    } catch {
      setErro("Erro de conexão.");
    }
  }
  useEffect(() => { carregar(); }, []);

  async function criar(e) {
    e.preventDefault();
    setSalvando(true);
    setErro("");
    setOk("");
    try {
      const res = await fetch("/api/compras", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ ...form, senha: senhaPedidos }),
      });
      const j = await res.json();
      if (!res.ok) setErro(j.error || "Erro ao criar pedido.");
      else {
        setOk(`Pedido #${j.id} criado!`);
        setForm({ ...form, produto_id: "", quantidade: "", solicitante: "" });
        carregar();
      }
    } catch {
      setErro("Erro de conexão.");
    } finally {
      setSalvando(false);
    }
  }

  // pedidos que ainda não estão em nenhuma OP (vínculo PD-<id>)
  const idsVinculados = new Set(
    ordens.flatMap((o) => o.pedidos.map((p) => p.codigo)).filter((c) => c?.startsWith("PD-")).map((c) => Number(c.slice(3)))
  );
  const pedidosDisponiveis = (pedidos || []).filter((p) => !idsVinculados.has(p.id) && !opSelecionados.some((s) => s.id === p.id));

  const proximoNumero = (() => {
    const maior = ordens
      .map((o) => parseInt(String(o.numero).replace("OP-", ""), 10))
      .filter((n) => !isNaN(n))
      .reduce((a, b) => Math.max(a, b), 1000);
    return `OP-${maior + 1}`;
  })();

  function escolherPedido(id) {
    const p = (pedidos || []).find((x) => x.id === Number(id));
    if (!p) return;
    const novos = [...opSelecionados, p];
    setOpSelecionados(novos);

    // Calcula meta descontando estoque disponível
    const metaTotal = novos.reduce((s, ped) => {
      const produtoInfo = produtos.find((pr) => pr.id === ped.produto_id);
      const quantidadeEmEstoque = produtoInfo?.quantidade || 0;
      const necessario = Math.max(0, (ped.quantidade || 0) - quantidadeEmEstoque);
      return s + necessario;
    }, 0);

    setOpForm((f) => ({
      ...f,
      numero: f.numero || proximoNumero,
      data: agoraData(),
      hora: agoraHora(),
      solicitante: f.solicitante || p.solicitante,
      meta: String(metaTotal),
    }));
  }

  function removerPedido(id) {
    const novos = opSelecionados.filter((p) => p.id !== id);
    setOpSelecionados(novos);

    // Recalcula meta descontando estoque
    const metaTotal = novos.reduce((s, ped) => {
      const produtoInfo = produtos.find((pr) => pr.id === ped.produto_id);
      const quantidadeEmEstoque = produtoInfo?.quantidade || 0;
      const necessario = Math.max(0, (ped.quantidade || 0) - quantidadeEmEstoque);
      return s + necessario;
    }, 0);

    setOpForm((f) => ({ ...f, meta: String(metaTotal) }));
  }

  // Verifica se todos os pedidos têm estoque suficiente
  function todosTemEstoque() {
    return opSelecionados.every((ped) => {
      const produtoInfo = produtos.find((pr) => pr.id === ped.produto_id);
      return (produtoInfo?.quantidade || 0) >= (ped.quantidade || 0);
    });
  }

  async function criarOrdem(e) {
    e.preventDefault();
    setCriandoOp(true);
    setErroOp("");
    setOkOp("");
    try {
      // Se todos têm estoque, retirar do estoque direto
      if (todosTemEstoque()) {
        const res = await fetch("/api/retirar-estoque", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            pedido_ids: opSelecionados.map((p) => p.id),
            solicitante: opForm.solicitante,
            data: opForm.data,
            hora: opForm.hora,
            senha: senhaPedidos,
          }),
        });
        const j = await res.json();
        if (!res.ok) setErroOp(j.error || "Erro ao retirar estoque.");
        else {
          setOkOp(`✅ ${j.retirados} pedido(s) retirado(s) do estoque! 📦`);
          setOpSelecionados([]);
          setOpForm({ numero: "", data: agoraData(), hora: agoraHora(), solicitante: "", meta: "" });
          carregar();
        }
      } else {
        // Caso contrário, criar OP de produção
        const res = await fetch("/api/ordens", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({
            numero: opForm.numero,
            data: opForm.data,
            hora: opForm.hora,
            solicitante: opForm.solicitante,
            meta_paletes: opForm.meta,
            pedido_ids: opSelecionados.map((p) => p.id),
            senha: senhaPedidos,
          }),
        });
        const j = await res.json();
        if (!res.ok) setErroOp(j.error || "Erro ao criar ordem.");
        else {
          setOkOp(`Ordem ${j.numero} criada e enviada ao grupo do WhatsApp! 📲`);
          setOpSelecionados([]);
          setOpForm({ numero: "", data: agoraData(), hora: agoraHora(), solicitante: "", meta: "" });
          carregar();
        }
      }
    } catch {
      setErroOp("Erro de conexão.");
    } finally {
      setCriandoOp(false);
    }
  }

  async function mudarStatusOrdem(id, status) {
    setErroOp("");
    const res = await fetch("/api/ordens", {
      method: "PATCH",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify({ id, status, senha: senhaPedidos }),
    });
    const j = await res.json();
    if (!res.ok) setErroOp(j.error || "Erro ao mudar status.");
    carregar();
  }

  const corStatus = { ABERTA: "ok", CONCLUIDA: "alto", CANCELADA: "baixo" };

  return (
    <div className="shell">
      <TopBar />

      {/* ── Senha de ações da página ── */}
      <div className="card" style={{ marginBottom: 18 }}>
        <div className="campo" style={{ maxWidth: 280 }}>
          <label>🔒 Senha de Pedidos</label>
          <input
            type="password"
            value={senhaPedidos}
            onChange={(e) => setSenhaPedidos(e.target.value)}
            placeholder="Necessária para criar/alterar pedidos e ordens"
          />
        </div>
      </div>

      {/* ── Novo pedido de venda ── */}
      <div className="card" style={{ marginBottom: 18 }}>
        <h3>🛒 Novo pedido de Venda</h3>
        {erro && <div className="erro">{erro}</div>}
        {ok && <div className="aviso" style={{ marginBottom: 14, borderColor: "rgba(34,197,94,.4)", color: "#86efac", background: "rgba(34,197,94,.1)" }}>✅ {ok}</div>}
        <form className="linha" onSubmit={criar}>
          <div className="campo" style={{ flex: 2, minWidth: 240 }}>
            <label>Produto * {form.produto_id && (() => { const p = produtos.find(x => x.id === Number(form.produto_id)); return p ? `(${p.quantidade} un. em estoque)` : ""; })()}</label>
            <select required value={form.produto_id} onChange={(e) => setForm({ ...form, produto_id: e.target.value })}>
              <option value="">Selecione o produto...</option>
              {produtos.map((p) => <option key={p.id} value={p.id}>{p.nome}</option>)}
            </select>
          </div>
          <div className="campo" style={{ maxWidth: 110 }}>
            <label>Qtd</label>
            <input type="number" min="1" value={form.quantidade}
              onChange={(e) => setForm({ ...form, quantidade: e.target.value })} />
          </div>
          <div className="campo" style={{ maxWidth: 160 }}>
            <label>Data *</label>
            <input type="date" required value={form.data} onChange={(e) => setForm({ ...form, data: e.target.value })} />
          </div>
          <div className="campo" style={{ maxWidth: 120 }}>
            <label>Hora *</label>
            <input type="time" required value={form.hora} onChange={(e) => setForm({ ...form, hora: e.target.value })} />
          </div>
          <div className="campo">
            <label>Nome *</label>
            <input required value={form.solicitante} placeholder="Solicitante"
              onChange={(e) => setForm({ ...form, solicitante: e.target.value })} />
          </div>
          <div className="campo" style={{ maxWidth: 170 }}>
            <label>Prioridade *</label>
            <select value={form.criticidade} onChange={(e) => setForm({ ...form, criticidade: e.target.value })}>
              <option value="EMERGENCIAL">🟣 EMERGENCIAL</option>
              <option value="URGENTE">🔴 URGENTE</option>
              <option value="MODERADO">🟢 MODERADO</option>
            </select>
          </div>
          <button className="btn" disabled={salvando}>{salvando ? "Criando..." : "Criar pedido"}</button>
        </form>
        <div className="linha" style={{ marginTop: 12, gap: 12 }}>
          <div className="campo" style={{ flex: 1 }}>
            <label>Nome do Cliente</label>
            <input value={form.nome_cliente} placeholder="Cliente"
              onChange={(e) => setForm({ ...form, nome_cliente: e.target.value })} />
          </div>
          <div className="campo" style={{ flex: 1 }}>
            <label>Região</label>
            <input value={form.regiao} placeholder="Região"
              onChange={(e) => setForm({ ...form, regiao: e.target.value })} />
          </div>
          <div className="campo" style={{ flex: 1 }}>
            <label>Comprador</label>
            <input value={form.comprador} placeholder="Comprador"
              onChange={(e) => setForm({ ...form, comprador: e.target.value })} />
          </div>
          <div className="campo" style={{ flex: 1 }}>
            <label>Vendedor</label>
            <input value={form.vendedor} placeholder="Vendedor"
              onChange={(e) => setForm({ ...form, vendedor: e.target.value })} />
          </div>
        </div>
      </div>

      {/* ── Gerar ordem de produção ── */}
      <div className="card" style={{ marginBottom: 18 }}>
        <h3>🏭 Gerar ordem de produção</h3>
        {erroOp && <div className="erro">{erroOp}</div>}
        {okOp && <div className="aviso" style={{ marginBottom: 14, borderColor: "rgba(34,197,94,.4)", color: "#86efac", background: "rgba(34,197,94,.1)" }}>✅ {okOp}</div>}
        <form onSubmit={criarOrdem}>
          <div className="linha" style={{ marginBottom: 12 }}>
            <div className="campo" style={{ flex: 2, minWidth: 260 }}>
              <label>Número do pedido (escolher preenche os dados)</label>
              <select value="" onChange={(e) => e.target.value && escolherPedido(e.target.value)}>
                <option value="">{pedidosDisponiveis.length ? "Selecione um pedido..." : "Nenhum pedido disponível"}</option>
                {pedidosDisponiveis.map((p) => (
                  <option key={p.id} value={p.id}>
                    {EMOJI_CRIT[p.criticidade]} Pedido #{p.id} • {p.produtos?.nome} • {p.quantidade} un. • {p.solicitante}
                  </option>
                ))}
              </select>
            </div>
            <div className="campo" style={{ maxWidth: 140 }}>
              <label>Nº da ordem</label>
              <input value={opForm.numero} placeholder={proximoNumero}
                onChange={(e) => setOpForm({ ...opForm, numero: e.target.value })} />
            </div>
            <div className="campo" style={{ maxWidth: 160 }}>
              <label>Data *</label>
              <input type="date" required value={opForm.data} onChange={(e) => setOpForm({ ...opForm, data: e.target.value })} />
            </div>
            <div className="campo" style={{ maxWidth: 120 }}>
              <label>Hora *</label>
              <input type="time" required value={opForm.hora} onChange={(e) => setOpForm({ ...opForm, hora: e.target.value })} />
            </div>
            <div className="campo">
              <label>Solicitante *</label>
              <input required value={opForm.solicitante} placeholder="Nome"
                onChange={(e) => setOpForm({ ...opForm, solicitante: e.target.value })} />
            </div>
            <div className="campo" style={{ maxWidth: 130 }}>
              <label>Meta (paletes)</label>
              <input type="number" min="1" value={opForm.meta}
                onChange={(e) => setOpForm({ ...opForm, meta: e.target.value })} />
            </div>
          </div>

          {opSelecionados.length > 0 && (
            <div style={{ display: "flex", gap: 8, flexWrap: "wrap", marginBottom: 14 }}>
              {opSelecionados.map((p) => (
                <span key={p.id} className="badge ok" style={{ background: "rgba(99,102,241,.14)", color: "#a5b4fc", fontSize: 13 }}>
                  {EMOJI_CRIT[p.criticidade]} #{p.id} {p.produtos?.nome} ({p.quantidade} un.)
                  <button type="button" onClick={() => removerPedido(p.id)}
                    style={{ background: "none", border: "none", color: "#fca5a5", cursor: "pointer", fontWeight: 800, fontSize: 14 }}>✕</button>
                </span>
              ))}
            </div>
          )}

          {opSelecionados.length === 0 ? (
            <span className="muted" style={{ marginLeft: 0, fontSize: 13 }}>Selecione ao menos um pedido acima.</span>
          ) : Number(opForm.meta) === 0 ? (
            // Todos têm estoque — retirar direto
            <button className="btn" onClick={criarOrdem} disabled={criandoOp} style={{ background: "#22c55e" }}>
              {criandoOp ? "Retirando..." : "✅ Retirar do Estoque"}
            </button>
          ) : (
            // Falta produto — criar OP
            <button className="btn" onClick={criarOrdem} disabled={criandoOp} style={{ background: "#6366f1" }}>
              {criandoOp ? "Criando ordem..." : "🏭 Criar Ordem de Produção"}
            </button>
          )}
        </form>
      </div>

      {!pedidos && !erro && <div className="spin" />}

      {/* ── Kanban de criticidade ── */}
      {pedidos && (
        <div className="kanban" style={{ marginBottom: 18 }}>
          {COLUNAS.map((c) => {
            const itens = pedidos.filter((p) => p.criticidade === c.chave);
            return (
              <div key={c.chave} className={`coluna ${c.classe}`}>
                <div className="cab">
                  <span>{c.emoji} {c.chave}</span>
                  <span>{itens.length}</span>
                </div>
                <div className="corpo">
                  {itens.length === 0 && <span className="muted center" style={{ fontSize: 13, padding: 20 }}>Nenhum pedido</span>}
                  {itens.map((p) => (
                    <a key={p.id} className="ticket" href={`/rastreio?pedido=${p.id}`} title="Abrir rastreio"
                      style={corTicket(p.status_rastreio)}>
                      <span className="prod">#{p.id} • {p.produtos?.nome}</span>
                      <span className="meta">Qtd: {p.quantidade} • {p.solicitante}</span>
                      <span className="meta">📅 {p.data?.split("-").reverse().join("/")} ⏰ {String(p.hora).slice(0, 5)}</span>
                      {p.status_rastreio > 0 && (
                        <span style={{ fontSize: 11, fontWeight: 700, marginTop: 4, color: p.status_rastreio === 6 ? "#4ade80" : p.status_rastreio >= 3 ? "#86efac" : "#a5b4fc" }}>
                          {ETAPA_LABEL[p.status_rastreio]}
                        </span>
                      )}
                    </a>
                  ))}
                </div>
              </div>
            );
          })}
        </div>
      )}

      {/* ── Ordens de produção criadas ── */}
      {pedidos && (
        <div className="card">
          <h3>📋 Ordens de produção ({ordens.length})</h3>

          {/* Filtros */}
          {ordens.length > 0 && (
            <div className="linha" style={{ marginBottom: 16, gap: 12 }}>
              <div className="campo" style={{ flex: 1, minWidth: 150 }}>
                <label>Filtrar por OP</label>
                <input value={filtroOP} onChange={(e) => setFiltroOP(e.target.value)} placeholder="Ex: OP-1010" />
              </div>
              <div className="campo" style={{ flex: 1, minWidth: 150 }}>
                <label>Filtrar por Data</label>
                <input type="date" value={filtroData} onChange={(e) => setFiltroData(e.target.value)} />
              </div>
              <div className="campo" style={{ flex: 1, minWidth: 150 }}>
                <label>Filtrar por Status</label>
                <select value={filtroStatus} onChange={(e) => setFiltroStatus(e.target.value)}>
                  <option value="">— Todos —</option>
                  <option value="ABERTA">ABERTA</option>
                  <option value="CONCLUIDA">CONCLUIDA</option>
                  <option value="ENTREGUE">ENTREGUE</option>
                  <option value="CANCELADA">CANCELADA</option>
                </select>
              </div>
            </div>
          )}

          {ordens.length === 0 && <p className="muted">Nenhuma ordem criada ainda.</p>}
          {ordens.length > 0 && (
            <div style={{ overflowX: "auto" }}>
              <table className="tab">
                <thead>
                  <tr>
                    <th>Ordem</th><th>Produto</th><th>Pedidos</th><th>Produção</th><th>Finalizado em</th><th>Status</th><th>Criada em</th><th style={{ textAlign: "right" }}>Ações</th>
                  </tr>
                </thead>
                <tbody>
                  {ordens.filter((o) => {
                    // Aplicar filtros
                    if (filtroOP && !o.numero.toLowerCase().includes(filtroOP.toLowerCase())) return false;
                    if (filtroData && !new Date(o.criado_em).toISOString().startsWith(filtroData)) return false;
                    if (filtroStatus && o.status !== filtroStatus) return false;
                    return true;
                  }).map((o) => (
                    <tr key={o.id}>
                      <td style={{ fontWeight: 700 }}>{o.numero}</td>
                      <td>{o.produto || "—"}</td>
                      <td>{o.pedidos.length ? o.pedidos.map((p) => p.codigo).join(", ") : "—"}</td>
                      <td style={{ minWidth: 140 }}>
                        {o.produzido}/{o.meta_paletes} paletes ({o.percentual}%)
                        <div className="barra" style={{ marginTop: 5 }}>
                          <div style={{ width: `${o.percentual}%`, background: o.percentual >= 100 ? "#22c55e" : "#6366f1" }} />
                        </div>
                      </td>
                      <td>{o.finalizado_em ? new Date(o.finalizado_em).toLocaleString("pt-BR", { day: "2-digit", month: "2-digit", year: "2-digit", hour: "2-digit", minute: "2-digit" }) : "—"}</td>
                      <td><span className={`badge ${corStatus[o.status] || "ok"}`}>{o.status}</span></td>
                      <td>{new Date(o.criado_em).toLocaleString("pt-BR", { day: "2-digit", month: "2-digit", year: "2-digit", hour: "2-digit", minute: "2-digit" })}</td>
                      <td style={{ textAlign: "right", whiteSpace: "nowrap" }}>
                        {o.status === "ABERTA" ? (
                          <>
                            <button className="btn mini sec" style={{ marginRight: 6 }} onClick={() => mudarStatusOrdem(o.id, "CONCLUIDA")}>✅ Concluir</button>
                            <button className="btn mini sec" style={{ color: "#fca5a5" }} onClick={() => mudarStatusOrdem(o.id, "CANCELADA")}>✕ Cancelar</button>
                          </>
                        ) : (
                          <button className="btn mini sec" onClick={() => mudarStatusOrdem(o.id, "ABERTA")}>↩ Reabrir</button>
                        )}
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          )}
        </div>
      )}
    </div>
  );
}
