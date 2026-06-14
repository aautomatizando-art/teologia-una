"use client";
import { useState } from "react";
import { useRouter, useSearchParams } from "next/navigation";

export default function LoginForm({ area, titulo, subtitulo, icone, destino }) {
  const [username, setUsername] = useState("");
  const [password, setPassword] = useState("");
  const [erro, setErro] = useState("");
  const [carregando, setCarregando] = useState(false);
  const router = useRouter();
  const params = useSearchParams();

  async function entrar(e) {
    e.preventDefault();
    setErro("");
    setCarregando(true);
    try {
      const res = await fetch("/api/auth/login", {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ username, password, area }),
      });
      const j = await res.json();
      if (!res.ok) {
        setErro(j.error || "Falha no login.");
        return;
      }
      router.push(params.get("redirect") || destino);
    } catch {
      setErro("Erro de conexão. Tente novamente.");
    } finally {
      setCarregando(false);
    }
  }

  return (
    <div className="login-wrap">
      <form className="login-card" onSubmit={entrar}>
        <div className="icone">{icone}</div>
        <h1>{titulo}</h1>
        <p className="sub">{subtitulo}</p>
        {erro && <div className="erro">{erro}</div>}
        <div className="campo">
          <label>Usuário</label>
          <input
            value={username}
            onChange={(e) => setUsername(e.target.value)}
            placeholder="Digite seu usuário"
            autoFocus
            required
          />
        </div>
        <div className="campo">
          <label>Senha</label>
          <input
            type="password"
            value={password}
            onChange={(e) => setPassword(e.target.value)}
            placeholder="••••••••"
            required
          />
        </div>
        <button className="btn" style={{ width: "100%" }} disabled={carregando}>
          {carregando ? "Entrando..." : "Entrar →"}
        </button>
        {area === "producao" ? (
          <p className="aviso">Acesso à área de <b>Estoque</b>? <a href="/login-estoque" style={{ textDecoration: "underline" }}>Entre aqui</a></p>
        ) : (
          <p className="aviso">Acesso à área de <b>Produção</b>? <a href="/" style={{ textDecoration: "underline" }}>Entre aqui</a></p>
        )}
      </form>
    </div>
  );
}
