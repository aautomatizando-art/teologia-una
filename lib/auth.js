import { SignJWT, jwtVerify } from "jose";

const COOKIE = "dp_session";

function secret() {
  return new TextEncoder().encode(
    process.env.AUTH_SECRET || "dev-secret-dashboard-producao"
  );
}

export async function criarToken(user) {
  return await new SignJWT({ sub: String(user.id), username: user.username, role: user.role })
    .setProtectedHeader({ alg: "HS256" })
    .setIssuedAt()
    .setExpirationTime("12h")
    .sign(secret());
}

export async function verificarToken(token) {
  try {
    const { payload } = await jwtVerify(token, secret());
    return payload;
  } catch {
    return null;
  }
}

export { COOKIE };
