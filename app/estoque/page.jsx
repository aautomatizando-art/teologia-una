"use client";
import { useEffect, useMemo, useState } from "react";
import TopBar from "@/components/TopBar";

function corStatus(s) {
  if (s === "ESTOQUE BAIXO") return { badge: "baixo", barra: "#ef4444" };
  if (s === "ESTOQUE ALTO") return { badge: "alto", barra: "#f59e0b" };
  return { badge: "ok", barra: "#22c55e" };
}

export default function PaginaEstoque() {
  const [dados, setDados] = useState(null);
  const [erro, setErro] = useState("");
  const [filtro, setFiltro] = useState("");
  const [soCriticos, setSoCriticos] = useState(false);
  const [modal, setModal] = useState(null); // produto selecionado
  const [form, setForm] = useState({});
  const [salvando, setSalvando] = useState(false);
  const [msgOk, setMsgOk] = useState("");

  async function carregar() {
    try {
      const res = await fetch("/api/estoque");
      const j = await res.json();
      if (!res.ok) setErro(j.error || "Erro ao carregar estoque.");
      else setDados(j);
    } catch {
      setErro("Erro de conexão.");
    }
  }
  useEffect(() => { carregar(); }, []);

  const lista = useMemo(() => {
    let l = dados?.produtos || [];
    if (filtro) l = l.filter((p) => p.nome.toLowerCase().includes(filtro.toLowerCase()));
    if (soCriticos) l = l.filter((p) => p.status !== "NORMAL");
    return l;
  }, [dados, filtro, soCriticos]);

  function abrirReserva(p) {
    const agora = new Date();
    setForm({
      produto_id: p.id,
      quantidade: "",
      pedido: "",
      ordem_producao: "",
      solicitante: "",
      data: agora.toISOString().slice(0, 10),
      hora: agora.toTimeString().slice(0, 5),
    });
    setModal(p);
    setMsgOk("");
    setErro("");
  }

  async function reservar(e) {
    e.preventDefault();
    setSalvando(true);
    setErro("");
    try {
      const res = await fetch("/api/reservas", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(form),
      });
      const j = await res.json();
      if (!res.ok) setErro(j.error || "Erro ao reservar.");
      else {
        setMsgOk(`Reserva registrada! Novo saldo: ${j.nova_quantidade}`);
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

      <div className="card" style={{ marginBottom: 18 }}>
        <div className="linha">
          <div className="campo">
            <label>Buscar produto</label>
            <input value={filtro} onChange={(e) => setFiltro(e.target.value)} placeholder="Ex.: BATATA PALHA..." />
          </div>
          <button className={`btn ${soCriticos ? "" : "sec"}`} onClick={() => setSoCriticos(!soCriticos)}>
            {soCriticos ? "Mostrando críticos ⚠️" : "Só críticos"}
          </button>
          <button className="btn sec" onClick={carregar}>↻ Atualizar</button>
        </div>
      </div>

      {erro && !modal && <div className="erro">{erro}</div>}
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
                <button className="btn mini" onClick={() => abrirReserva(p)}>Reservar</button>
              </div>
            </div>
          );
        })}
      </div>

      {modal && (
        <div className="modal-fundo" onClick={(e) => e.target === e.currentTarget && setModal(null)}>
          <form className="modal" onSubmit={reservar}>
            <h2>Reservar estoque</h2>
            <p className="sub">{modal.nome} — saldo atual: {modal.quantidade} un.</p>
            {erro && <div className="erro">{erro}</div>}
            <div className="campo">
              <label>Reservar quantidade *</label>
              <input type="number" min="1" max={modal.quantidade} required value={form.quantidade}
                onChange={(e) => setForm({ ...form, quantidade: e.target.value })} />
            </div>
            <div className="campo">
              <label>Pedido</label>
              <input value={form.pedido} placeholder="Ex.: PED-501"
                onChange={(e) => setForm({ ...form, pedido: e.target.value })} />
            </div>
            <div className="campo">
              <label>Ordem de produção</label>
              <input value={form.ordem_producao} placeholder="Ex.: OP-1001"
                onChange={(e) => setForm({ ...form, ordem_producao: e.target.value })} />
            </div>
            <div className="campo">
              <label>Pessoa que está solicitando *</label>
              <input required value={form.solicitante} placeholder="Nome completo"
                onChange={(e) => setForm({ ...form, solicitante: e.target.value })} />
            </div>
            <div className="linha">
              <div className="campo">
                <label>Data *</label>
                <input type="date" required value={form.data}
                  onChange={(e) => setForm({ ...form, data: e.target.value })} />
              </div>
              <div className="campo">
                <label>Hora *</label>
                <input type="time" required value={form.hora}
                  onChange={(e) => setForm({ ...form, hora: e.target.value })} />
              </div>
            </div>
            <div className="acoes">
              <button type="button" className="btn sec" onClick={() => setModal(null)}>Cancelar</button>
              <button className="btn" disabled={salvando}>{salvando ? "Reservando..." : "Confirmar reserva"}</button>
            </div>
          </form>
        </div>
      )}
    </div>
  );
}
