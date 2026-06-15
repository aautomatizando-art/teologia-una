"use client";
import { useEffect, useState } from "react";
import TopBar from "@/components/TopBar";

const COLUNAS = [
  { chave: "EMERGENCIAL", classe: "emergencial", emoji: "🟣" },
  { chave: "URGENTE", classe: "urgente", emoji: "🔴" },
  { chave: "MODERADO", classe: "moderado", emoji: "🟢" },
];

const EMOJI_CRIT = { EMERGENCIAL: "🟣", URGENTE: "🔴", MODERADO: "🟢" };

export default function PaginaExpedicion() {
  const [pedidosEstoque, setPedidosEstoque] = useState([]);
  const [pedidosCarregamento, setPedidosCarregamento] = useState([]);
  const [pedidosCarregados, setPedidosCarregados] = useState([]);
  const [erro, setErro] = useState("");
  const [carregando, setCarregando] = useState(false);
  const [filtroData, setFiltroData] = useState("");

  // Modal Retirada
  const [modalRetirada, setModalRetirada] = useState(null);
  const [retiradaForm, setRetiradaForm] = useState({ nome: "", senha: "" });
  const [retirandoEstoque, setRetirandoEstoque] = useState(false);

  // Modal Carregamento
  const [modalCarregamento, setModalCarregamento] = useState(null);
  const [carregamentoForm, setCarregamentoForm] = useState({
    responsavel_expedicao: "",
    nome_motorista: "",
    nome_ajudante: "",
    veiculo: "",
  });
  const [finalizandoCarregamento, setFinalizandoCarregamento] = useState(false);

  useEffect(() => {
    carregar();
  }, []);

  // Buscar data/hora da retirada para pedidos em carregamento
  useEffect(() => {
    if (pedidosCarregamento.length > 0) {
      Promise.all(
        pedidosCarregamento.map((p) =>
          fetch(`/api/expedicion/retirada-info?codigo_pedido=PD-${p.id}`)
            .then((r) => r.json())
            .then((d) => ({ id: p.id, ...d }))
        )
      ).then((infos) => {
        setPedidosCarregamento((prev) =>
          prev.map((p) => ({
            ...p,
            data_retirada: infos.find((i) => i.id === p.id)?.data,
            hora_retirada: infos.find((i) => i.id === p.id)?.hora,
          }))
        );
      });
    }
  }, [pedidosCarregamento.length]);

  // Buscar data/hora da finalização de carregamento para pedidos em rota
  useEffect(() => {
    if (pedidosCarregados.length > 0) {
      Promise.all(
        pedidosCarregados.map((p) =>
          fetch(`/api/expedicion/carregamento-info?codigo_pedido=PD-${p.id}`)
            .then((r) => r.json())
            .then((d) => ({ id: p.id, ...d }))
        )
      ).then((infos) => {
        setPedidosCarregados((prev) =>
          prev.map((p) => ({
            ...p,
            data_carregamento: infos.find((i) => i.id === p.id)?.data,
            hora_carregamento: infos.find((i) => i.id === p.id)?.hora,
            conferido_por: infos.find((i) => i.id === p.id)?.responsavel_expedicao,
            nome_motorista_carregamento: infos.find((i) => i.id === p.id)?.nome_motorista,
            nome_ajudante_carregamento: infos.find((i) => i.id === p.id)?.nome_ajudante,
            veiculo_carregamento: infos.find((i) => i.id === p.id)?.veiculo,
          }))
        );
      });
    }
  }, [pedidosCarregados.length]);

  async function carregar() {
    setCarregando(true);
    setErro("");
    try {
      const [resEstoque, resCarregamento, resCarregados] = await Promise.all([
        fetch("/api/expedicion/listar?status=3"),
        fetch("/api/expedicion/listar?status=4"),
        fetch("/api/expedicion/listar?status=5"),
      ]);

      const dataEstoque = await resEstoque.json();
      const dataCarregamento = await resCarregamento.json();
      const dataCarregados = await resCarregados.json();

      setPedidosEstoque(dataEstoque.pedidos || []);
      setPedidosCarregamento(dataCarregamento.pedidos || []);
      setPedidosCarregados(dataCarregados.pedidos || []);
    } catch {
      setErro("Erro ao carregar pedidos.");
    } finally {
      setCarregando(false);
    }
  }

  async function retirarEstoque() {
    if (!modalRetirada) return;
    setRetirandoEstoque(true);
    try {
      const res = await fetch("/api/expedicion/retirar", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          codigo_pedido: modalRetirada.codigo_pedido,
          quantidade: modalRetirada.quantidade,
          nome: retiradaForm.nome,
          senha: retiradaForm.senha,
        }),
      });

      const data = await res.json();
      if (!res.ok) {
        alert(`Erro: ${data.error}`);
      } else {
        alert("✅ Retirada registrada!");
        setModalRetirada(null);
        setRetiradaForm({ nome: "", senha: "" });
        carregar();
      }
    } catch {
      alert("Erro de conexão.");
    } finally {
      setRetirandoEstoque(false);
    }
  }

  async function finalizarCarregamento() {
    if (!modalCarregamento) return;
    setFinalizandoCarregamento(true);
    try {
      const res = await fetch("/api/expedicion/carregar", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({
          codigo_pedido: modalCarregamento.codigo_pedido,
          quantidade_carregada: modalCarregamento.quantidade,
          responsavel_expedicao: carregamentoForm.responsavel_expedicao,
          nome_motorista: carregamentoForm.nome_motorista,
          nome_ajudante: carregamentoForm.nome_ajudante,
          veiculo: carregamentoForm.veiculo,
        }),
      });

      const data = await res.json();
      if (!res.ok) {
        alert(`Erro: ${data.error}`);
      } else {
        alert("✅ Carregamento finalizado!");
        setModalCarregamento(null);
        setCarregamentoForm({
          responsavel_expedicao: "",
          nome_motorista: "",
          nome_ajudante: "",
          veiculo: "",
        });
        carregar();
      }
    } catch {
      alert("Erro de conexão.");
    } finally {
      setFinalizandoCarregamento(false);
    }
  }

  return (
    <div className="shell">
      <TopBar />

      {/* KANBAN - ESTOQUE */}
      <div className="card" style={{ marginBottom: 28 }}>
        <h3>📦 Pedidos em Estoque (Aguardando Retirada)</h3>
        {carregando ? (
          <div className="spin" />
        ) : (
          <div className="kanban">
            {COLUNAS.map((c) => {
              const itens = pedidosEstoque.filter((p) => p.criticidade === c.chave);
              return (
                <div key={c.chave} className={`coluna ${c.classe}`}>
                  <div className="cab">
                    <span>{c.emoji} {c.chave}</span>
                    <span>{itens.length}</span>
                  </div>
                  <div className="corpo">
                    {itens.length === 0 && (
                      <span className="muted center" style={{ fontSize: 13, padding: 20 }}>
                        Nenhum pedido
                      </span>
                    )}
                    {itens.map((p) => (
                      <div
                        key={p.id}
                        className="ticket"
                        onClick={() => {
                          setModalRetirada({
                            ...p,
                            quantidade: p.quantidade,
                            codigo_pedido: `PD-${p.id}`,
                          });
                          setRetiradaForm({ nome: p.solicitante, senha: "" });
                        }}
                        style={{ cursor: "pointer" }}
                      >
                        <span className="prod">#{p.id} • {p.produtos?.nome}</span>
                        <span className="meta">Qtd: {p.quantidade} • {p.solicitante}</span>
                        {p.localizacao && (
                          <span className="meta" style={{ fontSize: 12, color: "#a5b4fc" }}>
                            📍 Lote: {p.localizacao.numero_lote} | Rua: {p.localizacao.rua} | Prat: {p.localizacao.prateleira}
                          </span>
                        )}
                      </div>
                    ))}
                  </div>
                </div>
              );
            })}
          </div>
        )}
      </div>

      {/* AGUARDANDO CARREGAMENTO */}
      <div className="card">
        <h3>🚚 Aguardando Carregamento</h3>
        {pedidosCarregamento.length === 0 ? (
          <p className="muted">Nenhum pedido aguardando carregamento.</p>
        ) : (
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(300px, 1fr))", gap: 12 }}>
            {pedidosCarregamento.map((p) => (
              <div
                key={p.id}
                className="card"
                style={{
                  background: "rgba(99, 102, 241, .1)",
                  borderLeft: "4px solid #6366f1",
                  cursor: "pointer",
                  transition: "all 0.2s",
                }}
                onMouseEnter={(e) => (e.currentTarget.style.background = "rgba(99, 102, 241, .2)")}
                onMouseLeave={(e) => (e.currentTarget.style.background = "rgba(99, 102, 241, .1)")}
                onClick={() => {
                  setModalCarregamento({
                    ...p,
                    codigo_pedido: `PD-${p.id}`,
                  });
                  setCarregamentoForm({
                    responsavel_expedicao: "",
                    nome_motorista: "",
                    nome_ajudante: "",
                    veiculo: "",
                  });
                }}
              >
                <h4 style={{ marginTop: 0, marginBottom: 8 }}>
                  {EMOJI_CRIT[p.criticidade]} Pedido #{p.id}
                </h4>
                <p style={{ margin: "4px 0", fontSize: 13 }}>
                  <strong>{p.produtos?.nome}</strong>
                </p>
                <p style={{ margin: "4px 0", fontSize: 13, color: "#a5b4fc" }}>
                  Quantidade: {p.quantidade} un.
                </p>
                <p style={{ margin: "4px 0", fontSize: 13, color: "#a5b4fc" }}>
                  Solicitante: {p.solicitante}
                </p>
                {p.data_retirada && p.hora_retirada && (
                  <p style={{ margin: "4px 0", fontSize: 12, color: "#86efac" }}>
                    📅 Saída: {p.data_retirada} às {p.hora_retirada}
                  </p>
                )}
                <button
                  className="btn"
                  style={{ marginTop: 8, width: "100%" }}
                  onClick={(e) => {
                    e.stopPropagation();
                    setModalCarregamento({ ...p, codigo_pedido: `PD-${p.id}` });
                  }}
                >
                  📋 Carregar
                </button>
              </div>
            ))}
          </div>
        )}
      </div>

      {/* MODAL RETIRADA */}
      {modalRetirada && (
        <div
          style={{
            position: "fixed",
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            background: "rgba(0,0,0,.7)",
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            zIndex: 1000,
          }}
          onClick={() => setModalRetirada(null)}
        >
          <div
            className="card"
            style={{ maxWidth: 500, width: "90%", maxHeight: "90vh", overflowY: "auto" }}
            onClick={(e) => e.stopPropagation()}
          >
            <h3>📦 Retirada de Material</h3>

            <div className="campo">
              <label>Pedido</label>
              <div style={{ fontWeight: 700 }}>#{modalRetirada.id}</div>
            </div>

            <div className="campo">
              <label>Produto</label>
              <div>{modalRetirada.produtos?.nome}</div>
            </div>

            <div className="campo">
              <label>Quantidade</label>
              <div style={{ fontWeight: 700, fontSize: 18 }}>{modalRetirada.quantidade} un.</div>
            </div>

            {modalRetirada.localizacao && (
              <>
                <div className="campo">
                  <label>Numero de Lote</label>
                  <div>{modalRetirada.localizacao.numero_lote}</div>
                </div>
                <div className="campo">
                  <label>Rua (Local do Estoque)</label>
                  <div>{modalRetirada.localizacao.rua}</div>
                </div>
                <div className="campo">
                  <label>Prateleira</label>
                  <div>{modalRetirada.localizacao.prateleira}</div>
                </div>
              </>
            )}

            <div className="campo">
              <label>Nome *</label>
              <input
                value={retiradaForm.nome}
                onChange={(e) => setRetiradaForm({ ...retiradaForm, nome: e.target.value })}
                placeholder="Nome de quem retira"
                required
              />
            </div>

            <div className="campo">
              <label>Senha de Expedição *</label>
              <input
                type="password"
                value={retiradaForm.senha}
                onChange={(e) => setRetiradaForm({ ...retiradaForm, senha: e.target.value })}
                placeholder="••••••••"
                required
              />
            </div>

            <div style={{ display: "flex", gap: 8 }}>
              <button
                className="btn"
                onClick={retirarEstoque}
                disabled={retirandoEstoque || !retiradaForm.nome || !retiradaForm.senha}
                style={{ flex: 1 }}
              >
                {retirandoEstoque ? "Processando..." : "✅ Saída de Estoque"}
              </button>
              <button
                className="btn sec"
                onClick={() => setModalRetirada(null)}
                disabled={retirandoEstoque}
                style={{ flex: 1 }}
              >
                ✕ Fechar
              </button>
            </div>
          </div>
        </div>
      )}

      {/* PEDIDOS CARREGADOS */}
      <div className="card" style={{ marginBottom: 28 }}>
        <h3>✅ Pedidos Carregados (Em Rota)</h3>

        <div className="campo" style={{ marginBottom: 16 }}>
          <label>Filtrar por data</label>
          <input
            type="date"
            value={filtroData}
            onChange={(e) => setFiltroData(e.target.value)}
            placeholder="Filtrar por data"
          />
        </div>

        {(() => {
          const pedidosFiltrados = filtroData
            ? pedidosCarregados.filter((p) => p.data_carregamento === filtroData)
            : pedidosCarregados.sort((a, b) =>
                new Date(`${b.data_carregamento} ${b.hora_carregamento}`) -
                new Date(`${a.data_carregamento} ${a.hora_carregamento}`)
              ).slice(0, 5);

          return pedidosFiltrados.length === 0 ? (
            <p className="muted">Nenhum pedido carregado.</p>
          ) : (
            <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(300px, 1fr))", gap: 12 }}>
              {pedidosFiltrados.map((p) => (
                <div
                  key={p.id}
                  className="card"
                  style={{
                    background: "rgba(34, 197, 94, .1)",
                    borderLeft: "4px solid #22c55e",
                  }}
                >
                  <h4 style={{ marginTop: 0, marginBottom: 8, color: "#86efac" }}>
                    ✅ Pedido #{p.id}
                  </h4>
                  <p style={{ margin: "4px 0", fontSize: 13 }}>
                    <strong>{p.produtos?.nome}</strong>
                  </p>
                  <p style={{ margin: "4px 0", fontSize: 13, color: "#a5b4fc" }}>
                    Quantidade: {p.quantidade} un.
                  </p>
                  <p style={{ margin: "4px 0", fontSize: 13, color: "#a5b4fc" }}>
                    Solicitante: {p.solicitante}
                  </p>
                  {p.nome_cliente && (
                    <p style={{ margin: "4px 0", fontSize: 12, color: "#a5b4fc" }}>
                      Cliente: {p.nome_cliente}
                    </p>
                  )}
                  {p.regiao && (
                    <p style={{ margin: "4px 0", fontSize: 12, color: "#a5b4fc" }}>
                      Região: {p.regiao}
                    </p>
                  )}
                  {p.data_carregamento && p.hora_carregamento && (
                    <p style={{ margin: "4px 0", fontSize: 12, color: "#86efac" }}>
                      📅 Carregado: {p.data_carregamento} às {p.hora_carregamento}
                    </p>
                  )}
                  {p.conferido_por && (
                    <p style={{ margin: "4px 0", fontSize: 12, color: "#fbbf24" }}>
                      ✓ Conferido por: {p.conferido_por}
                    </p>
                  )}
                  {p.nome_motorista_carregamento && (
                    <p style={{ margin: "4px 0", fontSize: 12, color: "#fbbf24" }}>
                      🚗 Motorista: {p.nome_motorista_carregamento}
                    </p>
                  )}
                  {p.nome_ajudante_carregamento && (
                    <p style={{ margin: "4px 0", fontSize: 12, color: "#fbbf24" }}>
                      👤 Ajudante: {p.nome_ajudante_carregamento}
                    </p>
                  )}
                  {p.veiculo_carregamento && (
                    <p style={{ margin: "4px 0", fontSize: 12, color: "#fbbf24" }}>
                      🚛 Veículo: {p.veiculo_carregamento}
                    </p>
                  )}
                  <p style={{ margin: "4px 0", fontSize: 12, color: "#86efac" }}>
                    🚛 Em Rota
                  </p>
                </div>
              ))}
            </div>
          );
        })()}
      </div>

      {/* MODAL CARREGAMENTO */}
      {modalCarregamento && (
        <div
          style={{
            position: "fixed",
            top: 0,
            left: 0,
            right: 0,
            bottom: 0,
            background: "rgba(0,0,0,.7)",
            display: "flex",
            alignItems: "center",
            justifyContent: "center",
            zIndex: 1000,
          }}
          onClick={() => setModalCarregamento(null)}
        >
          <div
            className="card"
            style={{ maxWidth: 500, width: "90%", maxHeight: "90vh", overflowY: "auto" }}
            onClick={(e) => e.stopPropagation()}
          >
            <h3>🚚 Carregamento</h3>

            <div className="campo">
              <label>Pedido</label>
              <div style={{ fontWeight: 700 }}>#{modalCarregamento.id}</div>
            </div>

            <div className="campo">
              <label>Produto</label>
              <div>{modalCarregamento.produtos?.nome}</div>
            </div>

            <div className="campo">
              <label>Quantidade</label>
              <div style={{ fontWeight: 700, fontSize: 18 }}>{modalCarregamento.quantidade} un.</div>
            </div>

            <div className="campo">
              <label>Responsável pela Expedição *</label>
              <input
                value={carregamentoForm.responsavel_expedicao}
                onChange={(e) =>
                  setCarregamentoForm({ ...carregamentoForm, responsavel_expedicao: e.target.value })
                }
                placeholder="Nome"
                required
              />
            </div>

            <div className="campo">
              <label>Nome Motorista *</label>
              <input
                value={carregamentoForm.nome_motorista}
                onChange={(e) =>
                  setCarregamentoForm({ ...carregamentoForm, nome_motorista: e.target.value })
                }
                placeholder="Nome"
                required
              />
            </div>

            <div className="campo">
              <label>Nome Ajudante *</label>
              <input
                value={carregamentoForm.nome_ajudante}
                onChange={(e) =>
                  setCarregamentoForm({ ...carregamentoForm, nome_ajudante: e.target.value })
                }
                placeholder="Nome"
                required
              />
            </div>

            <div className="campo">
              <label>Veículo *</label>
              <select
                value={carregamentoForm.veiculo}
                onChange={(e) =>
                  setCarregamentoForm({ ...carregamentoForm, veiculo: e.target.value })
                }
                required
              >
                <option value="">Selecione um veículo...</option>
                {Array.from({ length: 10 }, (_, i) => (
                  <option key={i + 1} value={`Caminhão ${i + 1}`}>Caminhão {i + 1}</option>
                ))}
              </select>
            </div>

            <div style={{ display: "flex", gap: 8 }}>
              <button
                className="btn"
                onClick={finalizarCarregamento}
                disabled={
                  finalizandoCarregamento ||
                  !carregamentoForm.responsavel_expedicao ||
                  !carregamentoForm.nome_motorista ||
                  !carregamentoForm.nome_ajudante ||
                  !carregamentoForm.veiculo
                }
                style={{ flex: 1 }}
              >
                {finalizandoCarregamento ? "Processando..." : "✅ Finalizar Carregamento"}
              </button>
              <button
                className="btn sec"
                onClick={() => setModalCarregamento(null)}
                disabled={finalizandoCarregamento}
                style={{ flex: 1 }}
              >
                ✕ Fechar
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
