// POST /api/validar-senha — Validar senha ADM
export async function POST(req) {
  const { senha, tipo } = await req.json();

  if (!senha || !tipo) {
    return Response.json({ error: "senha e tipo são obrigatórios" }, { status: 400 });
  }

  try {
    // Validar senha ADM (padrão: "admin123")
    if (tipo === "adm" && senha === "admin123") {
      return Response.json({ ok: true }, { status: 200 });
    }

    return Response.json({ error: "Senha incorreta" }, { status: 401 });
  } catch (error) {
    return Response.json({ error: error.message }, { status: 500 });
  }
}
