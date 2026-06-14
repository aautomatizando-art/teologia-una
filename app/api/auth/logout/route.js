import { cookies } from "next/headers";
import { COOKIE } from "@/lib/auth";

export async function POST() {
  const jar = await cookies();
  jar.delete(COOKIE);
  return Response.json({ ok: true });
}
