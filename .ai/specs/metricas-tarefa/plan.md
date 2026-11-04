# Plano — M4: métricas de tarefa

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`nucleo-offline`](../nucleo-offline/plan.md) concluído.
- A sessão com `target_measured` (`persistencia-captura`). Para as capturas antigas, as medidas vêm
  de `validation.md` (tag de 88 mm, espaço de 26,4 mm) numa config.

## Etapas

- [x] 1. **Retificação nos pontos** (R1). `zedcal/m4.py`: `rectify` (sem zero disparity; fisheye
  com `balance = 1 − alpha`), `rectify_points` (`undistortPointsIter`, critério 100 iterações e
  1e-12) e `triangulate` (pontos no referencial da câmera esquerda, mm). 9 testes em
  `tests/test_m4.py`.

  Especificação original da etapa: `stereoRectify` sem zero disparity; cantos comuns
  retificados com o critério reforçado.
  - Testes primeiro: num par sintético ideal (câmeras paralelas), os pontos retificados coincidem
    com a projeção pinhole retificada.
  - **Pronto quando:** `pytest` verde.
- [x] 2. **Erro epipolar** (R2). y_E − y_D com sinal por canto; as estatísticas usam o valor
  absoluto. Com 0,05° em torno do eixo óptico, e = θ·(x − cx) com a inclinação certa (5 %).

  Especificação original da etapa: Estatísticas, frações abaixo dos limiares e mapas.
  - Testes primeiro: o caso "sem ruído" e o caso da rotação de 0,05°.
  - **Pronto quando:** `pytest` verde.
- [x] 3. **Triangulação e erro métrico** (R3). `metric_pairs`. O conjunto (c) são os pares
  extremos dos cantos visíveis ao longo de X, de Y e das duas diagonais do alvo (até 4 pares por
  vista). Um teste confere que o alvo medido é a verdade (alvo impresso 2 % maior: +2 % com o
  nominal, 0 com o medido).

  Especificação original da etapa: Os conjuntos (a), (b) e (c) com as distâncias
  medidas.
  - Testes primeiro: o caso sem ruído e o baseline 1 % errado; uma vista com só 2 tags gera os
    conjuntos possíveis sem erro.
  - **Pronto quando:** `pytest` verde.
- [x] 4. **Resíduo de plano** (R4). Com a flecha de 1 mm, o resíduo bate com o do domo
  verdadeiro até 1e-6 mm.

  Especificação original da etapa: Resíduo de plano.
  - Testes primeiro: o caso plano dá ≈ 0; uma flecha sintética de 1 mm dá um resíduo compatível com
    ela.
  - **Pronto quando:** `pytest` verde.
- [x] 5. **cx retificado e Δd** (R5). `disparity_bias`. **Desvio do spec:** a diferença de cx de
  P1/P2 não é o viés de disparidade (decisão registrada no spec, R5); o Δd principal vem da
  triangulação dos mesmos cantos.

  Especificação original da etapa: cx retificado e Δd.
  - Testes primeiro: o caso de Δd = 0,5 px.
  - **Pronto quando:** `pytest` verde.
- [x] 6. **Comando, agregação e figuras** (R6, R7). `python3 -m zedcal.m4_command --config ...`
  (`reference_calibration` obrigatório; alvo medido da config ou do `meta.json` da sessão, senão
  erro de configuração, código 2). Grava `m4.json` e `m4_<calibração>.png` (mapas do erro
  epipolar e do erro do passo).

  **Capturas do piloto (2026-11-03),** as 3 AprilGrid (89 vistas), alvo 88 / 26,4 mm
  (`validation.md`, Parte 1), calibrações `joint` do piloto, a referência e F. Saída em
  `calib_runs/piloto/resultados/m4/`. Agregado das 3 capturas:

  | Calibração | Epipolar RMS | < 0,25 px | < 0,1 px | Passo (média) | Vão (média) | Plano (mediana) |
  |---|---:|---:|---:|---:|---:|---:|
  | F | 0,211 px | 77 % | 36 % | +0,62 % | +0,71 % | 2,45 mm |
  | xadrez | 0,213 px | 77 % | 36 % | +0,42 % | +0,51 % | 2,49 mm |
  | v2 | 0,186 px | 83 % | 44 % | +0,40 % | +0,53 % | 2,49 mm |
  | acrílico | 0,182 px | 84 % | 45 % | −0,50 % | −0,56 % | 2,43 mm |
  | controles | 0,185 px | 84 % | 44 % | +0,35 % | +0,47 % | 2,47 mm |
  | referência | 0,176 px | 85 % | 46 % | +0,08 % | +0,14 % | 2,44 mm |

  - **O acrílico muda a escala em ~0,9 %.** O passo medido na captura com acrílico sai +0,82 a
    +1,04 % com as calibrações sem acrílico, e +0,06 % com a própria. A calibração com acrílico
    mede −0,80 % nas capturas sem acrílico. Confirma, agora em milímetros, a Parte 3 da validação
    e o resultado do M3.
  - **F mede ~0,4 % a mais** nas capturas sem acrílico (+0,36 e +0,43 % no passo), contra +0,05 a
    +0,22 % das nossas. O Δd efetivo de F para as nossas é pequeno (−0,05 a +0,2 px); a diferença
    vem mais da escala (f retificado de F 521,0 contra 524–525 px).
  - **O erro epipolar fica abaixo de 0,25 px (RMS) para todas,** mas só 77–85 % dos cantos estão
    abaixo de 0,25 px e 36–46 % abaixo de 0,1 px. F é a pior, e o pior lugar é a borda superior.
  - **O resíduo de plano é ruído de triangulação, não planeza do alvo:** cresce com Z² (0,7–0,9 mm
    abaixo de 0,5 m, 4,5 mm entre 1,2 e 2 m) e é igual para todas as calibrações. A planeza do
    alvo só pode ser lida nas vistas mais próximas.
  - **A dispersão por par é grande** (RMS do passo 1,4–2,8 % de 0,8 a 2 m): as médias e os vãos
    (c) são os números úteis, como o risco abaixo previa.

  Especificação original da etapa: `python3 -m zedcal.m4 --config ...`.
  - **Pronto quando:** roda nas capturas existentes com F e as calibrações existentes.

## Riscos

- **A triangulação a 1,4 m com pouco paralaxe é ruidosa.** O erro métrico dos lados das tags (a)
  fica dominado por ruído a grande distância. Por isso os três conjuntos são separados e as
  métricas são reportadas por faixa de distância.
