"use client";
import { useEffect, useMemo, useState } from "react";
import {
  ResponsiveContainer, BarChart, Bar, XAxis, YAxis,
  CartesianGrid, Tooltip, ReferenceLine,
} from "recharts";

export const dynamic = "force-dynamic";

const TOOLTIP = { background: "#1a2347", border: "1px solid #26305c", borderRadius: 10, color: "#e8ecf8" };
const GAUGE_MAX = 20;

const NIVEIS = [
  { min: 0,  max: 10,        cor: "#ef4444", label: "Péssimo" },
  { min: 10, max: 13,        cor: "#f97316", label: "Ruim"    },
  { min: 13, max: 14,        cor: "#eab308", label: "Bom"     },
  { min: 14, max: GAUGE_MAX, cor: "#22c55e", label: "Ótimo"   },
];

function nivelPara(v) {
  for (let i = NIVEIS.length - 1; i >= 0; i--) {
    if (v >= NIVEIS[i].min) return NIVEIS[i];
  }
  return NIVEIS[0];
}

// Converte ângulo de relógio (0°=topo, horário) para coordenada SVG
function gPt(cx, cy, r, deg) {
  const rad = deg * Math.PI / 180;
  return [cx + r * Math.sin(rad), cy - r * Math.cos(rad)];
}

// Arco donut: gauge vai de 270° (esquerda) a 450°=90° (direita) passando pelo TOPO (360°)
// valor v → ângulo = 270 + (v/maxV)*180
function arcSeg(fromV, toV, maxV, cx, cy, r, ri) {
  const a1 = 270 + (fromV / maxV) * 180;
  const a2 = 270 + (toV   / maxV) * 180;
  const large = (a2 - a1) > 180 ? 1 : 0;
  const [ox1, oy1] = gPt(cx, cy, r,  a1);
  const [ox2, oy2] = gPt(cx, cy, r,  a2);
  const [ix1, iy1] = gPt(cx, cy, ri, a1);
  const [ix2, iy2] = gPt(cx, cy, ri, a2);
  // outer: horário (sweep=1) | inner: anti-horário (sweep=0) → furo correto
  return `M ${ox1} ${oy1} A ${r} ${r} 0 ${large} 1 ${ox2} ${oy2} L ${ix2} ${iy2} A ${ri} ${ri} 0 ${large} 0 ${ix1} ${iy1} Z`;
}

function Velocimetro({ value, titulo, detalhe }) {
  const cx = 100, cy = 96, r = 80, ri = 50;
  const v = Math.max(0, Math.min(value || 0, GAUGE_MAX));
  const nivel = nivelPara(v);
  // agulha usa mesma convenção de ângulo: 270°=esquerda, 450°=direita passando pelo topo
  const needleDeg = 270 + (v / GAUGE_MAX) * 180;
  const [nx, ny] = gPt(cx, cy, ri - 4, needleDeg);
  // marcas de escala nas extremidades e no topo
  const [lx, ly] = gPt(cx, cy, r + 10, 270); // "0" na esquerda
  const [tx, ty] = gPt(cx, cy, r + 10, 360); // "10" no topo
  const [rx2, ry2] = gPt(cx, cy, r + 10, 450); // "20" na direita

  return (
    <div>
      <p style={{ textAlign: "center", fontSize: 12, color: "#8b96c0", margin: "0 0 2px", fontWeight: 600 }}>
        {titulo}
      </p>
      <svg viewBox="0 0 200 108" style={{ width: "100%", maxWidth: 280, display: "block", margin: "0 auto" }}>
        {/* fundo */}
        <path d={arcSeg(0, GAUGE_MAX, GAUGE_MAX, cx, cy, r, ri)} fill="#1a2347" />
        {/* segmentos coloridos */}
        {NIVEIS.map((n, i) => (
          <path key={i} d={arcSeg(n.min, Math.min(n.max, GAUGE_MAX), GAUGE_MAX, cx, cy, r, ri)}
            fill={n.cor} stroke="#0b1020" strokeWidth={1} opacity={v >= n.min ? 0.9 : 0.3} />
        ))}
        {/* agulha */}
        <line x1={cx} y1={cy} x2={nx} y2={ny} stroke="#fff" strokeWidth={3} strokeLinecap="round" />
        <circle cx={cx} cy={cy} r={5} fill="#fff" />
        {/* valor central */}
        <text x={cx} y={cy - 8} textAnchor="middle" fill="#f0f4ff" fontSize={19} fontWeight="bold">
          {(value || 0).toFixed(1)}
        </text>
        <text x={cx} y={cy + 6} textAnchor="middle" fill={nivel.cor} fontSize={10} fontWeight="700">
          {nivel.label}
        </text>
        {/* marcas de escala */}
        <text x={lx}  y={ly}  textAnchor="middle" fill="#4b5563" fontSize={9}>0</text>
        <text x={tx}  y={ty}  textAnchor="middle" fill="#4b5563" fontSize={9}>{GAUGE_MAX / 2}</text>
        <text x={rx2} y={ry2} textAnchor="middle" fill="#4b5563" fontSize={9}>{GAUGE_MAX}</text>
      </svg>
      {detalhe && (
        <p style={{ textAlign: "center", fontSize: 11, color: "#6b7280", margin: "6px 0 0" }}>{detalhe}</p>
      )}
    </div>
  );
}

export default function RendimentoPage() {
  const hoje = new Date().toISOString().split("T")[0];

  const [form, setForm] = useState({
    data: hoje, fornecedor: "", regiao: "", tipo_batata: "",
    peso_liquido: "", qtd_sacos: "", qtd_sacos_produzidos: "", media_tirada: "",
  });
  const [salvando, setSalvando] = useState(false);
  const [erroForm, setErroForm] = useState("");
  const [okForm, setOkForm] = useState("");
  const [registros, setRegistros] = useState([]);
  const [filtro, setFiltro] = useState("diario"); // "diario" | "mensal"

  async function carregar() {
    try {
      const j = await fetch("/api/rendimento").then((r) => r.json());
      setRegistros(j.registros || []);
    } catch {}
  }

  useEffect(() => { carregar(); }, []);

  function campo(key, valor) {
    setForm((prev) => {
      const next = { ...prev, [key]: valor };
      if (key === "peso_liquido") {
        const n = parseFloat(valor);
        next.qtd_sacos = (!isNaN(n) && n > 0) ? String(Math.round(n / 50)) : "";
      }
      if (key === "qtd_sacos") {
        const n = parseFloat(valor);
        next.peso_liquido = (!isNaN(n) && n > 0) ? String(n * 50) : "";
      }
      return next;
    });
  }

  async function salvar(e) {
    e.preventDefault();
    setSalvando(true); setErroForm(""); setOkForm("");
    try {
      const res = await fetch("/api/rendimento", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify(form),
      });
      const j = await res.json();
      if (!res.ok) { setErroForm(j.error || "Erro ao salvar."); return; }
      setOkForm("✅ Romaneio salvo com sucesso!");
      setForm((p) => ({ data: p.data, fornecedor: "", regiao: "", tipo_batata: "",
        peso_liquido: "", qtd_sacos: "", qtd_sacos_produzidos: "", media_tirada: "" }));
      carregar();
    } catch { setErroForm("Erro de conexão."); }
    finally { setSalvando(false); }
  }

  // Gauge 1: ao vivo do formulário
  const gauge1 = parseFloat(form.media_tirada) || 0;

  // Gauge 2: soma do dia (db + form em preenchimento)
  const regHoje = registros.filter((r) => r.data === hoje);
  const totalPeso  = regHoje.reduce((s, r) => s + Number(r.peso_liquido || 0), 0)
                   + (parseFloat(form.peso_liquido) || 0);
  const totalSacos = regHoje.reduce((s, r) => s + Number(r.qtd_sacos_produzidos || 0), 0)
                   + (parseInt(form.qtd_sacos_produzidos) || 0);
  const gauge2 = totalSacos > 0 ? totalPeso / totalSacos : 0;

  // Dados do gráfico de barras
  const dadosGrafico = useMemo(() => {
    const map = {};
    for (const r of registros) {
      const chave = filtro === "mensal" ? r.data.slice(0, 7) : r.data;
      if (!map[chave]) map[chave] = { peso: 0, sacos: 0 };
      map[chave].peso  += Number(r.peso_liquido || 0);
      map[chave].sacos += Number(r.qtd_sacos_produzidos || 0);
    }
    return Object.entries(map)
      .sort(([a], [b]) => a.localeCompare(b))
      .map(([chave, v]) => ({
        periodo: filtro === "mensal"
          ? new Date(chave + "-15").toLocaleDateString("pt-BR", { month: "short", year: "2-digit" })
          : new Date(chave + "T12:00:00").toLocaleDateString("pt-BR", { day: "2-digit", month: "2-digit" }),
        media: v.sacos > 0 ? parseFloat((v.peso / v.sacos).toFixed(2)) : 0,
        cor: nivelPara(v.sacos > 0 ? v.peso / v.sacos : 0).cor,
      }));
  }, [registros, filtro]);

  return (
    <div className="shell">
      {/* Cabeçalho */}
      <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", marginBottom: 24 }}>
        <h1 style={{ fontSize: 18, fontWeight: 800, color: "#a5b4fc", margin: 0 }}>
          🥔 Controle de Rendimento
        </h1>
        <form action="/api/auth/logout" method="post">
          <button type="submit" className="btn sec" style={{ fontSize: 12 }}>⬅ Sair</button>
        </form>
      </div>

      {/* ── BLOCO 1: ROMANEIO ── */}
      <div className="card" style={{ marginBottom: 24 }}>
        <h3>📋 Romaneio</h3>
        {erroForm && <div className="erro">{erroForm}</div>}
        {okForm && (
          <div className="aviso" style={{ borderColor: "rgba(34,197,94,.4)", color: "#86efac", background: "rgba(34,197,94,.1)", marginBottom: 14 }}>
            {okForm}
          </div>
        )}
        <form onSubmit={salvar}>
          <div style={{ display: "grid", gridTemplateColumns: "repeat(auto-fill, minmax(190px, 1fr))", gap: 12, marginBottom: 16 }}>
            <div className="campo">
              <label>Data</label>
              <input type="date" value={form.data} onChange={(e) => campo("data", e.target.value)} required />
            </div>
            <div className="campo">
              <label>Fornecedor da Batata</label>
              <input value={form.fornecedor} onChange={(e) => campo("fornecedor", e.target.value)}
                placeholder="Ex.: Fazenda Esperança" required />
            </div>
            <div className="campo">
              <label>Região da Batata</label>
              <input value={form.regiao} onChange={(e) => campo("regiao", e.target.value)}
                placeholder="Ex.: SP / MG" />
            </div>
            <div className="campo">
              <label>Tipo de Batata</label>
              <input value={form.tipo_batata} onChange={(e) => campo("tipo_batata", e.target.value)}
                placeholder="Ex.: Asterix" />
            </div>
            <div className="campo">
              <label>Peso Líquido (kg)</label>
              <input type="number" step="0.1" min="0" value={form.peso_liquido}
                onChange={(e) => campo("peso_liquido", e.target.value)} placeholder="Ex.: 5000" />
              <span className="muted" style={{ fontSize: 10 }}>Preenche Qtd Sacos ÷ 50</span>
            </div>
            <div className="campo">
              <label>Qtd Sacos</label>
              <input type="number" step="1" min="0" value={form.qtd_sacos}
                onChange={(e) => campo("qtd_sacos", e.target.value)} placeholder="Auto: Peso ÷ 50" />
              <span className="muted" style={{ fontSize: 10 }}>Preenche Peso Líquido × 50</span>
            </div>
            <div className="campo">
              <label>Qtd Sacos Produzidos</label>
              <input type="number" step="1" min="0" required value={form.qtd_sacos_produzidos}
                onChange={(e) => campo("qtd_sacos_produzidos", e.target.value)} placeholder="Ex.: 380" />
            </div>
            <div className="campo">
              <label>Média Tirada (kg/saco)</label>
              <input type="number" step="0.01" min="0" required value={form.media_tirada}
                onChange={(e) => campo("media_tirada", e.target.value)} placeholder="Ex.: 13.5" />
              <span className="muted" style={{ fontSize: 10 }}>Atualiza o velocímetro ao vivo</span>
            </div>
          </div>
          <button className="btn" disabled={salvando}>
            {salvando ? "Salvando..." : "💾 Salvar Romaneio"}
          </button>
        </form>
      </div>

      {/* ── BLOCO 2: VELOCÍMETROS ── */}
      <div className="grid g2" style={{ marginBottom: 24 }}>
        <div className="card">
          <h3>🎯 Média Tirada</h3>
          <Velocimetro
            value={gauge1}
            titulo="Leitura ao vivo do formulário"
            detalhe={`Valor inserido: ${gauge1.toFixed(2)} kg/saco`}
          />
          <div style={{ display: "flex", justifyContent: "center", flexWrap: "wrap", gap: 10, marginTop: 10 }}>
            {NIVEIS.map((n) => (
              <div key={n.label} style={{ display: "flex", alignItems: "center", gap: 4, fontSize: 10 }}>
                <span style={{ width: 8, height: 8, borderRadius: "50%", background: n.cor, display: "inline-block" }} />
                <span style={{ color: n.cor, fontWeight: 700 }}>{n.label}</span>
                <span className="muted">
                  {n.min === 0 ? `< ${n.max}` : n.max >= GAUGE_MAX ? `> ${n.min}` : `${n.min}–${n.max}`}
                </span>
              </div>
            ))}
          </div>
        </div>

        <div className="card">
          <h3>📊 Média do Dia</h3>
          <Velocimetro
            value={gauge2}
            titulo="Total do dia: Peso ÷ Sacos Produzidos"
            detalhe={
              totalSacos > 0
                ? `${totalPeso.toLocaleString("pt-BR", { maximumFractionDigits: 1 })} kg ÷ ${totalSacos} sacos`
                : "Sem dados para hoje"
            }
          />
        </div>
      </div>

      {/* ── BLOCO 3: GRÁFICO HISTÓRICO ── */}
      <div className="card" style={{ marginBottom: 24 }}>
        <div style={{ display: "flex", alignItems: "center", justifyContent: "space-between", flexWrap: "wrap", gap: 10, marginBottom: 16 }}>
          <h3 style={{ margin: 0 }}>📈 Histórico — kg/saco produzido</h3>
          <div style={{ display: "flex", gap: 8 }}>
            <button className={`btn mini ${filtro === "diario" ? "" : "sec"}`}
              onClick={() => setFiltro("diario")}>Diário</button>
            <button className={`btn mini ${filtro === "mensal" ? "" : "sec"}`}
              onClick={() => setFiltro("mensal")}>Mensal</button>
          </div>
        </div>

        {dadosGrafico.length === 0 ? (
          <div className="muted" style={{ padding: "40px 0", textAlign: "center" }}>
            Nenhum dado. Salve romaneios para visualizar o histórico.
          </div>
        ) : (
          <div style={{ height: 320 }}>
            <ResponsiveContainer>
              <BarChart data={dadosGrafico} margin={{ top: 8, right: 16, left: -10, bottom: 50 }}>
                <CartesianGrid stroke="#26305c" strokeDasharray="3 3" vertical={false} />
                <XAxis dataKey="periodo" tick={{ fill: "#8b96c0", fontSize: 11 }}
                  angle={-35} textAnchor="end" interval={0} />
                <YAxis tick={{ fill: "#8b96c0", fontSize: 11 }} domain={[0, "auto"]} />
                <Tooltip contentStyle={TOOLTIP}
                  formatter={(v) => [`${v} kg/saco`, "Média"]} />
                <ReferenceLine y={15} stroke="#6366f1" strokeDasharray="6 3"
                  label={{ value: "Ref: 15", fill: "#a5b4fc", fontSize: 11, position: "insideTopRight" }} />
                {dadosGrafico.map((d, i) => null)}
                <Bar dataKey="media" name="kg/saco" radius={[6, 6, 0, 0]}
                  fill="#06b6d4"
                  label={false}
                />
              </BarChart>
            </ResponsiveContainer>
          </div>
        )}
        <div className="muted" style={{ fontSize: 11, marginTop: 8 }}>
          — — Linha de referência em 15 sacos
        </div>
      </div>

      {/* Tabela de registros */}
      {registros.length > 0 && (
        <div className="card">
          <h3>🗂️ Registros Recentes</h3>
          <div style={{ overflowX: "auto" }}>
            <table className="tab">
              <thead>
                <tr>
                  <th>Data</th><th>Fornecedor</th><th>Região</th><th>Tipo</th>
                  <th>Peso Líq. (kg)</th><th>Sacos</th><th>Sacos Prod.</th><th>Média (kg/saco)</th>
                </tr>
              </thead>
              <tbody>
                {registros.slice(0, 30).map((r) => {
                  const nivel = nivelPara(Number(r.media_tirada));
                  return (
                    <tr key={r.id}>
                      <td>{new Date(r.data + "T12:00:00").toLocaleDateString("pt-BR")}</td>
                      <td>{r.fornecedor}</td>
                      <td>{r.regiao || "—"}</td>
                      <td>{r.tipo_batata || "—"}</td>
                      <td>{Number(r.peso_liquido).toLocaleString("pt-BR")}</td>
                      <td>{r.qtd_sacos}</td>
                      <td>{r.qtd_sacos_produzidos}</td>
                      <td>
                        <span style={{ color: nivel.cor, fontWeight: 700 }}>
                          {Number(r.media_tirada).toFixed(2)}
                          <span style={{ fontSize: 10, marginLeft: 4 }}>({nivel.label})</span>
                        </span>
                      </td>
                    </tr>
                  );
                })}
              </tbody>
            </table>
          </div>
        </div>
      )}
    </div>
  );
}
