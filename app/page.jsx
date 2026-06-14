import { Suspense } from "react";
import LoginForm from "@/components/LoginForm";

// Página 1 — Login da área de PRODUÇÃO
export default function PaginaLoginProducao() {
  return (
    <Suspense>
      <LoginForm
        area="producao"
        icone="🏭"
        titulo="Área de Produção"
        subtitulo="Acompanhe ordens de produção, paletes, perdas e qualidade"
        destino="/producao"
      />
    </Suspense>
  );
}
