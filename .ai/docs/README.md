# Documentação

| Documento | Para quem | Conteúdo |
|---|---|---|
| [`fluxo-calibracao.md`](fluxo-calibracao.md) | Quem vai calibrar | Visão geral: antes × agora, o fluxo completo, os experimentos |
| [`fluxo-coleta.md`](fluxo-coleta.md) | Quem vai à bancada | Preparação, sessão, coleta com o roteiro, o que fica gravado, aceitar ou refazer, calibrar sem a câmera |
| [`fluxo-analise-offline.md`](fluxo-analise-offline.md) | Quem vai avaliar | Os comandos de análise, com um exemplo completo nas capturas gravadas e como ler cada saída |
| [`funcionalidades.md`](funcionalidades.md) | Quem precisa justificar | Cada funcionalidade e métrica, para que serve, de onde veio, com as referências |
| [`bench.md`](bench.md) | Quem desenvolve | Ambiente do container, permissões, armadilhas, regressão e utilitários de teste |

Outros registros em `.ai/`:
- `product/`: o brief metodológico, a visão, o backlog, as referências e as pendências de bancada;
- `specs/<módulo>/`: o spec, o plano e as decisões de cada módulo implementado;
- `auditoria.md`: a auditoria das ferramentas originais contra o brief (fase 0).

Os resultados de análise citados nos specs (em `calib_runs/piloto/`, `testes_persistencia/`,
`demo/` etc.) foram apagados na limpeza de 2026-11-09. As capturas continuam em `calib_runs/`
(`calib_runs/README.md`), e os resultados se refazem com `fluxo-analise-offline.md` e com os
scripts de `analysis/piloto/`.
