"use client";
import { usePathname, useRouter } from "next/navigation";

const LINKS = [
  { href: "/producao", rotulo: "📊 Produção" },
  { href: "/estoque", rotulo: "📦 Estoque" },
  { href: "/compras", rotulo: "📋 Pedidos" },
  { href: "/expedicion", rotulo: "🚚 Expedição" },
  { href: "/entrega", rotulo: "✅ Entrega" },
  { href: "/rastreio", rotulo: "📍 Rastreio" },
  { href: "/backup", rotulo: "💾 Backup" },
];

export default function TopBar() {
  const pathname = usePathname();
  const router = useRouter();

  async function sair() {
    await fetch("/api/auth/logout", { method: "POST" });
    router.push("/");
  }

  return (
    <div className="topbar">
      <div className="brand">
        <div className="logo">🏭</div>
        <span>Fábrica • Dashboard</span>
      </div>
      <nav className="nav">
        {LINKS.map((l) => (
          <a key={l.href} href={l.href} className={pathname.startsWith(l.href) ? "ativo" : ""}>
            {l.rotulo}
          </a>
        ))}
      </nav>
      <button className="btn-sair" onClick={sair}>Sair ⏻</button>
    </div>
  );
}
