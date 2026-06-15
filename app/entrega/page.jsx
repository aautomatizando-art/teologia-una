"use client";
import { useEffect, useState } from "react";
import TopBar from "@/components/TopBar";

const EMOJI_CRIT = { EMERGENCIAL: "🟣", URGENTE: "🔴", MODERADO: "🟢" };

export default function PaginaEntrega() {
  const [veiculos, setVeiculos] = useState([]);
  const [veiculo, setVeiculo] = useState("");
  const [pedidos, setPedidos] = useState([]);
  const [carregando, setCarregando] = useState(false);
  const [erro, setErro] = useState("");

  // Buscar veículos disponíveis
  useEffect(() => {
    async function buscarVeiculos() {
      try {
        const res = await fetch("/api/entrega/veiculos");
        const data = await res.json();
        if (res.ok) setVeiculos(data.veiculos || []);
      } catch {
        setErro("Erro ao carregar veículos");
      }
    }
    buscarVeiculos();
  }, []);

  // Buscar pedidos do veículo selecionado
  async function buscarPedidos(nomeVeiculo) {
    setVeiculo(nomeVeiculo);
    setPedidos([]);
    setCarregando(true);
    try {
      const res = await fetch(`/api/entrega/pedidos?veiculo=${encodeURIComponent(nomeVeiculo)}`);
      const data = await res.json();
      if (res.ok) setPedidos(data.pedidos || []);
      else setErro(data.error || "Erro ao carregar pedidos");
    } catch {
      setErro("Erro de conexão");
    } finally {
      setCarregando(false);
    }
  }

  return (
    <div className="shell">
      <TopBar />

      <div className="card" style={{ marginBottom: 18 }}>
        <h3>🚚 Entrega Finalizada</h3>

        <div className="campo" style={{ marginBottom: 16 }}>
          <label>Selecionar Veículo</label>
          <select value={veiculo} onChange={(e) => buscarPedidos(e.target.value)}>
            <option value="">— Selecione um veículo —</option>
            {veiculos.map((v) => (
              <option key={v} value={v}>
                {v}
              </option>
            ))}
          </select>
        </div>

        {erro && <div className="erro">{erro}</div>}
      </div>

      {carregando && <div className="spin" />}

      {veiculo && pedidos.length > 0 && (
        <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(350px, 1fr))", gap: 12 }}>
          {pedidos.map((p) => (
            <PedidoCard key={p.id} pedido={p} onAtualizar={() => buscarPedidos(veiculo)} />
          ))}
        </div>
      )}

      {veiculo && pedidos.length === 0 && !carregando && (
        <p className="muted">Nenhum pedido para este veículo</p>
      )}
    </div>
  );
}

function PedidoCard({ pedido, onAtualizar }) {
  const [finalizando, setFinalizando] = useState(false);
  const [erro, setErro] = useState("");

  async function finalizarEntrega() {
    setFinalizando(true);
    setErro("");
    try {
      const res = await fetch("/api/entrega/finalizar", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ pedido_id: pedido.id }),
      });
      const data = await res.json();
      if (!res.ok) setErro(data.error || "Erro ao finalizar");
      else {
        alert("✅ Entrega finalizada!");
        onAtualizar();
      }
    } catch {
      setErro("Erro de conexão");
    } finally {
      setFinalizando(false);
    }
  }

  return (
    <div
      className="card"
      style={{
        background: "rgba(34, 197, 94, .1)",
        borderLeft: "4px solid #22c55e",
      }}
    >
      <h4 style={{ marginTop: 0, marginBottom: 8, color: "#86efac" }}>
        ✅ Pedido #{pedido.id}
      </h4>

      <p style={{ margin: "4px 0", fontSize: 13 }}>
        <strong>{pedido.produtos?.nome}</strong>
      </p>

      <p style={{ margin: "4px 0", fontSize: 12, color: "#a5b4fc" }}>
        Quantidade: {pedido.quantidade} un.
      </p>

      <p style={{ margin: "4px 0", fontSize: 12, color: "#a5b4fc" }}>
        Solicitante: {pedido.solicitante}
      </p>

      {pedido.nome_cliente && (
        <p style={{ margin: "4px 0", fontSize: 12, color: "#a5b4fc" }}>
          Cliente: {pedido.nome_cliente}
        </p>
      )}

      {pedido.regiao && (
        <p style={{ margin: "4px 0", fontSize: 12, color: "#a5b4fc" }}>
          Região: {pedido.regiao}
        </p>
      )}

      <p style={{ margin: "4px 0", fontSize: 12, color: "#fbbf24" }}>
        🚛 Veículo: {pedido.veiculo}
      </p>

      <p style={{ margin: "4px 0", fontSize: 12, color: "#fbbf24" }}>
        🚗 Motorista: {pedido.nome_motorista}
      </p>

      <p style={{ margin: "4px 0", fontSize: 12, color: "#fbbf24" }}>
        👤 Ajudante: {pedido.nome_ajudante}
      </p>

      {erro && (
        <p style={{ margin: "8px 0", fontSize: 12, color: "#ef4444" }}>
          ❌ {erro}
        </p>
      )}

      <button
        className={`btn ${pedido.status_rastreio === 6 ? "sec" : ""}`}
        onClick={finalizarEntrega}
        disabled={finalizando || pedido.status_rastreio === 6}
        style={{ marginTop: 12, width: "100%", opacity: pedido.status_rastreio === 6 ? 0.6 : 1 }}
      >
        {finalizando ? "Processando..." : pedido.status_rastreio === 6 ? "✅ Entregue" : "✅ Entrega Finalizada"}
      </button>
    </div>
  );
}
