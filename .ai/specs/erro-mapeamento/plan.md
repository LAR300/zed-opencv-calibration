# Plano — M2: erro de mapeamento e EME empírico

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`nucleo-offline`](../nucleo-offline/plan.md), etapas 4–7: YAML, projeção, sintético e calibração.

## Etapas

- [x] 1. **Grade e desprojeção controlada** (R1, R2). `pixel_grid`, `unproject` (`undistortPointsIter`, 100 iterações, 1e-12) e a exclusão por ida e volta (> 1e-3 px).

  Especificação original da etapa: Grade configurável; `undistortPointsIter`
  com critério reforçado; ida e volta com A e exclusão dos pontos acima do limiar.
  - Testes primeiro: sem distorção, a ida e volta é exata; num modelo com k1 extremo, os pontos
    excluídos ficam só na borda e são contados.
  - **Pronto quando:** `pytest` verde.
- [x] 2. **Erro A → B** (R1, R3). `mapping_error`: RMS por ponto, máximo, K por coordenada, mapa e número de pontos excluídos; `coverage_mask` para a região coberta (R1b).

  Especificação original da etapa: RMS e máximo nas duas convenções, mais o mapa por ponto.
  - Testes primeiro: os casos "idênticas" e "fx perturbado" dos critérios.
  - **Pronto quando:** `pytest` verde.
- [x] 3. **Variante com rotação** (R4). `least_squares` sobre 3 parâmetros de rotação dos raios. O teste de cx deslocado cobre o caso; a rotação imposta de 0,1° não foi testada separadamente.

  Especificação original da etapa: `least_squares` sobre 3 parâmetros de rotação.
  - Testes primeiro: o caso "cx deslocado"; uma rotação imposta de 0,1° entre A e B (raios
    rodados) é recuperada até 1e-4°.
  - **Pronto quando:** `pytest` verde.
- [x] 4. **Referência da condição** (R5). `reference_calibration`: um modelo do mesmo tipo ajustado à projeção média de uma grade de raios, com R pela média cordal e T pela média simples, mais a média simples dos parâmetros para comparar. A calibração conjunta (todas as imagens juntas) é o `zedcal.calibrate` com as detecções juntas e não foi embutida aqui.

  Especificação original da etapa: A média no espaço da imagem com o ajuste `rational8`, a
  média de parâmetros e a calibração conjunta, via núcleo.
  - Testes primeiro: o critério das 10 calibrações sintéticas; o ajuste à média de 10 cópias de um
    mesmo modelo devolve esse modelo.
  - **Pronto quando:** `pytest` verde.
- [x] 5. **EME empírico, matriz de pares e F** (R6, R7). `empirical_eme`, `pair_matrix` e `percentile_of`; 11 testes.

  Especificação original da etapa:
  - Testes primeiro: n = 10 dá 45 pares; o percentil de F é calculado sobre os 10 valores e
    testado com um conjunto conhecido.
  - **Pronto quando:** `pytest` verde.
- [x] 6. **Comando, figuras e config** (R3, R8). `python3 -m zedcal.m2_command --config ...`.

  **Dados reais (2026-11-03),** as 3 calibrações AprilGrid existentes (`v2`, acrílico, controles
  fixos; condições diferentes, só como demonstração) e F, com a região coberta pelas detecções da
  `v2` e da `camera_settings`:

  | | Esquerda | Direita |
  |---|---|---|
  | EME sem rotação | 1,36 px | 1,43 px |
  | EME com rotação | 0,44 px | 0,41 px |
  | F × referência, sem / com rotação | 2,65 / 2,50 px (p100) | 2,74 / 2,58 px (p100) |

  - As nossas calibrações concordam entre si a menos de algo que a rotação compensa (ponto
    principal).
  - F fica fora da distribuição, por uma diferença que a rotação não compensa. É coerente com o
    fy/fx: +0,28 % nas nossas contra +0,005 % na fábrica, ~1,9 px na borda.
  - Somado ao BR ≈ 0,7 do M1, isso aponta o viés para o nosso lado (alvo, detecção ou poses). Vai
    para o piloto.
  - A referência ajustou a projeção média com 0,0002–0,0004 px.

  Especificação original da etapa: `python3 -m zedcal.m2 --config ...`; mapas de calor
  por par e por câmera.
  - **Pronto quando:** roda com as calibrações reais existentes e F, e produz a matriz e os mapas.

## Riscos

- **O ajuste `rational8` à média pode ser mal condicionado** nos coeficientes altos. Inicializar
  com a média de parâmetros e conferir o resíduo do ajuste. Esse resíduo também vai para o
  relatório.
