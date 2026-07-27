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
  if (status === 5) return { background: "rgba(6,182,212,.18)", borderLeft: "4px solid #06b6d4" };
  if (status === 3 || status === 4) return { background: "rgba(34,197,94,.35)", borderLeft: "4px solid #22c55e" };
  return {};
}

const EMOJI_CRIT = { EMERGENCIAL: "🟣", URGENTE: "🔴", MODERADO: "🟢" };

function agoraData() { return new Date().toISOString().slice(0, 10); }
function agoraHora() { return new Date().toTimeString().slice(0, 5); }

const formHeaderVazio = () => ({
  data: agoraData(),
  hora: agoraHora(),
  solicitante: "",
  criticidade: "MODERADO",
  nome_cliente: "",
  regiao: "",
  comprador: "",
  vendedor: "",
});

export default function PaginaCompras() {
  // ── Auth local ──
  const [autenticado, setAutenticado] = useState(false);
  const [senhaInput, setSenhaInput] = useState("");
  const [erroLogin, setErroLogin] = useState("");
  const [senhaPedidos, setSenhaPedidos] = useState("");

  // ── Dados ──
  const [pedidos, setPedidos] = useState(null);
  const [produtos, setProdutos] = useState([]);
  const [ordens, setOrdens] = useState([]);
  const [erro, setErro] = useState("");
  const [ok, setOk] = useState("");
  const [salvando, setSalvando] = useState(false);

  // ── Novo pedido ──
  const [formHeader, setFormHeader] = useState(formHeaderVazio());
  const [linhasProduto, setLinhasProduto] = useState([{ produto_id: "", quantidade: "" }]);

  // ── Gerar OP ──
  const [opForm, setOpForm] = useState({ numero: "", data: agoraData(), hora: agoraHora(), solicitante: "", meta: "" });
  const [opSelecionados, setOpSelecionados] = useState([]);
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
  useEffect(() => { if (autenticado) carregar(); }, [autenticado]);

  // ── Login ──
  function entrar(e) {
    e.preventDefault();
    if (!senhaInput.trim()) { setErroLogin("Informe a senha."); return; }
    setSenhaPedidos(senhaInput);
    setAutenticado(true);
    setErroLogin("");
  }

  function sair() {
    setAutenticado(false);
    setSenhaPedidos("");
    setSenhaInput("");
    setPedidos(null);
  }

  // ── Produtos do pedido ──
  function adicionarLinha() {
    setLinhasProduto((prev) => [...prev, { produto_id: "", quantidade: "" }]);
  }
  function removerLinha(i) {
    setLinhasProduto((prev) => prev.filter((_, idx) => idx !== i));
  }
  function atualizarLinha(i, key, val) {
    setLinhasProduto((prev) => prev.map((l, idx) => idx === i ? { ...l, [key]: val } : l));
  }

  // ── Criar pedido(s) ──
  async function criar(e) {
    e.preventDefault();
    const linhasValidas = linhasProduto.filter((l) => l.produto_id && l.quantidade);
    if (!linhasValidas.length) { setErro("Adicione ao menos um produto com quantidade."); return; }
    setSalvando(true); setErro(""); setOk("");
    try {
      const ids = [];
      for (const linha of linhasValidas) {
        const res = await fetch("/api/compras", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ ...formHeader, produto_id: linha.produto_id, quantidade: linha.quantidade, senha: senhaPedidos }),
        });
        const j = await res.json();
        if (!res.ok) { setErro(j.error || "Erro ao criar pedido."); return; }
        ids.push(j.id);
      }
      setOk(`${ids.length} pedido(s) criado(s): #${ids.join(", #")}!`);
      setFormHeader(formHeaderVazio());
      setLinhasProduto([{ produto_id: "", quantidade: "" }]);
      carregar();
    } catch { setErro("Erro de conexão."); }
    finally { setSalvando(false); }
  }

  // ── OP ──
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
    const metaTotal = novos.reduce((s, ped) => {
      const pr = produtos.find((x) => x.id === ped.produto_id);
      return s + Math.max(0, (ped.quantidade || 0) - (pr?.quantidade || 0));
    }, 0);
    setOpForm((f) => ({ ...f, numero: f.numero || proximoNumero, data: agoraData(), hora: agoraHora(), solicitante: f.solicitante || p.solicitante, meta: String(metaTotal) }));
  }

  function removerPedido(id) {
    const novos = opSelecionados.filter((p) => p.id !== id);
    setOpSelecionados(novos);
    const metaTotal = novos.reduce((s, ped) => {
      const pr = produtos.find((x) => x.id === ped.produto_id);
      return s + Math.max(0, (ped.quantidade || 0) - (pr?.quantidade || 0));
    }, 0);
    setOpForm((f) => ({ ...f, meta: String(metaTotal) }));
  }

  function todosTemEstoque() {
    return opSelecionados.every((ped) => (produtos.find((pr) => pr.id === ped.produto_id)?.quantidade || 0) >= (ped.quantidade || 0));
  }

  async function criarOrdem(e) {
    e.preventDefault();
    setCriandoOp(true); setErroOp(""); setOkOp("");
    try {
      if (todosTemEstoque()) {
        const res = await fetch("/api/retirar-estoque", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ pedido_ids: opSelecionados.map((p) => p.id), solicitante: opForm.solicitante, data: opForm.data, hora: opForm.hora, senha: senhaPedidos }),
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
        const res = await fetch("/api/ordens", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify({ numero: opForm.numero, data: opForm.data, hora: opForm.hora, solicitante: opForm.solicitante, meta_paletes: opForm.meta, pedido_ids: opSelecionados.map((p) => p.id), senha: senhaPedidos }),
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
    } catch { setErroOp("Erro de conexão."); }
    finally { setCriandoOp(false); }
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

  const corStatus = { ABERTA: "ok", CONCLUIDA: "alto", ENTREGUE: "ok", CANCELADA: "baixo" };

  // ── LOGIN GATE ──
  if (!autenticado) {
    return (
      <>
        <TopBar />
        <div style={{ minHeight: "80vh", display: "flex", alignItems: "center", justifyContent: "center", padding: 24 }}>
          <div className="card" style={{ width: "100%", maxWidth: 400 }}>
            <div style={{ textAlign: "center", marginBottom: 24 }}>
              <div style={{ fontSize: 48, marginBottom: 8 }}>🔒</div>
              <h2 style={{ color: "#a5b4fc", margin: 0, fontSize: 20 }}>Acesso Restrito</h2>
              <p className="muted" style={{ marginTop: 6, fontSize: 13 }}>
                Digite a senha para acessar a página de pedidos.
              </p>
            </div>
            {erroLogin && <div className="erro" style={{ marginBottom: 16 }}>{erroLogin}</div>}
            <form onSubmit={entrar}>
              <div className="campo" style={{ marginBottom: 20 }}>
                <label>Senha de Pedidos</label>
                <input
                  type="password"
                  value={senhaInput}
                  onChange={(e) => setSenhaInput(e.target.value)}
                  placeholder="••••••••"
                  autoFocus
                  autoComplete="current-password"
                />
              </div>
              <button className="btn" type="submit" style={{ width: "100%", justifyContent: "center" }}>
                Entrar →
              </button>
            </form>
          </div>
        </div>
      </>
    );
  }

  // ── PÁGINA PRINCIPAL ──
  return (
    <>
      <TopBar />
      <div className="shell">

        {/* ── Novo pedido de Venda ── */}
        <div className="card" style={{ marginBottom: 18 }}>
          <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 16 }}>
            <h3 style={{ margin: 0 }}>🛒 Novo Pedido de Venda</h3>
            <button className="btn sec" style={{ fontSize: 12 }} onClick={sair}>🔒 Sair</button>
          </div>

          {erro && <div className="erro">{erro}</div>}
          {ok && <div className="aviso" style={{ marginBottom: 14, borderColor: "rgba(34,197,94,.4)", color: "#86efac", background: "rgba(34,197,94,.1)" }}>✅ {ok}</div>}

          <form onSubmit={criar}>
            {/* ── Linha 1: identificação do pedido ── */}
            <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(160px, 1fr))", gap: 12, marginBottom: 12 }}>
              <div className="campo">
                <label>Data *</label>
                <input type="date" required value={formHeader.data} onChange={(e) => setFormHeader({ ...formHeader, data: e.target.value })} />
              </div>
              <div className="campo">
                <label>Hora *</label>
                <input type="time" required value={formHeader.hora} onChange={(e) => setFormHeader({ ...formHeader, hora: e.target.value })} />
              </div>
              <div className="campo">
                <label>Nome *</label>
                <input required value={formHeader.solicitante} placeholder="Solicitante"
                  onChange={(e) => setFormHeader({ ...formHeader, solicitante: e.target.value })} />
              </div>
              <div className="campo">
                <label>Prioridade *</label>
                <select value={formHeader.criticidade} onChange={(e) => setFormHeader({ ...formHeader, criticidade: e.target.value })}>
                  <option value="EMERGENCIAL">🟣 EMERGENCIAL</option>
                  <option value="URGENTE">🔴 URGENTE</option>
                  <option value="MODERADO">🟢 MODERADO</option>
                </select>
              </div>
              <div className="campo">
                <label>Nome do Cliente</label>
                <input value={formHeader.nome_cliente} placeholder="Cliente"
                  onChange={(e) => setFormHeader({ ...formHeader, nome_cliente: e.target.value })} />
              </div>
              <div className="campo">
                <label>Região</label>
                <input value={formHeader.regiao} placeholder="Região"
                  onChange={(e) => setFormHeader({ ...formHeader, regiao: e.target.value })} />
              </div>
              <div className="campo">
                <label>Comprador</label>
                <input value={formHeader.comprador} placeholder="Comprador"
                  onChange={(e) => setFormHeader({ ...formHeader, comprador: e.target.value })} />
              </div>
              <div className="campo">
                <label>Vendedor</label>
                <input value={formHeader.vendedor} placeholder="Vendedor"
                  onChange={(e) => setFormHeader({ ...formHeader, vendedor: e.target.value })} />
              </div>
            </div>

            {/* ── Linhas de produto ── */}
            <div style={{ borderTop: "1px solid #26305c", paddingTop: 14, marginBottom: 14 }}>
              <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 10 }}>
                <span style={{ fontSize: 13, color: "#8b96c0", fontWeight: 600 }}>Produtos do pedido</span>
                <button type="button" className="btn sec" style={{ fontSize: 12 }} onClick={adicionarLinha}>
                  + Adicionar Produto
                </button>
              </div>

              {linhasProduto.map((linha, i) => (
                <div key={i} style={{ display: "grid", gridTemplateColumns: "1fr 120px auto", gap: 10, marginBottom: 8, alignItems: "end" }}>
                  <div className="campo" style={{ margin: 0 }}>
                    {i === 0 && <label>Produto *</label>}
                    <select required value={linha.produto_id} onChange={(e) => atualizarLinha(i, "produto_id", e.target.value)}>
                      <option value="">Selecione o produto...</option>
                      {produtos.map((p) => (
                        <option key={p.id} value={p.id}>
                          {p.nome}{p.quantidade > 0 ? ` (${p.quantidade} un.)` : ""}
                        </option>
                      ))}
                    </select>
                  </div>
                  <div className="campo" style={{ margin: 0 }}>
                    {i === 0 && <label>Qtd *</label>}
                    <input type="number" min="1" required value={linha.quantidade}
                      onChange={(e) => atualizarLinha(i, "quantidade", e.target.value)}
                      placeholder="Ex.: 100" />
                  </div>
                  <div style={{ paddingBottom: 2 }}>
                    {linhasProduto.length > 1 && (
                      <button type="button" onClick={() => removerLinha(i)}
                        style={{ background: "rgba(239,68,68,.15)", border: "1px solid rgba(239,68,68,.3)", color: "#fca5a5", borderRadius: 8, padding: "6px 12px", cursor: "pointer", fontSize: 16, lineHeight: 1 }}>
                        ✕
                      </button>
                    )}
                  </div>
                </div>
              ))}
            </div>

            <button className="btn" disabled={salvando} style={{ width: "100%", justifyContent: "center", fontSize: 15 }}>
              {salvando ? "Criando pedido(s)..." : "🛒 Criar Pedido"}
            </button>
          </form>
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
              <button className="btn" onClick={criarOrdem} disabled={criandoOp} style={{ background: "#22c55e" }}>
                {criandoOp ? "Retirando..." : "✅ Retirar do Estoque"}
              </button>
            ) : (
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
              const itens = pedidos.filter((p) => p.criticidade === c.chave && p.status_rastreio !== 6);
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
    </>
  );
}
