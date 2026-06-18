"use client";
import { useEffect, useState } from "react";

// Pequeno indicador de conexão do ESP32.
// Uso: <Esp32Status painel={1} />
export default function Esp32Status({ painel }) {
  const [status, setStatus] = useState(null); // null=carregando, true=online, false=offline

  useEffect(() => {
    async function checar() {
      try {
        const j = await fetch(`/api/esp32/ping?painel=${painel}`).then((r) => r.json());
        const d = (j.dispositivos || []).find((x) => x.painel === painel);
        setStatus(d ? d.conectado : false);
      } catch {
        setStatus(false);
      }
    }
    checar();
    const id = setInterval(checar, 5000);
    return () => clearInterval(id);
  }, [painel]);

  if (status === null) return null;

  return (
    <span title={status ? "ESP32 conectado" : "ESP32 desconectado"} style={{ display: "inline-flex", alignItems: "center", gap: 4, fontSize: 11, fontWeight: 600, color: status ? "#4ade80" : "#6b7280", marginLeft: 8 }}>
      <span style={{
        display: "inline-block", width: 8, height: 8, borderRadius: "50%",
        background: status ? "#22c55e" : "#4b5563",
        boxShadow: status ? "0 0 6px #22c55e" : "none",
      }} />
      {status ? "BOTOEIRA ON" : "BOTOEIRA OFF"}
    </span>
  );
}
