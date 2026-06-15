"use client";

import { useEffect, useState } from "react";
export const dynamic = "force-dynamic";
import TopBar from "@/components/TopBar";
import {
  ResponsiveContainer, BarChart, Bar, XAxis, YAxis, Tooltip, CartesianGrid, Cell,
} from "recharts";

const TOOLTIP = { background: "#1a2347", border: "1px solid #26305c", borderRadius: 10, color: "#e8ecf8" };

const PAINEIS = [
  { titulo: "Painel 1", cor: "#6366f1" },
  { titulo: "Painel 2", cor: "#06b6d4" },
  { titulo: "Painel 3", cor: "#a855f7" },
];

const REFRESH_MS = 30000;

export default function PainelVisualizacao() {
  const [paineis, setPaineis] = useState([null, null, null]);
  const [carregando, setCarregando] = useState(true);

  async function carregar() {
    try {
      const j = await fetch("/api/producao").then((r) => r.json());
      const todas = j.ordens || [];
      const escolhidas = [todas[0], todas[1] || todas[0], todas[2] || todas[0]];
      const dados = await Promise.all(
        escolhidas.map((o) =>
          o ? fetch(`/api/producao?op=${encodeURIComponent(o.numero)}`).then((r) => r.json()) : Promise.resolve(null)
        )
      );
      setPaineis(dados);
    } catch {
      // mantém os dados anteriores em caso de falha temporária
    } finally {
      setCarregando(false);
    }
  }

  useEffect(() => {
    carregar();
    const id = setInterval(carregar, REFRESH_MS);
    return () => clearInterval(id);
  }, []);

  // total geral: deduplica por número de OP (evita somar 2x quando painéis compartilham a mesma OP)
  const distintas = {};
  for (const d of paineis) {
    const o = d?.ordem;
    if (o && !distintas[o.numero]) distintas[o.numero] = o;
  }
  const lista = Object.values(distintas);
  const totalProduzidoCx = lista.reduce((s, o) => s + (o.produzido || 0) * (o.caixasPorPalete || 1), 0);
  const totalMeta = lista.reduce((s, o) => s + (o.meta_paletes || 0), 0);
  const totalPct = totalMeta ? Math.min(100, Math.round((totalProduzidoCx / totalMeta) * 100)) : 0;

  const dadosBarra = paineis.map((d, i) => ({
    nome: d?.ordem?.numero || PAINEIS[i].titulo,
    pct: d?.ordem?.percentual || 0,
    cor: PAINEIS[i].cor,
  }));

  return (
    <div className="shell" style={{ height: "100vh", display: "flex", flexDirection: "column", overflow: "hidden" }}>
      <TopBar />
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 14 }}>
        <a href="/producao" className="btn sec" style={{ textDecoration: "none" }}>← Voltar para lançamentos</a>
        <span className="muted" style={{ fontSize: 12 }}>
          {carregando ? "Atualizando..." : "Atualiza automaticamente a cada 30s"}
        </span>
      </div>

      <div style={{ flex: "0 0 auto", display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: 18, marginBottom: 18 }}>
        {paineis.map((d, i) => (
          <PainelCard key={i} dados={d} cor={PAINEIS[i].cor} titulo={PAINEIS[i].titulo} />
        ))}
      </div>

      <div className="card" style={{ flex: 1, minHeight: 0, display: "flex", flexDirection: "column" }}>
        <h3>📊 Total geral em produção</h3>
        <div style={{ display: "flex", gap: 28, alignItems: "baseline", margin: "10px 0 6px" }}>
          <div className="kpi" style={{ fontSize: 44 }}>{totalPct}%</div>
          <div className="muted" style={{ fontSize: 15 }}>
            {totalProduzidoCx.toLocaleString("pt-BR")} / {totalMeta.toLocaleString("pt-BR")} caixas
          </div>
        </div>
        <div style={{ flex: 1, minHeight: 0 }}>
          <ResponsiveContainer width="100%" height="100%">
            <BarChart data={dadosBarra} layout="vertical" margin={{ top: 8, right: 24, left: 0, bottom: 8 }}>
              <CartesianGrid strokeDasharray="3 3" stroke="#26305c" horizontal={false} />
              <XAxis type="number" domain={[0, 100]} stroke="#8b96c0" unit="%" />
              <YAxis type="category" dataKey="nome" stroke="#8b96c0" width={100} />
              <Tooltip contentStyle={TOOLTIP} formatter={(v) => [`${v}%`, "Concluído"]} />
              <Bar dataKey="pct" radius={[0, 8, 8, 0]} barSize={36}>
                {dadosBarra.map((d, i) => <Cell key={i} fill={d.cor} />)}
              </Bar>
            </BarChart>
          </ResponsiveContainer>
        </div>
      </div>
    </div>
  );
}

function PainelCard({ dados, cor, titulo }) {
  const o = dados?.ordem;
  if (!o) {
    return (
      <div className="card" style={{ borderTop: `3px solid ${cor}` }}>
        <h3>🏭 {titulo}</h3>
        <div className="muted">Nenhuma OP cadastrada.</div>
      </div>
    );
  }
  const produzidoCx = (o.produzido || 0) * (o.caixasPorPalete || 1);
  return (
    <div className="card" style={{ borderTop: `3px solid ${cor}` }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
        <h3 style={{ margin: 0 }}>🏭 {titulo}</h3>
        <span className={`badge ${o.status === "ABERTA" ? "ok" : "alto"}`}>{o.status}</span>
      </div>
      <div style={{ fontWeight: 800, fontSize: 17, marginTop: 8 }}>{o.numero}</div>
      <div className="muted" style={{ fontSize: 13, marginBottom: 12, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
        {o.produto || "—"}
      </div>
      <div className="kpi" style={{ fontSize: 32 }}>{o.percentual}%</div>
      <div className="barra" style={{ margin: "8px 0" }}>
        <div style={{ width: `${o.percentual}%`, background: cor }} />
      </div>
      <div className="muted" style={{ fontSize: 13 }}>
        {produzidoCx.toLocaleString("pt-BR")} / {o.meta_paletes.toLocaleString("pt-BR")} cx
        {" "}({(o.produzido || 0).toLocaleString("pt-BR")} paletes)
      </div>
    </div>
  );
}
