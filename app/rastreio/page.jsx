"use client";
import { Suspense, useEffect, useState } from "react";
import { useSearchParams } from "next/navigation";
import TopBar from "@/components/TopBar";

const ETAPAS = [
  { rotulo: "GERADO ORDEM DE PRODUÇÃO", icone: "📋" },
  { rotulo: "PRODUZINDO", icone: "🏭" },
  { rotulo: "ENTRADO NO ESTOQUE", icone: "📦" },
  { rotulo: "NA EXPEDIÇÃO", icone: "🚛" },
  { rotulo: "EM ROTA PARA ENTREGA", icone: "🛣️" },
];

function Rastreio() {
  const params = useSearchParams();
  const [pedidos, setPedidos] = useState(null);
  const [selecionado, setSelecionado] = useState(null);
  const [erro, setErro] = useState("");
  const [salvando, setSalvando] = useState(false);

  async function carregar(idManter) {
    try {
      const res = await fetch("/api/compras");
      const j = await res.json();
      if (!res.ok) return setErro(j.error || "Erro ao carregar pedidos.");
      setPedidos(j.pedidos);
      const alvo = idManter ?? Number(params.get("pedido"));
      if (alvo) {
        const p = j.pedidos.find((x) => x.id === alvo);
        if (p) setSelecionado(p);
      } else if (j.pedidos.length && !selecionado) {
        setSelecionado(j.pedidos[0]);
      }
    } catch {
      setErro("Erro de conexão.");
    }
  }
  useEffect(() => { carregar(); }, []);

  async function mudarStatus(novo) {
    if (!selecionado) return;
    setSalvando(true);
    setErro("");
    try {
      const res = await fetch("/api/rastreio", {
        method: "PATCH",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ id: selecionado.id, status: novo }),
      });
      const j = await res.json();
      if (!res.ok) setErro(j.error || "Erro ao atualizar status.");
      else await carregar(selecionado.id);
    } catch {
      setErro("Erro de conexão.");
    } finally {
      setSalvando(false);
    }
  }

  const s = selecionado?.status_rastreio || 0;

  return (
    <div className="shell">
      <TopBar />

      <div className="card" style={{ marginBottom: 18 }}>
        <h3>🚚 Rastreio de pedido</h3>
        {erro && <div className="erro">{erro}</div>}
        <div className="campo">
          <label>Selecione o pedido</label>
          <select
            value={selecionado?.id || ""}
            onChange={(e) => setSelecionado(pedidos.find((p) => p.id === Number(e.target.value)))}
          >
            <option value="">—</option>
            {(pedidos || []).map((p) => (
              <option key={p.id} value={p.id}>
                #{p.id} • {p.produtos?.nome} • {p.criticidade} • {p.solicitante}
              </option>
            ))}
          </select>
        </div>
      </div>

      {!pedidos && !erro && <div className="spin" />}

      {selecionado && (
        <div className="card">
          <div style={{ display: "flex", justifyContent: "space-between", flexWrap: "wrap", gap: 10, marginBottom: 6 }}>
            <div>
              <div style={{ fontWeight: 800, fontSize: 17 }}>Pedido #{selecionado.id} — {selecionado.produtos?.nome}</div>
              <div className="muted" style={{ fontSize: 13, marginTop: 4 }}>
                Qtd: {selecionado.quantidade} • Solicitante: {selecionado.solicitante} • Criticidade: {selecionado.criticidade}
              </div>
            </div>
            <span className="badge ok" style={{ alignSelf: "start" }}>{ETAPAS[s - 1]?.rotulo}</span>
          </div>

          <div className="timeline">
            {ETAPAS.map((e, i) => {
              const n = i + 1;
              return (
                <div key={n} className={`etapa ${n <= s ? "feita" : ""} ${n === s ? "atual" : ""}`}>
                  <div className="bola">{n <= s ? e.icone : n}</div>
                  <div className="rotulo">{e.rotulo}</div>
                </div>
              );
            })}
          </div>

          <div style={{ display: "flex", gap: 10, justifyContent: "center", flexWrap: "wrap", marginTop: 10 }}>
            <button className="btn sec" disabled={salvando || s <= 1} onClick={() => mudarStatus(s - 1)}>
              ← Voltar etapa
            </button>
            <button className="btn" disabled={salvando || s >= 5} onClick={() => mudarStatus(s + 1)}>
              {salvando ? "Atualizando..." : s >= 5 ? "Pedido em rota 🎉" : `Avançar para: ${ETAPAS[s]?.rotulo} →`}
            </button>
          </div>
        </div>
      )}
    </div>
  );
}

export default function PaginaRastreio() {
  return (
    <Suspense>
      <Rastreio />
    </Suspense>
  );
}
