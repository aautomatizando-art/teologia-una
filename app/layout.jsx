import "./globals.css";

export const metadata = {
  title: "Fábrica • Dashboard de Produção e Estoque",
  description: "Controle de produtividade, estoque, compras e rastreio de pedidos",
};

export default function RootLayout({ children }) {
  return (
    <html lang="pt-BR">
      <body>{children}</body>
    </html>
  );
}
