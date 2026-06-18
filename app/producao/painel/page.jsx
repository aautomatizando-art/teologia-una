"use client";

import { useEffect, useState } from "react";
export const dynamic = "force-dynamic";
import {
  ResponsiveContainer, LineChart, Line, BarChart, Bar, XAxis, YAxis, Tooltip, CartesianGrid,
} from "recharts";
import { pertenceALinhas, agruparPorTipo } from "@/lib/linhas";
import Esp32Status from "@/components/Esp32Status";

const TOOLTIP = { background: "#1a2347", border: "1px solid #26305c", borderRadius: 10, color: "#e8ecf8" };

const PAINEIS = [
  { titulo: "Painel 1", cor: "#6366f1", linhas: [1, 2] },
  { titulo: "Painel 2", cor: "#06b6d4", linhas: [3, 4] },
  { titulo: "Painel 3", cor: "#a855f7", linhas: [5, 6] },
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

  return (
    <div className="shell" style={{ height: "100vh", display: "flex", flexDirection: "column", overflow: "hidden" }}>
      <div style={{ display: "flex", justifyContent: "space-between", alignItems: "center", marginBottom: 14 }}>
        <a href="/producao" className="btn sec" style={{ textDecoration: "none" }}>← Voltar</a>
        <h2 style={{ fontSize: 15, fontWeight: 800, letterSpacing: "0.06em", textTransform: "uppercase", color: "#a5b4fc", margin: 0 }}>
          📺 Visão geral da produção
        </h2>
        <span className="muted" style={{ fontSize: 12 }}>
          {carregando ? "Atualizando..." : "Atualiza automaticamente a cada 30s"}
        </span>
      </div>

      <div style={{ flex: 1, minHeight: 0, display: "grid", gridTemplateColumns: "1fr 1fr 1fr", gap: 14 }}>
        {paineis.map((d, i) => (
          <PainelColuna key={i} dados={d} cor={PAINEIS[i].cor} titulo={PAINEIS[i].titulo} linhas={PAINEIS[i].linhas} painel={i + 1} />
        ))}
      </div>
    </div>
  );
}

function PainelColuna({ dados, cor, titulo, linhas, painel }) {
  const o = dados?.ordem;

  if (!o) {
    return (
      <div className="card" style={{ borderTop: `3px solid ${cor}` }}>
        <h3>🏭 {titulo}</h3>
        <div className="muted">Nenhuma OP cadastrada.</div>
      </div>
    );
  }

  const registrosFiltrados = (dados?.registros || []).filter((r) => pertenceALinhas(r, linhas));
  let acc = 0;
  const linhaAcum = registrosFiltrados.map((r) => {
    const d = new Date(r.registrado_em);
    acc += r.paletes || 0;
    return {
      hora: d.toLocaleString("pt-BR", { day: "2-digit", month: "2-digit", hour: "2-digit", minute: "2-digit" }),
      acumulado: acc,
    };
  });
  const produzido = acc;
  const produzidoCx = produzido * (o.caixasPorPalete || 1);
  // percentual total da OP (todos painéis somados) — fica igual nos 3 quando compartilham a mesma OP
  const pct = o.percentual ?? 0;
  const totalOpCx = Math.round((o.produzido || 0) * (o.caixasPorPalete || 1));

  const perdas = agruparPorTipo((dados?.perdasDetalhe || []).filter((r) => pertenceALinhas(r, linhas)));
  const problemas = agruparPorTipo((dados?.problemasDetalhe || []).filter((r) => pertenceALinhas(r, linhas)));

  return (
    <div style={{ display: "flex", flexDirection: "column", gap: 12, minHeight: 0 }}>
      <div className="card" style={{ borderTop: `3px solid ${cor}`, flex: "0 0 auto", padding: "12px 16px" }}>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline" }}>
          <h3 style={{ margin: 0, fontSize: 13, display: "flex", alignItems: "center", flexWrap: "wrap", gap: 6 }}>
            🏭 {titulo} <span className="muted" style={{ fontWeight: 400, fontSize: 11 }}>(Linhas {linhas.join("+")})</span>
            <Esp32Status painel={painel} />
          </h3>
          <span className={`badge ${o.status === "ABERTA" ? "ok" : "alto"}`}>{o.status}</span>
        </div>
        <div style={{ display: "flex", justifyContent: "space-between", alignItems: "flex-end", marginTop: 8 }}>
          <div style={{ minWidth: 0 }}>
            <div style={{ fontWeight: 800, fontSize: 16 }}>{o.numero}</div>
            <div className="muted" style={{ fontSize: 12, whiteSpace: "nowrap", overflow: "hidden", textOverflow: "ellipsis" }}>
              {o.produto || "—"}
            </div>
          </div>
          <div style={{ textAlign: "right" }}>
            <div className="kpi" style={{ fontSize: 24 }}>{produzido.toLocaleString("pt-BR")} <small>paletes</small></div>
            <div className="muted" style={{ fontSize: 11 }}>{produzidoCx.toLocaleString("pt-BR")} cx</div>
          </div>
        </div>
        <div style={{ marginTop: 10 }}>
          <div style={{ display: "flex", justifyContent: "space-between", alignItems: "baseline", marginBottom: 4 }}>
            <span className="muted" style={{ fontSize: 11 }}>
              {totalOpCx.toLocaleString("pt-BR")} / {(o.meta_paletes || 0).toLocaleString("pt-BR")} cx (OP total)
            </span>
            <span style={{ fontSize: 13, fontWeight: 800, color: cor }}>{pct}%</span>
          </div>
          <div style={{ width: "100%", height: 10, background: "#26305c", borderRadius: 99, overflow: "hidden" }}>
            <div style={{
              height: "100%",
              width: `${pct}%`,
              background: `linear-gradient(90deg, ${cor}99, ${cor})`,
              borderRadius: 99,
              transition: "width 0.6s ease",
              boxShadow: pct > 0 ? `0 0 8px ${cor}88` : "none",
            }} />
          </div>
        </div>
      </div>

      <div className="card" style={{ flex: "1 1 0", minHeight: 0, padding: "12px 16px", display: "flex", flexDirection: "column" }}>
        <h3 style={{ fontSize: 13, marginBottom: 6 }}>📈 Paletes × tempo</h3>
        {linhaAcum.length === 0 ? (
          <div className="muted" style={{ fontSize: 12 }}>Sem lançamentos para estas linhas.</div>
        ) : (
          <div style={{ flex: 1, minHeight: 0 }}>
            <ResponsiveContainer width="100%" height="100%">
              <LineChart data={linhaAcum} margin={{ top: 4, right: 8, left: -16, bottom: 0 }}>
                <CartesianGrid stroke="#26305c" strokeDasharray="3 3" />
                <XAxis dataKey="hora" tick={{ fill: "#8b96c0", fontSize: 9 }} />
                <YAxis tick={{ fill: "#8b96c0", fontSize: 9 }} allowDecimals={false} />
                <Tooltip contentStyle={TOOLTIP} />
                <Line type="monotone" dataKey="acumulado" name="Acumulado" stroke={cor} strokeWidth={2} dot={{ r: 2 }} />
              </LineChart>
            </ResponsiveContainer>
          </div>
        )}
      </div>

      <div className="card" style={{ flex: "1 1 0", minHeight: 0, padding: "12px 16px", display: "flex", flexDirection: "column" }}>
        <h3 style={{ fontSize: 13, marginBottom: 6 }}>📦 Perdas de embalagem</h3>
        {perdas.length === 0 ? (
          <div className="muted" style={{ fontSize: 12 }}>Sem perdas registradas.</div>
        ) : (
          <div style={{ flex: 1, minHeight: 0 }}>
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={perdas} margin={{ top: 4, right: 8, left: -16, bottom: 0 }}>
                <CartesianGrid stroke="#26305c" strokeDasharray="3 3" vertical={false} />
                <XAxis dataKey="tipo" tick={{ fill: "#8b96c0", fontSize: 9 }} />
                <YAxis tick={{ fill: "#8b96c0", fontSize: 9 }} allowDecimals={false} />
                <Tooltip contentStyle={TOOLTIP} />
                <Bar dataKey="quantidade" fill="#f59e0b" radius={[6, 6, 0, 0]} />
              </BarChart>
            </ResponsiveContainer>
          </div>
        )}
      </div>

      <div className="card" style={{ flex: "1 1 0", minHeight: 0, padding: "12px 16px", display: "flex", flexDirection: "column" }}>
        <h3 style={{ fontSize: 13, marginBottom: 6 }}>🧪 Problemas qualitativos</h3>
        {problemas.length === 0 ? (
          <div className="muted" style={{ fontSize: 12 }}>Sem problemas registrados.</div>
        ) : (
          <div style={{ flex: 1, minHeight: 0 }}>
            <ResponsiveContainer width="100%" height="100%">
              <BarChart data={problemas} margin={{ top: 4, right: 8, left: -16, bottom: 0 }}>
                <CartesianGrid stroke="#26305c" strokeDasharray="3 3" vertical={false} />
                <XAxis dataKey="tipo" tick={{ fill: "#8b96c0", fontSize: 9 }} />
                <YAxis tick={{ fill: "#8b96c0", fontSize: 9 }} allowDecimals={false} />
                <Tooltip contentStyle={TOOLTIP} />
                <Bar dataKey="quantidade" fill="#ef4444" radius={[6, 6, 0, 0]} />
              </BarChart>
            </ResponsiveContainer>
          </div>
        )}
      </div>
    </div>
  );
}
