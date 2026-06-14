import { createHash } from "crypto";
import { cookies } from "next/headers";
import { getSupabase, supabaseIndisponivel } from "@/lib/supabase";
import { criarToken, COOKIE } from "@/lib/auth";

export async function POST(req) {
  const supabase = getSupabase();
  if (!supabase) return supabaseIndisponivel();

  const { username, password, area } = await req.json();
  if (!username || !password) {
    return Response.json({ error: "Informe usuário e senha." }, { status: 400 });
  }

  const hash = createHash("sha256").update(password).digest("hex");
  const { data: user, error } = await supabase
    .from("app_users")
    .select("id, username, role")
    .eq("username", username.trim().toLowerCase())
    .eq("password_hash", hash)
    .maybeSingle();

  if (error) return Response.json({ error: error.message }, { status: 500 });
  if (!user) return Response.json({ error: "Usuário ou senha inválidos." }, { status: 401 });

  // Cada área de login exige o papel correspondente (admin entra em ambas)
  const exigido = area === "estoque" ? ["estoque", "admin"] : ["producao", "admin"];
  if (!exigido.includes(user.role)) {
    return Response.json(
      { error: `Este usuário não tem acesso à área de ${area === "estoque" ? "estoque" : "produção"}.` },
      { status: 403 }
    );
  }

  const token = await criarToken(user);
  const jar = await cookies();
  jar.set(COOKIE, token, {
    httpOnly: true,
    secure: process.env.NODE_ENV === "production",
    sameSite: "lax",
    path: "/",
    maxAge: 60 * 60 * 12,
  });

  return Response.json({ ok: true, role: user.role });
}
