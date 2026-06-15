"use client";

import { useEffect, useState } from "react";
export const dynamic = "force-dynamic";
import TopBar from "@/components/TopBar";
import {
  ResponsiveContainer, LineChart, Line, XAxis, YAxis, Tooltip, CartesianGrid,
  BarChart, Bar, PieChart, Pie, Cell, Legend,
} from "recharts";

const CORES = ["#6366f1", "#8b5cf6", "#06b6d4", "#22c55e", "#f59e0b", "#ef4444", "#ec4899", "#14b8a6"];
const TOOLTIP = { background: "#1a2347", border: "1px solid #26305c", borderRadius: 10, color: "#e8ecf8" };

const INSUMOS_CONFIG = [
  { chave: "kg_batata_por_caixa", label: "Batata", unidade: "kg" },
  { chave: "caixa_por_caixa", label: "Caixa", unidade: "cx" },
  { chave: "kg_filme_bopp_por_caixa", label: "Filme BOPP", unidade: "kg" },
  { chave: "kg_condimento_por_caixa", label: "Condimento", unidade: "kg" },
  { chave: "kg_oleo_por_caixa", label: "Óleo", unidade: "kg" },
  { chave: "cm_fita_adesiva_por_caixa", label: "Fita Adesiva", unidade: "m", fator: 0.01 },
];

export default function PaginaProducao() {
  const [op, setOp] = useState("");
  const [dados, setDados] = useState(null);
  const [ordens, setOrdens] = useState([]);
  const [erro, setErro] = useState("");
  const [carregando, setCarregando] = useState(false);
  const [reg, setReg] = useState({ acao: "paletes", paletes: "", tipo: "", quantidade: "" });

  // Bloco de produção por pedido
  const [pedidos, setPedidos] = useState([]);
  const [pedidoSelecionado, setPedidoSelecionado] = useState(null);
  const [pedidoForm, setPedidoForm] = useState({ quantidade: "", senha: "", numero_lote: "", rua: "", prateleira: "" });
  const [finalizandoPedido, setFinalizandoPedido] = useState(false);
  const [okPedido, setOkPedido] = useState("");
  const [erroPedido, setErroPedido] = useState("");

  // Dropdown de OPs
  const [dropdownAberto, setDropdownAberto] = useState(false);
  const ordensAbertas = ordens.filter((x) => x.status === "ABERTA");

  useEffect(() => {
    fetch("/api/producao")
      .then((r) => r.json())
      .then((j) => {
        setOrdens(j.ordens || []);
        if (j.ordens?.length) {
          setOp(j.ordens[0].numero);
          buscar(j.ordens[0].numero);
        }
      })
      .catch(() => {});
  }, []);

  async function buscar(numero) {
    const alvo = (numero || op).trim();
    if (!alvo) return;
    setCarregando(true);
    setErro("");
    try {
      const opRes = await fetch(`/api/producao?op=${encodeURIComponent(alvo)}`);
      const opData = await opRes.json();
      if (!opRes.ok) {
        setErro(opData.error || "Erro ao buscar ordem.");
        setDados(null);
        return;
      }
      setDados(opData);

      // Pega o ID da ordem para buscar pedidos
      const ordens = await fetch("/api/ordens").then((r) => r.json());
      const ordem = ordens.ordens?.find((o) => o.numero === alvo);
      if (ordem) {
        const pedRes = await fetch(`/api/producao/pedido?ordem_id=${ordem.id}`);
        const pedData = await pedRes.json();
        if (pedRes.ok) {
          const novosPedidos = pedData.pedidos || [];
          setPedidos(novosPedidos);
          // Mantém o pedido selecionado, mas com os dados (produzido/insumos) atualizados
          setPedidoSelecionado((prev) => (prev ? novosPedidos.find((p) => p.id === prev.id) || null : null));
        }
      } else {
        setPedidos([]);
        setPedidoSelecionado(null);
      }
    } catch {
      setErro("Erro de conexão.");
    } finally {
      setCarregando(false);
    }
  }

  async function registrar(e) {
    e.preventDefault();
    const body = { op, acao: reg.acao };
    if (reg.acao === "paletes") body.paletes = reg.paletes;
    else {
      body.tipo = reg.tipo;
      body.quantidade = reg.quantidade;
    }
    const res = await fetch("/api/producao", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    });
    const j = await res.json();
    if (!res.ok) return setErro(j.error || "Erro ao registrar.");
    setReg({ acao: reg.acao, paletes: "", tipo: "", quantidade: "" });
    buscar();
  }

  async function finalizarPedido(e) {
    e.preventDefault();
    if (!pedidoSelecionado) return;
    setFinalizandoPedido(true);
    setErroPedido("");
    setOkPedido("");
    try {
      const res = await fetch("/api/producao/pedido", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          pedido_id: pedidoSelecionado.id,
          quantidade: Number(pedidoForm.quantidade),
          senha: pedidoForm.senha,
          numero_lote: pedidoForm.numero_lote,
          rua: pedidoForm.rua,
          prateleira: pedidoForm.prateleira,
        }),
      });
      const j = await res.json();
      if (!res.ok) {
        setErroPedido(j.error || "Erro ao registrar produção.");
      } else {
        setOkPedido(`✅ ${j.status} — ${j.novaQtd} caixas registradas`);
        setPedidoForm({ quantidade: "", senha: "", numero_lote: "", rua: "", prateleira: "" });
        // Aguarda um pouco para garantir que o banco foi atualizado
        setTimeout(() => {
          buscar();
        }, 500);
      }
    } catch {
      setErroPedido("Erro de conexão.");
    } finally {
      setFinalizandoPedido(false);
    }
  }

  const o = dados?.ordem;
  const pct = o?.percentual ?? 0;
  const donut = [
    { name: "Produzido", value: pct },
    { name: "Restante", value: Math.max(0, 100 - pct) },
  ];
  const linha = (dados?.registros || []).map((r) => {
    const d = new Date(r.registrado_em);
    return { hora: d.toLocaleString("pt-BR", { day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit" }), paletes: r.paletes };
  });
  let acc = 0;
  const linhaAcum = linha.map((p) => ({ ...p, acumulado: (acc += p.paletes) }));
  const pedidosDados = (dados?.pedidos || []).map((p) => ({ name: p.codigo_pedido, value: p.qtd_planejada }));

  return (
    <div className="shell">
      <TopBar />

      <div className="card" style={{ marginBottom: 18 }}>
        <h3>🔎 Ordem de Produção</h3>
        <div className="linha">
          <div className="campo" style={{ position: "relative", flex: 1 }}>
            <label>Número da ordem de produção</label>
            <input
              value={op}
              onChange={(e) => setOp(e.target.value)}
              onFocus={() => setDropdownAberto(true)}
              onBlur={() => setTimeout(() => setDropdownAberto(false), 200)}
              placeholder="Ex.: OP-1001"
              onKeyDown={(e) => e.key === "Enter" && buscar()}
            />
            {dropdownAberto && ordensAbertas.length > 0 && (
              <div style={{
                position: "absolute", top: "100%", left: 0, right: 0,
                background: "#0b1020", border: "1px solid #26305c", borderRadius: 8,
                marginTop: 4, maxHeight: 250, overflowY: "auto", zIndex: 10
              }}>
                {ordensAbertas.map((ordem) => (
                  <div
                    key={ordem.numero}
                    onClick={() => {
                      setOp(ordem.numero);
                      buscar(ordem.numero);
                      setDropdownAberto(false);
                    }}
                    style={{
                      padding: "12px 16px", borderBottom: "1px solid #26305c",
                      cursor: "pointer", color: "#a5b4fc", fontSize: 14,
                      transition: "background 0.2s"
                    }}
                    onMouseEnter={(e) => e.target.style.background = "#1a2347"}
                    onMouseLeave={(e) => e.target.style.background = "transparent"}
                  >
                    <strong>{ordem.numero}</strong> — {ordem.produto || "—"} ({ordem.produzido}/{ordem.meta_paletes} paletes)
                  </div>
                ))}
              </div>
            )}
          </div>
          <button className="btn" onClick={() => buscar()} disabled={carregando}>
            {carregando ? "Buscando..." : "Buscar"}
          </button>
        </div>
        {erro && <div className="erro mt">{erro}</div>}
      </div>

      {carregando && <div className="spin" />}

      {o && (
        <>
          {(() => {
            const produzidoCx = (o.produzido || 0) * (o.caixasPorPalete || 1);
            const metaAlcancada = o.meta_paletes ? produzidoCx >= o.meta_paletes : false;
            const estiloMeta = metaAlcancada ? { borderColor: "rgba(34,197,94,.4)", background: "rgba(34,197,94,.1)" } : undefined;
            const corMeta = metaAlcancada ? { color: "#86efac" } : undefined;
            return (
              <div className="grid g4" style={{ marginBottom: 18 }}>
                <div className="card"><h3>Produto</h3><div style={{ fontWeight: 700, fontSize: 15 }}>{o.produto || "—"}</div></div>
                <div className="card" style={estiloMeta}>
                  <h3>{metaAlcancada ? "Meta Alcançada" : "Meta"}</h3>
                  <div className="kpi" style={corMeta}>{o.meta_paletes} <small>caixas</small></div>
                </div>
                <div className="card">
                  <h3>Produzido</h3>
                  <div style={{ display: "flex", gap: 16, flexWrap: "wrap" }}>
                    <div className="kpi" style={{ fontSize: 26 }}>{produzidoCx.toLocaleString("pt-BR")} <small>cx</small></div>
                    <div className="kpi" style={{ fontSize: 26 }}>{(o.produzido || 0).toLocaleString("pt-BR")} <small>paletes</small></div>
                  </div>
                </div>
                <div className="card"><h3>Status</h3><span className={`badge ${o.status === "ABERTA" ? "ok" : "alto"}`}>{o.status}</span></div>
              </div>
            );
          })()}

          {/* ── BLOCO DE PRODUÇÃO POR PEDIDO ── */}
          {pedidos.length > 0 && (
            <div className="grid g2" style={{ marginBottom: 18, gridTemplateColumns: "minmax(0, 1fr) minmax(0, 1fr)" }}>
            <div className="card">
              <h3>📦 Produzir por Pedido</h3>
              {erroPedido && <div className="erro">{erroPedido}</div>}
              {okPedido && <div className="aviso" style={{ marginBottom: 14, borderColor: "rgba(34,197,94,.4)", color: "#86efac", background: "rgba(34,197,94,.1)" }}>{okPedido}</div>}

              {!pedidoSelecionado ? (
                <div>
                  <p className="muted" style={{ marginBottom: 12 }}>Selecione um pedido para registrar produção:</p>
                  <div style={{ display: "flex", gap: 10, flexWrap: "wrap" }}>
                    {pedidos.map((p) => (
                      <button
                        key={p.id}
                        className="btn sec"
                        onClick={() => {
                          setPedidoSelecionado(p);
                          setPedidoForm({ quantidade: "", senha: "" });
                        }}
                      >
                        {p.status === "FINALIZADO" ? "✅" : "🔄"} {p.codigo} ({p.produzido}/{p.qtd_planejada} caixas)
                      </button>
                    ))}
                  </div>
                </div>
              ) : (
                <>
                  <div className="grid g4" style={{ marginBottom: 16 }}>
                    <div className="card"><h3>Produto</h3><div style={{ fontSize: 13, fontWeight: 700 }}>{o.produto}</div></div>
                    <div className="card"><h3>Meta</h3><div className="kpi">{pedidoSelecionado.qtd_planejada} <small>caixas</small></div></div>
                    <div className="card"><h3>Produzido</h3><div className="kpi">{pedidoSelecionado.produzido} <small>caixas</small></div></div>
                    <div className="card"><h3>Status</h3><span className={`badge ${pedidoSelecionado.status === "FINALIZADO" ? "ok" : "alto"}`}>{pedidoSelecionado.status}</span></div>
                  </div>

                  <form onSubmit={finalizarPedido} style={{ display: "flex", gap: 12, alignItems: "flex-end", flexWrap: "wrap" }}>
                    <div className="campo">
                      <label>Caixas a registrar {pedidoSelecionado.status === "FINALIZADO" ? "(Pedido finalizado)" : ""}</label>
                      <input
                        type="number"
                        min="0"
                        step="1"
                        max={pedidoSelecionado.qtd_planejada - pedidoSelecionado.produzido}
                        required
                        disabled={pedidoSelecionado.status === "FINALIZADO"}
                        value={pedidoForm.quantidade}
                        onChange={(e) => setPedidoForm({ ...pedidoForm, quantidade: e.target.value })}
                      />
                    </div>
                    <div className="campo">
                      <label>Senha da produção *</label>
                      <input
                        type="password"
                        required
                        disabled={pedidoSelecionado.status === "FINALIZADO"}
                        value={pedidoForm.senha}
                        placeholder="••••••••"
                        onChange={(e) => setPedidoForm({ ...pedidoForm, senha: e.target.value })}
                      />
                    </div>
                    <div className="campo">
                      <label>Numero de Lote *</label>
                      <input
                        type="text"
                        required
                        disabled={pedidoSelecionado.status === "FINALIZADO"}
                        value={pedidoForm.numero_lote}
                        placeholder="Ex: L-001"
                        onChange={(e) => setPedidoForm({ ...pedidoForm, numero_lote: e.target.value })}
                      />
                    </div>
                    <div className="campo">
                      <label>Rua (Local do Estoque) *</label>
                      <input
                        type="text"
                        required
                        disabled={pedidoSelecionado.status === "FINALIZADO"}
                        value={pedidoForm.rua}
                        placeholder="Ex: A"
                        onChange={(e) => setPedidoForm({ ...pedidoForm, rua: e.target.value })}
                      />
                    </div>
                    <div className="campo">
                      <label>Prateleira *</label>
                      <input
                        type="text"
                        required
                        disabled={pedidoSelecionado.status === "FINALIZADO"}
                        value={pedidoForm.prateleira}
                        placeholder="Ex: 1"
                        onChange={(e) => setPedidoForm({ ...pedidoForm, prateleira: e.target.value })}
                      />
                    </div>
                    <button
                      className="btn"
                      disabled={finalizandoPedido || pedidoSelecionado.status === "FINALIZADO"}
                      title={pedidoSelecionado.status === "FINALIZADO" ? "Pedido já foi finalizado" : ""}
                    >
                      {finalizandoPedido ? "Registrando..." : "✅ Finalizar Pedido"}
                    </button>
                    <button
                      type="button"
                      className="btn sec"
                      onClick={() => setPedidoSelecionado(null)}
                    >
                      ← Voltar
                    </button>
                  </form>
                </>
              )}

              {/* Tabela com status de cada pedido */}
              <div style={{ marginTop: 18, overflowX: "auto" }}>
                <h3 style={{ fontSize: 13, color: "#8b96c0", marginBottom: 10 }}>Status dos pedidos da OP:</h3>
                <table className="tab">
                  <thead>
                    <tr><th>Pedido</th><th>Planejado</th><th>Produzido</th><th>Estoque</th><th>Total</th><th>%</th><th>Status</th></tr>
                  </thead>
                  <tbody>
                    {pedidos.map((p) => (
                      <tr key={p.id}>
                        <td style={{ fontWeight: 700 }}>{p.codigo}</td>
                        <td>{p.qtd_planejada} caixas</td>
                        <td>{p.produzido} caixas</td>
                        <td style={{ color: "#86efac" }}>{p.estoque} caixas</td>
                        <td style={{ fontWeight: 700, color: "#fbbf24" }}>{p.totalDisponivel} caixas</td>
                        <td>{p.percentual}%</td>
                        <td><span className={`badge ${p.status === "FINALIZADO" ? "ok" : "alto"}`}>{p.status}</span></td>
                      </tr>
                    ))}
                  </tbody>
                </table>
              </div>
            </div>

            <div className="card">
              <h3>🧪 Insumos do Pedido</h3>
              {!pedidoSelecionado ? (
                <p className="muted">Selecione um pedido para ver o consumo de insumos.</p>
              ) : (() => {
                const caixasPorPalete = pedidoSelecionado.caixasPorPalete || 0;
                const produzidoCaixas = (o.produzido || 0) * caixasPorPalete;
                return (
                <>
                  <p className="sub" style={{ marginTop: -8, marginBottom: 14 }}>
                    Consumo estimado para {produzidoCaixas.toLocaleString("pt-BR")} caixas produzidas ({(o.produzido || 0).toLocaleString("pt-BR")} paletes × {caixasPorPalete.toLocaleString("pt-BR")} cx/palete)
                  </p>
                  <div style={{ display: "grid", gridTemplateColumns: "repeat(2, 1fr)", gap: 12 }}>
                    {INSUMOS_CONFIG.map((item) => {
                      const taxa = pedidoSelecionado.insumos?.[item.chave] || 0;
                      const valor = taxa * produzidoCaixas * (item.fator ?? 1);
                      return (
                        <div key={item.chave} style={{ background: "var(--bg-2)", border: "1px solid var(--border)", borderRadius: 10, padding: "12px 14px" }}>
                          <div className="muted" style={{ fontSize: 12, marginBottom: 4 }}>{item.label}</div>
                          <div style={{ fontSize: 20, fontWeight: 800 }}>
                            {valor.toLocaleString("pt-BR", { maximumFractionDigits: 2 })} <small style={{ fontSize: 12, color: "var(--muted)", fontWeight: 500 }}>{item.unidade}</small>
                          </div>
                        </div>
                      );
                    })}
                  </div>
                </>
                );
              })()}
            </div>
            </div>
          )}

          <div className="grid g2" style={{ marginBottom: 18 }}>
            <div className="card">
              <h3>🎯 Percentual de produção em relação à OP</h3>
              <div style={{ position: "relative", height: 260 }}>
                <ResponsiveContainer>
                  <PieChart>
                    <Pie data={donut} dataKey="value" innerRadius="68%" outerRadius="90%" startAngle={90} endAngle={-270} stroke="none">
                      <Cell fill="#6366f1" />
                      <Cell fill="#1a2347" />
                    </Pie>
                  </PieChart>
                </ResponsiveContainer>
                <div style={{ position: "absolute", inset: 0, display: "grid", placeItems: "center", pointerEvents: "none" }}>
                  <div className="center">
                    <div style={{ fontSize: 42, fontWeight: 800 }}>{pct}%</div>
                    <div className="muted" style={{ fontSize: 13 }}>{o.produzido} de {o.meta_paletes} paletes</div>
                  </div>
                </div>
              </div>
            </div>

            <div className="card">
              <h3>📈 Paletes produzidos × tempo</h3>
              <div style={{ height: 260 }}>
                <ResponsiveContainer>
                  <LineChart data={linhaAcum} margin={{ top: 8, right: 12, left: -16, bottom: 0 }}>
                    <CartesianGrid stroke="#26305c" strokeDasharray="3 3" />
                    <XAxis dataKey="hora" tick={{ fill: "#8b96c0", fontSize: 11 }} />
                    <YAxis tick={{ fill: "#8b96c0", fontSize: 11 }} />
                    <Tooltip contentStyle={TOOLTIP} />
                    <Legend />
                    <Line type="monotone" dataKey="paletes" name="Paletes/registro" stroke="#06b6d4" strokeWidth={2} dot={{ r: 3 }} />
                    <Line type="monotone" dataKey="acumulado" name="Acumulado" stroke="#8b5cf6" strokeWidth={2.5} dot={false} />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </div>
          </div>

          <div className="grid" style={{ gridTemplateColumns: pedidosDados.length > 1 ? "1fr 1fr 1fr" : "1fr 1fr", marginBottom: 18 }}>
            {pedidosDados.length > 1 && (
              <div className="card">
                <h3>🧾 Percentual por pedido da OP</h3>
                <div style={{ height: 240 }}>
                  <ResponsiveContainer>
                    <PieChart>
                      <Pie data={pedidosDados} dataKey="value" nameKey="name" outerRadius="80%" stroke="none"
                        label={({ name, percent }) => `${name} ${(percent * 100).toFixed(0)}%`}>
                        {pedidosDados.map((_, i) => <Cell key={i} fill={CORES[i % CORES.length]} />)}
                      </Pie>
                      <Tooltip contentStyle={TOOLTIP} />
                    </PieChart>
                  </ResponsiveContainer>
                </div>
              </div>
            )}

            <div className="card">
              <h3>📦 Perdas de embalagem</h3>
              <div style={{ height: 240 }}>
                <ResponsiveContainer>
                  <BarChart data={dados.perdas} margin={{ top: 8, right: 12, left: -16, bottom: 0 }}>
                    <CartesianGrid stroke="#26305c" strokeDasharray="3 3" />
                    <XAxis dataKey="tipo" tick={{ fill: "#8b96c0", fontSize: 10 }} interval={0} />
                    <YAxis tick={{ fill: "#8b96c0", fontSize: 11 }} />
                    <Tooltip contentStyle={TOOLTIP} cursor={{ fill: "rgba(99,102,241,0.08)" }} />
                    <Bar dataKey="quantidade" name="Qtd" fill="#f59e0b" radius={[6, 6, 0, 0]} />
                  </BarChart>
                </ResponsiveContainer>
              </div>
            </div>

            <div className="card">
              <h3>🧪 Problemas qualitativos</h3>
              <div style={{ height: 240 }}>
                <ResponsiveContainer>
                  <BarChart data={dados.problemas} margin={{ top: 8, right: 12, left: -16, bottom: 0 }}>
                    <CartesianGrid stroke="#26305c" strokeDasharray="3 3" />
                    <XAxis dataKey="tipo" tick={{ fill: "#8b96c0", fontSize: 10 }} interval={0} />
                    <YAxis tick={{ fill: "#8b96c0", fontSize: 11 }} />
                    <Tooltip contentStyle={TOOLTIP} cursor={{ fill: "rgba(239,68,68,0.08)" }} />
                    <Bar dataKey="quantidade" name="Qtd" fill="#ef4444" radius={[6, 6, 0, 0]} />
                  </BarChart>
                </ResponsiveContainer>
              </div>
            </div>
          </div>

          <div className="card">
            <h3>✍️ Registrar na OP {o.numero}</h3>
            <form className="linha" onSubmit={registrar}>
              <div className="campo">
                <label>Tipo de registro</label>
                <select value={reg.acao} onChange={(e) => setReg({ ...reg, acao: e.target.value })}>
                  <option value="paletes">Paletes produzidos</option>
                  <option value="perda">Perda de embalagem</option>
                  <option value="problema">Problema qualitativo</option>
                </select>
              </div>
              {reg.acao === "paletes" ? (
                <div className="campo">
                  <label>Qtd de paletes</label>
                  <input type="number" min="1" required value={reg.paletes}
                    onChange={(e) => setReg({ ...reg, paletes: e.target.value })} />
                </div>
              ) : (
                <>
                  <div className="campo">
                    <label>Descrição / tipo</label>
                    <input required value={reg.tipo} placeholder={reg.acao === "perda" ? "Ex.: Filme rasgado" : "Ex.: Peso fora do padrão"}
                      onChange={(e) => setReg({ ...reg, tipo: e.target.value })} />
                  </div>
                  <div className="campo">
                    <label>Quantidade</label>
                    <input type="number" min="1" required value={reg.quantidade}
                      onChange={(e) => setReg({ ...reg, quantidade: e.target.value })} />
                  </div>
                </>
              )}
              <button className="btn">Registrar</button>
            </form>
          </div>
        </>
      )}

      {!o && !carregando && !erro && (
        <div className="card center muted" style={{ padding: 50 }}>
          Digite o número de uma ordem de produção para visualizar os gráficos.
        </div>
      )}
    </div>
  );
}
