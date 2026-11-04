# Spec — M1: qualidade do ajuste (RMS, mapa de resíduos e Bias Ratio)

> **O quê, por quê e condições de implementação.** Origem:
> [`brief.md`](../../product/brief.md) R4, M1. Base teórica:
> [`referencias.md`](../../product/referencias.md) §A (Hagemann et al., IJCV 2022). Depende de
> [`nucleo-offline`](../nucleo-offline/spec.md).

## Problema

O RMS de reprojeção não distingue **ruído** de **viés de modelo**. Dois exemplos:
- **Escala errada do alvo:** na Parte 1 (§3) de `aprilgrid-target/validation.md`, o RMS foi
  idêntico com 25,4 mm e com 23,8 mm.
- **Os nossos números:** as calibrações AprilGrid têm RMS ~0,31 px e um fy/fx +0,2–0,3 % sem
  explicação. Na literatura, o RMS típico no ar fica em 0,03–0,1 px (Shortis 2015). O ruído do
  detector medido por Hagemann foi σ_d ≈ 0,03 px.

O brief pede M1 como **filtro, não validação**:
- RMS mono e estéreo, com o filtro < 0,5 px;
- mapa de resíduos por região da imagem;
- **Bias Ratio (BR)**, que estima a fração do erro de ajuste explicada por viés sistemático.

## Contexto

- **Hagemann (2022) define o BR assim**, com N contando **coordenadas** (2 por canto) e N_P os
  parâmetros livres:
  - MSE_calib = σ_d²(1 − N_P/N) + ε_bias²(1 − N_P/N);
  - σ_d é estimado por "alvos virtuais" de 4 cantos exclusivos, com a pose reestimada (6 graus de
    liberdade) e os intrínsecos fixos: σ̂_d² = MSE_v / (1 − 6/8) = 4·MSE_v, agregado com MAD;
  - ε̂_bias² = max(MSE_calib/(1 − N_P/N) − σ̂_d², 0);
  - BR = ε̂_bias²·(1 − N_P/N)/MSE_calib.

  A proposta do brief confere com o artigo (`referencias.md` §A.3).
- **Armadilhas que o artigo e a auditoria apontam** (`referencias.md` §A.3; `auditoria.md` §3.4):
  1. contar por coordenada; o RMS do OpenCV é por canto;
  2. o MAD dá σ, não σ²;
  3. robusto dos dois lados;
  4. mesmo detector;
  5. PnP de 4 pontos ambíguo, resolvido com `IPPE_SQUARE` + LM.
- **No AprilGrid, cada tag é um conjunto exclusivo de 4 cantos**, então é um alvo virtual natural.
  No xadrez, os alvos virtuais são quadrados 2×2 de cantos sem cantos compartilhados, como no
  artigo.

## Requisitos

- **R1. RMS.** Para uma calibração e as detecções que a geraram, o módulo reporta, na convenção
  `per_corner_2d` e em MSE por coordenada:
  - o RMS mono E e D com pose livre por vista;
  - o RMS por câmera no ajuste estéreo;
  - o RMS estéreo.

  Também indica se cada um passa no filtro configurado (padrão 0,5 px).
- **R2. Mapa de resíduos por região.** Para cada câmera, numa grade configurável (padrão 8×8,
  a mesma do R3 do brief), o mapa traz, por célula:
  - o número de cantos;
  - o RMS;
  - a média do resíduo com sinal em x e em y, que revela padrões sistemáticos.

  Saídas: um arquivo numérico (`.npz` ou `.json`) e figuras: o mapa de RMS e o campo vetorial do
  resíduo médio.
- **R3. Bias Ratio por câmera,** com as fórmulas do Contexto e:
  - MSE_calib é o da calibração mono daquela câmera com pose livre. N e N_P contam coordenadas e
    parâmetros livres: intrínsecos + 6 por vista;
  - alvos virtuais por tag (AprilGrid) ou por quadrado 2×2 exclusivo (xadrez), com os intrínsecos
    da calibração avaliada fixos;
  - σ̂_d² = 4 × MSE robusto dos resíduos com sinal, x e y juntos, de todos os alvos virtuais;
  - MSE_calib **robusto**, pelo mesmo estimador, e também na forma comum. O BR usa o robusto nos
    dois lados (armadilha 3). A forma comum fica no relatório, para comparar;
  - **MSE robusto** = média quadrática depois de descartar |r − mediana| > 5 σ_MAD.
    - **Desvio da fórmula do brief,** decidido na implementação (2026-11-03). O (1,4826·MAD)² saiu
      **4 % baixo** nos alvos virtuais: os 4 cantos de uma tag têm alavancagens diferentes na pose,
      os resíduos têm variâncias diferentes, e o MAD dessa mistura fica abaixo da média quadrática
      que o fator 4 supõe.
    - Em dados sintéticos sem viés, isso deu BR = 0,08, em vez de ≈ 0. A média aparada deu σ̂_d
      a 0,5 % e continua robusta a outliers grosseiros.
- **R4. Saída do BR:** BR, √BR, ε_bias (px), σ̂_d (px), MSE_calib (robusto e comum), N, N_P e o
  número de alvos virtuais usados e descartados.
- **R5. Alvos virtuais descartados.** Uma tag cuja pose não converge ou cujo resíduo é
  degenerado é descartada e contada. Uma tag com lado < 10 px já vem filtrada pelo detector.
- **R6. Configuração** em YAML: tamanho da grade, filtro de RMS e calibração e detecções de
  entrada. A saída vai para `captura_<XX>/metricas/m1_<calib>.json` e as figuras.

## Condições de implementação

- **O MSE_calib usa o modelo mono, com pose livre por vista,** inclusive para avaliar intrínsecos
  vindos de um ajuste estéreo. O BR de Hagemann é definido para uma câmera, e N_P fica bem
  definido assim. O caso estéreo seria uma extrapolação (`referencias.md` §A.3, item 6), fora
  deste spec.
- **A ambiguidade do IPPE é resolvida pelo menor resíduo** e a pose é refinada com
  `solvePnPRefineLM`. Uma pose mal convergida infla σ_d e esconde o viés (armadilha 5).
- **Sem detecção em Python:** os cantos vêm do `detections.json`.

## Não-objetivos

- O BR estéreo.
- A calibração robusta com kernel de Cauchy dentro da otimização (Hagemann a recomenda). Aqui o
  estimador robusto é aplicado aos resíduos. A calibração robusta pode entrar como variante no
  núcleo, se o piloto mostrar outliers relevantes.
- Usar o BR como critério de aceite automático: o limiar é decisão do usuário (ver Perguntas).

## Critérios de aceitação

> Validação: ver as etapas de [`plan.md`](plan.md), registro de 2026-11-03.

- [x] **Dados sintéticos sem viés** (σ = 0,1 px, modelo verdadeiro = modelo ajustado): BR < 0,05 e
      σ̂_d a menos de 10 % de 0,1 px (com o estimador aparado de R3).
- [x] **Distorção omitida:** dados gerados com o racional e k1 = −0,30, ajustados sem distorção:
      BR > 0,5.
- [x] **Alvo não plano:** flecha de 1 mm no centro do alvo de 660 mm. BR maior que no caso sem
      viés, com o mesmo ruído.
- [x] **Convenção:** tratar o RMS² do OpenCV como MSE por coordenada nos dados sem viés dá
      BR ≈ 0,5. Esse teste documenta a armadilha 1.
- [x] O mapa de resíduos de dados sintéticos com um viés vertical imposto (Δy = +0,2 px na metade
      superior da imagem) mostra o sinal na média de y das células certas.
- [x] Nas detecções de `2026-11-03_camera_settings`, com o YAML do C++, o módulo produz o relatório
      e as figuras sem erro: BR 0,71 / 0,66; ver plan, etapa 6.

## Perguntas em aberto

- NEEDS CLARIFICATION: **Limiar de BR para um alerta.** A literatura lida não fixa um número.
  Proposta: só reportar no piloto; o usuário define um limiar, se quiser, depois de ver os valores
  do xadrez e do AprilGrid.
