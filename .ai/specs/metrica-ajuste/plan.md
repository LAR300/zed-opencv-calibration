# Plano — M1: RMS, mapa de resíduos e Bias Ratio

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`nucleo-offline`](../nucleo-offline/plan.md), etapas 1–7: leitura, resíduos, PnP, sintético e
  calibração.

## Etapas

- [x] 1. **Estimadores** (R3). `zedcal/m1.py`: `robust_sigma` (1,4826·MAD), `robust_mse` (média
  quadrática aparada em 5 σ_MAD, ver o spec R3), `mse_plain` e `dof_factor`.
  - Testes: com 5 % de outliers grosseiros, o `robust_mse` muda < 3 % e o comum explode.
  - O MAD sozinho muda ~6 % com essa contaminação: o limite de 3 % do plano original valia para o
    estimador, não para o MAD.

  Especificação original da etapa: `robust_sigma(residuals)` = 1,4826·MAD dos componentes com sinal;
  `mse_plain`; `dof_factor(N, N_P)`.
  - Testes primeiro: com uma gaussiana de σ = 0,2 e 10⁵ amostras, `robust_sigma` = 0,2 ± 1 %; com
    5 % de outliers de 5 px, o estimador robusto muda < 3 % e o comum muda muito.
  - **Pronto quando:** `pytest` verde.
- [ ] 2. **RMS** (R1). Pose livre por vista, ajuste estéreo e RMS estéreo, com o filtro.
  - Testes primeiro: dados sintéticos sem ruído dão ≈ 0; com σ, o RMS por canto fica ≈ σ√2; o
    filtro reprova acima do limiar.
  - **Pronto quando:** `pytest` verde.
- [x] 3. **Alvos virtuais e σ_d** (R3, R5). `virtual_target_residuals`: cada tag com a sua
  pose (IPPE + LM, intrínsecos fixos); no xadrez, quadrados 2×2 exclusivos. Falhas são descartadas
  e contadas.
  - Sem viés: σ̂_d a menos de 10 % de 0,1 px (medido: 0,5 % com o estimador aparado), e nenhum alvo
    virtual descartado.

  Especificação original da etapa: Pose por tag com `IPPE_SQUARE` + LM e a menor das duas
  soluções; quadrados 2×2 exclusivos no xadrez; descarte contado.
  - Testes primeiro: em dados sintéticos com σ = 0,1, σ̂_d fica a menos de 10 %; uma tag com os
    cantos trocados de ordem é descartada e contada.
  - **Pronto quando:** `pytest` verde.
- [x] 4. **Bias Ratio** (R3, R4). `bias_ratio`: BR, √BR, ε_bias, σ̂_d, MSE robusto e comum, N, N_P,
  alvos virtuais usados e descartados. Os quatro casos sintéticos passam (tests/test_m1.py, 9
  testes):
  - sem viés, BR < 0,05;
  - distorção omitida (k1 = −0,30 ajustado sem distorção), BR > 0,5;
  - alvo com flecha de 1 mm, BR maior que sem viés;
  - RMS² do OpenCV como MSE por coordenada, BR ≈ 0,5.

  A decomposição fecha.

  Especificação original da etapa:
  - Testes primeiro:
    - os quatro casos sintéticos dos critérios de aceitação: sem viés, distorção omitida, alvo não
      plano e convenção errada;
    - a decomposição MSE_calib = (σ̂_d² + ε̂²)(1 − N_P/N) fecha quando ε̂² > 0.
  - **Pronto quando:** `pytest` verde nos quatro casos.
- [ ] 5. **Mapa de resíduos** (R2). Grade por câmera, com contagem, RMS e média de x e y; figuras
  de RMS e do campo vetorial.
  - Testes primeiro: o caso do viés vertical imposto; uma célula sem cantos aparece como `NaN`, não
    como 0.
  - **Pronto quando:** `pytest` verde e as figuras são geradas para um caso sintético.
- [ ] 6. **Comando e saída** (R6). `python3 -m zedcal.m1 --config ...`, com a saída em
  `metricas/`.
  - **Pronto quando:** roda nas detecções de `2026-11-03_camera_settings` com o YAML do C++ e
    produz o JSON e as figuras; a config é copiada para a saída.

## Riscos

- **Com poucos cantos por tag a grande distância, a pose de uma tag isolada é mal condicionada.**
  Isso infla σ_d. Mitigação: reportar σ̂_d também por faixa de tamanho da tag em px, para ver se
  depende da distância.
