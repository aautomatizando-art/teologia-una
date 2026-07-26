import { Suspense } from "react";
import LoginForm from "@/components/LoginForm";

export default function PaginaLoginRendimento() {
  return (
    <Suspense>
      <LoginForm
        area="rendimento"
        icone="🥔"
        titulo="Controle de Rendimento"
        subtitulo="Romaneio de batata e análise de média tirada"
        destino="/rendimento"
      />
    </Suspense>
  );
}
