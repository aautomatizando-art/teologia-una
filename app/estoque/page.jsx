"use client";
import { useEffect, useMemo, useState } from "react";
import TopBar from "@/components/TopBar";
import {
  ResponsiveContainer, BarChart, Bar, XAxis, YAxis, Tooltip, CartesianGrid, Cell,
} from "recharts";

const TOOLTIP = { background: "#1a2347", border: "1px solid #26305c", borderRadius: 10, color: "#e8ecf8" };

function corStatus(s) {
  if (s === "ESTOQUE BAIXO") return { badge: "baixo", barra: "#ef4444" };
  if (s === "ESTOQUE ALTO") return { badge: "alto", barra: "#f59e0b" };
  return { badge: "ok", barra: "#22c55e" };
}

export default function PaginaEstoque() {
  const [dados, setDados] = useState(null);
  const [erro, setErro] = useState("");
  const [soCriticos, setSoCriticos] = useState(false);
  const [modal, setModal] = useState(null); // produto selecionado para edição
  const [infoProduto, setInfoProduto] = useState(null); // produto selecionado no gráfico
  const [form, setForm] = useState({});
  const [salvando, setSalvando] = useState(false);
  const [msgOk, setMsgOk] = useState("");
  const [pedindoSenha, setPedindoSenha] = useState(false);
  const [senhaADM, setSenhaADM] = useState("");
  const [produtoParaEditar, setProdutoParaEditar] = useState(null);
  const [carregando, setCarregando] = useState(false);
  const [qtdDesbloqueada, setQtdDesbloqueada] = useState(false);
  const [pedindoSenhaQtd, setPedindoSenhaQtd] = useState(false);
  const [senhaQtd, setSenhaQtd] = useState("");
  const [erroQtd, setErroQtd] = useState("");

  async function carregar() {
    setCarregando(true);
    try {
      const res = await fetch("/api/estoque");
      const j = await res.json();
      if (!res.ok) setErro(j.error || "Erro ao carregar estoque.");
      else setDados(j);
    } catch {
      setErro("Erro de conexão.");
    } finally {
      setCarregando(false);
    }
  }

  // Carregar dados iniciais
  useEffect(() => { carregar(); }, []);

  // Refresh automático a cada 30 segundos
  useEffect(() => {
    const intervalo = setInterval(() => {
      carregar();
    }, 30000);
    return () => clearInterval(intervalo);
  }, []);

  const lista = useMemo(() => {
    let l = dados?.produtos || [];
    if (soCriticos) l = l.filter((p) => p.status !== "NORMAL");
    return l;
  }, [dados, soCriticos]);

  function abrirEditar(p) {
    setPedindoSenha(true);
    setProdutoParaEditar(p);
    setSenhaADM("");
    setErro("");
  }

  async function confirmarSenhaADM() {
    setErro("");
    try {
      const res = await fetch("/api/validar-senha", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ senha: senhaADM, tipo: "adm" }),
      });

      if (!res.ok) {
        setErro("Senha ADM incorreta.");
        return;
      }

      setPedindoSenha(false);
      setSenhaADM("");

      const agora = new Date();
      setForm({
        produto_id: produtoParaEditar.id,
        nome_produto: produtoParaEditar.nome,
        quantidade_maxima: produtoParaEditar.qtd_maxima || 500,
        quantidade_minima: produtoParaEditar.qtd_minima || 50,
        caixas_por_palete: produtoParaEditar.caixas_por_palete || 1,
        quantidade: produtoParaEditar.quantidade ?? 0,
        rua: produtoParaEditar.rua || "",
        prateleira: produtoParaEditar.prateleira || "",
        lote: produtoParaEditar.lote || "",
        data: agora.toISOString().slice(0, 10),
        hora: agora.toTimeString().slice(0, 5),
      });
      setQtdDesbloqueada(false);
      setModal(produtoParaEditar);
      setMsgOk("");
    } catch {
      setErro("Erro ao validar senha.");
    }
  }

  async function confirmarDesbloqueioQtd() {
    setErroQtd("");
    try {
      const res = await fetch("/api/validar-senha", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ senha: senhaQtd, tipo: "adm" }),
      });

      if (!res.ok) {
        setErroQtd("Senha ADM incorreta.");
        return;
      }

      setQtdDesbloqueada(true);
      setPedindoSenhaQtd(false);
      setSenhaQtd("");
    } catch {
      setErroQtd("Erro ao validar senha.");
    }
  }

  async function editar(e) {
    e.preventDefault();
    setSalvando(true);
    setErro("");
    try {
      const res = await fetch("/api/estoque/editar", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(form),
      });
      const j = await res.json();
      if (!res.ok) setErro(j.error || "Erro ao editar produto.");
      else {
        setMsgOk(`Produto editado com sucesso!`);
        setModal(null);
        carregar();
      }
    } catch {
      setErro("Erro de conexão.");
    } finally {
      setSalvando(false);
    }
  }

  return (
    <div className="shell">
      <TopBar />

      {dados && (
        <div className="grid g4" style={{ marginBottom: 18 }}>
          <div className="card"><h3>📦 Total em estoque</h3><div className="kpi">{dados.total.toLocaleString("pt-BR")} <small>un.</small></div></div>
          <div className="card"><h3>🗂️ Produtos</h3><div className="kpi">{dados.produtos.length}</div></div>
          <div className="card"><h3>🔴 Estoque baixo</h3><div className="kpi" style={{ color: "#ef4444" }}>{dados.baixos}</div></div>
          <div className="card"><h3>🟠 Estoque alto</h3><div className="kpi" style={{ color: "#f59e0b" }}>{dados.altos}</div></div>
        </div>
      )}

      {lista.length > 0 && (
        <div className="card grafico-estoque" style={{ marginBottom: 18 }}>
          <h3>📊 Quantidade por produto</h3>
          <p className="sub" style={{ marginTop: -8, marginBottom: 12 }}>Clique em uma coluna para ver os detalhes do produto</p>
          <div style={{ overflowX: "auto" }}>
            <div style={{ height: 320, minWidth: Math.max(900, lista.length * 70) }}>
              <ResponsiveContainer>
                <BarChart data={lista} margin={{ top: 8, right: 12, left: -16, bottom: 70 }}>
                  <CartesianGrid stroke="#26305c" strokeDasharray="3 3" />
                  <XAxis dataKey="nome" tick={{ fill: "#8b96c0", fontSize: 10 }} interval={0} angle={-45} textAnchor="end" height={90} />
                  <YAxis tick={{ fill: "#8b96c0", fontSize: 11 }} />
                  <Tooltip contentStyle={TOOLTIP} itemStyle={{ color: "#fff" }} cursor={{ fill: "rgba(99,102,241,0.08)" }} separator="" formatter={(v) => [`${v.toLocaleString("pt-BR")} un.`, ""]} />
                  <Bar dataKey="quantidade" name="Quantidade" radius={[6, 6, 0, 0]} onClick={(data) => setInfoProduto(data.payload)}>
                    {lista.map((p) => <Cell key={p.id} fill={corStatus(p.status).barra} />)}
                  </Bar>
                </BarChart>
              </ResponsiveContainer>
            </div>
          </div>
        </div>
      )}

      <div className="card" style={{ marginBottom: 18 }}>
        <div className="linha">
          <div className="campo">
            <label>Buscar produto</label>
            <select value="" onChange={(e) => {
              const p = dados?.produtos.find((p) => String(p.id) === e.target.value);
              if (p) setInfoProduto(p);
            }}>
              <option value="">— Selecione um produto —</option>
              {(dados?.produtos || []).map((p) => (
                <option key={p.id} value={p.id}>{p.nome}</option>
              ))}
            </select>
          </div>
          <button className={`btn ${soCriticos ? "" : "sec"}`} onClick={() => setSoCriticos(!soCriticos)}>
            {soCriticos ? "Mostrando críticos ⚠️" : "Só críticos"}
          </button>
          <button className="btn sec" onClick={carregar} disabled={carregando}>
            {carregando ? "Carregando..." : "↻ Atualizar"}
          </button>
        </div>
      </div>

      {erro && !modal && !pedindoSenhaQtd && <div className="erro">{erro}</div>}
      {msgOk && <div className="aviso" style={{ marginBottom: 16, borderColor: "rgba(34,197,94,.4)", color: "#86efac", background: "rgba(34,197,94,.1)" }}>✅ {msgOk}</div>}
      {!dados && !erro && <div className="spin" />}

      <div className="produtos-grid">
        {lista.map((p) => {
          const cor = corStatus(p.status);
          const pctBarra = Math.min(100, Math.round((p.quantidade / Math.max(1, p.qtd_maxima)) * 100));
          return (
            <div className="produto-card" key={p.id}>
              <div style={{ display: "flex", justifyContent: "space-between", gap: 8 }}>
                <span className="badge ok" style={{ background: "rgba(99,102,241,.14)", color: "#a5b4fc" }}>{p.categoria}</span>
                <span className={`badge ${cor.badge}`}>{p.status}</span>
              </div>
              <div className="nome">{p.nome}</div>
              <div className="qtd">
                {p.quantidade.toLocaleString("pt-BR")} <small>un. • mín {p.qtd_minima} / máx {p.qtd_maxima}</small>
              </div>
              <div className="barra"><div style={{ width: `${pctBarra}%`, background: cor.barra }} /></div>
              <div className="rodape">
                <span className="muted" style={{ fontSize: 12 }}>{pctBarra}% da capacidade</span>
                <button className="btn mini" onClick={() => abrirEditar(p)}>Editar</button>
              </div>
            </div>
          );
        })}
      </div>

      {pedindoSenha && (
        <div className="modal-fundo" onClick={(e) => e.target === e.currentTarget && setPedindoSenha(false)}>
          <div className="modal" style={{ maxWidth: 300 }}>
            <h2>Verificação ADM</h2>
            <p className="sub">Digite a senha ADM para editar este produto</p>
            {erro && <div className="erro">{erro}</div>}
            <div className="campo">
              <label>Senha ADM *</label>
              <input type="password" value={senhaADM} placeholder="••••••••"
                onChange={(e) => setSenhaADM(e.target.value)}
                onKeyDown={(e) => e.key === "Enter" && confirmarSenhaADM()} />
            </div>
            <div className="acoes">
              <button type="button" className="btn sec" onClick={() => setPedindoSenha(false)}>Cancelar</button>
              <button className="btn" onClick={confirmarSenhaADM}>Confirmar</button>
            </div>
          </div>
        </div>
      )}

      {infoProduto && (
        <div className="modal-fundo" onClick={(e) => e.target === e.currentTarget && setInfoProduto(null)}>
          <div className="modal" style={{ maxWidth: 380 }}>
            <div style={{ display: "flex", justifyContent: "space-between", gap: 8, marginBottom: 10 }}>
              <span className="badge ok" style={{ background: "rgba(99,102,241,.14)", color: "#a5b4fc" }}>{infoProduto.categoria}</span>
              <span className={`badge ${corStatus(infoProduto.status).badge}`}>{infoProduto.status}</span>
            </div>
            <h2>{infoProduto.nome}</h2>
            <div className="qtd" style={{ margin: "10px 0" }}>
              {infoProduto.quantidade.toLocaleString("pt-BR")} <small>un. • mín {infoProduto.qtd_minima} / máx {infoProduto.qtd_maxima}</small>
            </div>
            <div className="barra" style={{ marginBottom: 18 }}>
              <div style={{
                width: `${Math.min(100, Math.round((infoProduto.quantidade / Math.max(1, infoProduto.qtd_maxima)) * 100))}%`,
                background: corStatus(infoProduto.status).barra,
              }} />
            </div>
            <div className="campo"><label>Rua (local do estoque)</label><div>{infoProduto.rua || "—"}</div></div>
            <div className="campo"><label>Prateleira</label><div>{infoProduto.prateleira || "—"}</div></div>
            <div className="campo"><label>Lote</label><div>{infoProduto.lote || "—"}</div></div>
            <div className="acoes">
              <button type="button" className="btn sec" onClick={() => setInfoProduto(null)}>Fechar</button>
              <button className="btn" onClick={() => { setInfoProduto(null); abrirEditar(infoProduto); }}>Editar</button>
            </div>
          </div>
        </div>
      )}

      {modal && (
        <div className="modal-fundo" onClick={(e) => e.target === e.currentTarget && setModal(null)}>
          <form className="modal" onSubmit={editar}>
            <h2>Editar Produto</h2>
            <p className="sub">{modal.nome} — saldo atual: {modal.quantidade} un.</p>
            {erro && <div className="erro">{erro}</div>}
            <div className="campo">
              <label>Nome do Produto *</label>
              <input required value={form.nome_produto}
                onChange={(e) => setForm({ ...form, nome_produto: e.target.value })}
                placeholder="Nome do produto" />
            </div>
            <div className="campo">
              <label>Quantidade máxima *</label>
              <input type="number" min="1" required value={form.quantidade_maxima}
                onChange={(e) => setForm({ ...form, quantidade_maxima: parseInt(e.target.value) || 0 })} />
            </div>
            <div className="campo">
              <label>Quantidade mínima *</label>
              <input type="number" min="1" required value={form.quantidade_minima}
                onChange={(e) => setForm({ ...form, quantidade_minima: parseInt(e.target.value) || 0 })} />
            </div>
            <div className="campo">
              <label>Quantidade caixa por palete *</label>
              <input type="number" min="1" required value={form.caixas_por_palete}
                onChange={(e) => setForm({ ...form, caixas_por_palete: parseInt(e.target.value) || 1 })} />
            </div>
            <div className="campo">
              <label>Rua (local do Estoque)</label>
              <input value={form.rua} onChange={(e) => setForm({ ...form, rua: e.target.value })} placeholder="Ex.: Rua 3" />
            </div>
            <div className="campo">
              <label>Prateleira</label>
              <input value={form.prateleira} onChange={(e) => setForm({ ...form, prateleira: e.target.value })} placeholder="Ex.: A2" />
            </div>
            <div className="campo">
              <label>Lote</label>
              <input value={form.lote} onChange={(e) => setForm({ ...form, lote: e.target.value })} placeholder="Ex.: L20260615" />
            </div>
            <div className="campo">
              <label>Quantidade (estoque atual)</label>
              <div style={{ display: "flex", gap: 8 }}>
                <input type="number" min="0" value={form.quantidade} disabled={!qtdDesbloqueada}
                  style={!qtdDesbloqueada ? { opacity: 0.6 } : undefined}
                  onChange={(e) => setForm({ ...form, quantidade: parseInt(e.target.value) || 0 })} />
                {!qtdDesbloqueada && (
                  <button type="button" className="btn sec mini" onClick={() => { setErroQtd(""); setSenhaQtd(""); setPedindoSenhaQtd(true); }}>
                    🔒 Destravar
                  </button>
                )}
              </div>
              <small className="muted">Atualizado automaticamente pela produção e controle de estoque. Alteração manual exige senha ADM.</small>
            </div>
            <div className="acoes">
              <button type="button" className="btn sec" onClick={() => setModal(null)}>Cancelar</button>
              <button className="btn" disabled={salvando}>{salvando ? "Salvando..." : "Confirmar Edição"}</button>
            </div>
          </form>
        </div>
      )}

      {pedindoSenhaQtd && (
        <div className="modal-fundo" onClick={(e) => e.target === e.currentTarget && setPedindoSenhaQtd(false)}>
          <div className="modal" style={{ maxWidth: 300 }}>
            <h2>Verificação ADM</h2>
            <p className="sub">Digite a senha ADM para liberar a edição da quantidade</p>
            {erroQtd && <div className="erro">{erroQtd}</div>}
            <div className="campo">
              <label>Senha ADM *</label>
              <input type="password" value={senhaQtd} placeholder="••••••••"
                onChange={(e) => setSenhaQtd(e.target.value)}
                onKeyDown={(e) => e.key === "Enter" && confirmarDesbloqueioQtd()} />
            </div>
            <div className="acoes">
              <button type="button" className="btn sec" onClick={() => setPedindoSenhaQtd(false)}>Cancelar</button>
              <button className="btn" onClick={confirmarDesbloqueioQtd}>Confirmar</button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
