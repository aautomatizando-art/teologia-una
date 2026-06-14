import { Suspense } from "react";
import LoginForm from "@/components/LoginForm";

// Página 3 — Login da área de ESTOQUE (redireciona para a página 4)
export default function PaginaLoginEstoque() {
  return (
    <Suspense>
      <LoginForm
        area="estoque"
        icone="📦"
        titulo="Área de Estoque"
        subtitulo="Controle de estoque, reservas, compras e rastreio de pedidos"
        destino="/estoque"
      />
    </Suspense>
  );
}
