import { NextResponse } from "next/server";
import { verificarToken, COOKIE } from "@/lib/auth";

// Regras de acesso:
//  /producao            -> papel "producao" ou "admin"
//  /estoque, /compras,
//  /rastreio, /backup,
//  /entrega, /expedicion -> papel "estoque" ou "admin"
const REGRAS = [
  { prefixo: "/producao", papeis: ["producao", "admin"], login: "/" },
  { prefixo: "/estoque", papeis: ["estoque", "admin"], login: "/login-estoque" },
  { prefixo: "/compras", papeis: ["estoque", "admin"], login: "/login-estoque" },
  { prefixo: "/rastreio", papeis: ["estoque", "admin"], login: "/login-estoque" },
  { prefixo: "/backup", papeis: ["estoque", "admin"], login: "/login-estoque" },
  { prefixo: "/entrega", papeis: ["estoque", "admin"], login: "/login-estoque" },
  { prefixo: "/expedicion", papeis: ["estoque", "admin"], login: "/login-estoque" },
];

export async function middleware(req) {
  const { pathname } = req.nextUrl;
  const regra = REGRAS.find((r) => pathname.startsWith(r.prefixo));
  if (!regra) return NextResponse.next();

  const token = req.cookies.get(COOKIE)?.value;
  const payload = token ? await verificarToken(token) : null;

  if (!payload || !regra.papeis.includes(payload.role)) {
    const url = req.nextUrl.clone();
    url.pathname = regra.login;
    url.searchParams.set("redirect", pathname);
    return NextResponse.redirect(url);
  }
  return NextResponse.next();
}

export const config = {
  matcher: [
    "/producao/:path*",
    "/estoque/:path*",
    "/compras/:path*",
    "/rastreio/:path*",
    "/backup/:path*",
    "/entrega/:path*",
    "/expedicion/:path*",
  ],
};
