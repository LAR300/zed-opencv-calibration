# Plano — Verificação de cobertura

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`nucleo-offline`](../nucleo-offline/plan.md), etapas 1–6.
- A definição de ângulos de [`roteiro-poses`](../roteiro-poses/spec.md) (condições de
  implementação). O teste de paridade (etapa 3) precisa do `common/pose_script/` C++.

## Etapas

- [x] 1. **Grade de densidade** (R1, R2). `zedcal/coverage.py` (`density`): a célula de um canto é
  `floor(x / largura_da_célula)`, então um canto exatamente no limite cai na célula da direita (ou
  de baixo); cantos fora da imagem não contam.

  Especificação original da etapa: Contagem por célula e por câmera, critério N e células de
  canto.
  - Testes primeiro: uma grade sintética conhecida dá as contagens exatas; um canto no limite de
    duas células cai numa só, com a regra documentada.
  - **Pronto quando:** `pytest` verde.
- [x] 2. **Inclinações** (R3, R4). `view_geometry` (gêmeo Python do `viewGeometry` C++), `views` e
  `check`. Critério: pelo menos K poses além de ±30° no eixo e, com `both_signs`, ao menos uma de
  cada sinal. Os testes acharam um erro do `pose` do núcleo com o alvo inclinado 35° na borda da
  imagem, corrigido (nota em [`nucleo-offline`](../nucleo-offline/plan.md), etapa 5).

  Especificação original da etapa: Pose PnP, os dois ângulos, histogramas e critério com K e sinais.
  - Testes primeiro: poses sintéticas com ângulos conhecidos dão erro < 0,1°; o critério reprova a
    falta de um eixo ou de um sinal.
  - **Pronto quando:** `pytest` verde.
- [x] 3. **Paridade com o C++.** `tests/fixtures/pose_geometry.json`, lido por
  `view_geometry_matches_the_shared_fixture` (C++, 162/162) e por
  `test_angles_match_the_shared_fixture` (Python).

  Especificação original da etapa: Um fixture com a pose sintética e os ângulos esperados é usado
  pelos dois testes, o C++ e o Python.
  - **Pronto quando:** os dois passam com o mesmo fixture.
- [x] 4. **Relatório e código de saída** (R5–R7). `python3 -m zedcal.coverage_command --config
  <yaml>`, como os outros comandos (detecções, calibração para a pose, pasta de saída, critérios).
  Grava `coverage.json` (contagens, cantos da imagem, geometria por vista com fração da largura e
  distância, histogramas, motivos) e `coverage.png`. Código 0 aprovada, 1 reprovada, 2 erro de
  configuração. 7 testes em `test_coverage.py`.

  Especificação original da etapa: `python3 -m zedcal.coverage <captura>`, com JSON,
  figuras e código de saída.
  - **Pronto quando:** os três casos sintéticos dos critérios dão os motivos certos e os códigos
    0 e ≠ 0.
- [ ] 5. **Capturas existentes.** Rodado em 2026-11-03 (calibração F para a pose; saídas em
  `calib_runs/piloto/resultados/coverage_*`). Falta só a escolha de N pelo usuário.

  Células abaixo de N (de 64), por câmera E / D:

  | Captura | N=1 | 3 | 5 | 8 | 10 | 15 | 20 | Mín. E / D | Inclinação h (+/−) | v (+/−) |
  |---|---|---|---|---|---|---|---|---|---|---|
  | AprilGrid v2 | 0/0 | 0/0 | 0/3 | 3/5 | 5/10 | 12/16 | 19/21 | 6 / 3 | 1/4 | 6/6 |
  | AprilGrid acrílico | 0/0 | 0/0 | 0/1 | 2/1 | 3/2 | 5/5 | 9/11 | 6 / 3 | 1/4 | 6/4 |
  | AprilGrid controles | 0/0 | 0/1 | 0/2 | 2/3 | 4/5 | 12/14 | 15/15 | 5 / 1 | 0/2 | 4/3 |
  | Xadrez 9×6 (referência) | 5/6 | — | — | — | — | — | — | 0 / 0 | 6/2 | 5/1 |

  - As células mais pobres são sempre as da borda direita da câmera direita e as dos cantos da
    imagem (o mínimo da direita está numa célula de canto nas três capturas AprilGrid).
  - Com N = 1, v2 e acrílico são aprovadas; a captura dos controles é reprovada só pela
    inclinação horizontal (nenhuma pose além de +30°). O xadrez é reprovado nas colunas das bordas.
  - Nenhuma dessas capturas seguiu o roteiro (risco abaixo).

  Especificação original da etapa: Rodar nas 3 capturas AprilGrid e registrar as distribuições de
  contagem no resultado do piloto, para a escolha de N.
  - **Pronto quando:** os mapas estão em `calib_runs/piloto/`, e N foi escolhido pelo usuário e
    gravado na config padrão.

## Riscos

- **N é escolhido depois das capturas existentes.** Elas não seguiram o roteiro, então a
  distribuição pode ser mais pobre que a de uma captura com roteiro. Rever N depois da primeira
  captura completa com roteiro.
