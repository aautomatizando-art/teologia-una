// Verifica se um lançamento (campo "linhas" tipo "1,2") pertence a alguma das linhas informadas
export function pertenceALinhas(item, linhasAlvo) {
  const ls = (item.linhas || "")
    .split(",")
    .map((s) => parseInt(s.trim(), 10))
    .filter((n) => !isNaN(n));
  return ls.some((l) => linhasAlvo.includes(l));
}

// Agrupa uma lista de lançamentos {tipo, quantidade} somando por tipo
export function agruparPorTipo(itens) {
  const m = {};
  for (const item of itens || []) m[item.tipo] = (m[item.tipo] || 0) + item.quantidade;
  return Object.entries(m).map(([tipo, quantidade]) => ({ tipo, quantidade }));
}
