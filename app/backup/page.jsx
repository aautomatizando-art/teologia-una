"use client";
import { useEffect, useState } from "react";
import TopBar from "@/components/TopBar";

export default function PaginaBackup() {
  const [backups, setBackups] = useState(null);
  const [erro, setErro] = useState("");
  const [ok, setOk] = useState("");
  const [gerando, setGerando] = useState(false);
  const [confirmar, setConfirmar] = useState(null); // arquivo a restaurar
  const [textoConfirma, setTextoConfirma] = useState("");
  const [senhaRestaurar, setSenhaRestaurar] = useState("");
  const [ocupado, setOcupado] = useState(false);

  async function carregar() {
    try {
      const res = await fetch("/api/backup");
      const j = await res.json();
      if (!res.ok) setErro(j.error || "Erro ao listar backups.");
      else setBackups(j.backups);
    } catch {
      setErro("Erro de conexão.");
    }
  }
  useEffect(() => { carregar(); }, []);

  async function gerar() {
    setGerando(true); setErro(""); setOk("");
    try {
      const res = await fetch("/api/backup", { method: "POST" });
      const j = await res.json();
      if (!res.ok) setErro(j.error || "Erro ao gerar backup.");
      else { setOk(`Backup "${j.arquivo}" gerado com ${j.total_registros} registros.`); carregar(); }
    } catch {
      setErro("Erro de conexão.");
    } finally {
      setGerando(false);
    }
  }

  async function excluir(arquivo) {
    if (!window.confirm(`Excluir o backup "${arquivo}"?`)) return;
    const senha = window.prompt("Digite a senha de Backup para excluir:");
    if (senha === null) return;
    setErro(""); setOk("");
    const res = await fetch(`/api/backup?arquivo=${encodeURIComponent(arquivo)}&senha=${encodeURIComponent(senha)}`, { method: "DELETE" });
    const j = await res.json();
    if (!res.ok) setErro(j.error || "Erro ao excluir.");
    else { setOk(`Backup "${arquivo}" excluído.`); carregar(); }
  }

  async function restaurar() {
    if (textoConfirma !== "RESTAURAR" || !senhaRestaurar) return;
    setOcupado(true); setErro(""); setOk("");
    try {
      const res = await fetch("/api/backup/restaurar", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ arquivo: confirmar, senha: senhaRestaurar }),
      });
      const j = await res.json();
      if (!res.ok) setErro(j.error || "Erro ao restaurar.");
      else setOk(`Estoque restaurado: ${j.restaurados} produtos voltaram ao estado do backup.`);
      setConfirmar(null);
      setTextoConfirma("");
      setSenhaRestaurar("");
    } catch {
      setErro("Erro de conexão.");
    } finally {
      setOcupado(false);
    }
  }

  return (
    <div className="shell">
      <TopBar />

      <div className="card" style={{ marginBottom: 18 }}>
        <h3>💾 Backup das informações</h3>
        <p className="muted" style={{ fontSize: 14, marginBottom: 16 }}>
          Gera uma cópia completa de todas as tabelas (produtos, estoque, ordens de produção,
          reservas, pedidos de compra, usuários) guardada com segurança no Supabase Storage.
          Você pode baixar o arquivo JSON quando precisar ou restaurar o estoque a partir de um backup.
        </p>
        {erro && <div className="erro">{erro}</div>}
        {ok && <div className="aviso" style={{ marginBottom: 14, borderColor: "rgba(34,197,94,.4)", color: "#86efac", background: "rgba(34,197,94,.1)" }}>✅ {ok}</div>}
        <button className="btn" onClick={gerar} disabled={gerando}>
          {gerando ? "Gerando backup..." : "💾 Gerar backup agora"}
        </button>
      </div>

      {!backups && !erro && <div className="spin" />}

      {backups && (
        <div className="card">
          <h3>📂 Backups guardados ({backups.length})</h3>
          {backups.length === 0 && <p className="muted">Nenhum backup ainda. Clique em "Gerar backup agora".</p>}
          {backups.length > 0 && (
            <table className="tab">
              <thead>
                <tr><th>Arquivo</th><th>Criado em</th><th>Tamanho</th><th style={{ textAlign: "right" }}>Ações</th></tr>
              </thead>
              <tbody>
                {backups.map((b) => (
                  <tr key={b.arquivo}>
                    <td style={{ fontWeight: 600 }}>{b.arquivo}</td>
                    <td>{b.criado_em ? new Date(b.criado_em).toLocaleString("pt-BR") : "—"}</td>
                    <td>{b.tamanho_kb} KB</td>
                    <td style={{ textAlign: "right", whiteSpace: "nowrap" }}>
                      <a className="btn mini sec" style={{ marginRight: 6 }} href={`/api/backup/download?arquivo=${encodeURIComponent(b.arquivo)}`}>
                        ⬇️ Baixar
                      </a>
                      <button className="btn mini sec" style={{ marginRight: 6 }} onClick={() => { setConfirmar(b.arquivo); setTextoConfirma(""); setSenhaRestaurar(""); }}>
                        ♻️ Restaurar estoque
                      </button>
                      <button className="btn mini sec" style={{ color: "#fca5a5" }} onClick={() => excluir(b.arquivo)}>
                        🗑️
                      </button>
                    </td>
                  </tr>
                ))}
              </tbody>
            </table>
          )}
        </div>
      )}

      {confirmar && (
        <div className="modal-fundo" onClick={(e) => e.target === e.currentTarget && setConfirmar(null)}>
          <div className="modal">
            <h2>♻️ Restaurar estoque do backup</h2>
            <p className="sub">
              As quantidades de estoque (e limites mín/máx) de todos os produtos voltarão ao estado
              salvo em <b>{confirmar}</b>. O histórico de reservas, pedidos e produção não é alterado.
            </p>
            <div className="campo">
              <label>Senha de Backup</label>
              <input type="password" value={senhaRestaurar} onChange={(e) => setSenhaRestaurar(e.target.value)} placeholder="Senha de Backup" autoFocus />
            </div>
            <div className="campo">
              <label>Digite RESTAURAR para confirmar</label>
              <input value={textoConfirma} onChange={(e) => setTextoConfirma(e.target.value.toUpperCase())} placeholder="RESTAURAR" />
            </div>
            <div className="acoes">
              <button className="btn sec" onClick={() => setConfirmar(null)}>Cancelar</button>
              <button className="btn" disabled={ocupado || textoConfirma !== "RESTAURAR" || !senhaRestaurar} onClick={restaurar}>
                {ocupado ? "Restaurando..." : "Confirmar restauração"}
              </button>
            </div>
          </div>
        </div>
      )}
    </div>
  );
}
