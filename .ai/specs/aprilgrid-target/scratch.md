# Scratch — Alvo AprilGrid

> Anotações de processo, efêmeras. O que virar decisão migra para `decisions.md`.

## Portão da etapa 8 (2026-10-20): falhou, causa encontrada

### Resultado do portão
- **Convenção do Kalibr: OK.** Na tela, o 0 aparece embaixo à esquerda, o 5 embaixo à direita, o 30
  em cima à esquerda e o 35 em cima à direita. O canto 0 de cada tag (quadradinho vermelho do
  OpenCV) fica embaixo à direita. Os erros ficam em 0,4–1,7 px, não em dezenas de pixels.
- **Precisão: falhou.** Com a calibração de fábrica, Left e Right ficaram em 0,38–1,66 px, crescendo
  com o tamanho do alvo na imagem.
- A geometria da placa foi medida pelo usuário e bate com o nominal (88 mm de tag e 26,4 mm de
  espaço). Ele também confirmou a regressão do xadrez no checker.

### Diagnóstico
Dados em `calib_runs/2026-10-20_aprilgrid_checker/raw/`: 19 pares crus capturados com
`calib_runs/tools/raw_capture`, mais a `calibration_parameters_raw` do SDK. Os scripts estão em
`calib_runs/tools/*.py`.

1. **Calibração de fábrica imprecisa na periferia: descartada.** A correlação do erro por tag com o
   raio na imagem fica entre 0,0 e +0,28.
2. **Causa real: os quadradinhos do Kalibr tocam as quinas das tags.** O detector do OpenCV acha as
   tags pelo contorno das manchas pretas; quando o fio cinza entre a tag e o quadradinho é
   classificado como preto, os dois viram uma mancha só. Dois efeitos:
   - **de longe:** a mancha não é um quadrilátero e a tag **não é detectada**. 11 de 19 quadros da
     esquerda tinham menos de 12 tags, com nitidez normal;
   - **de perto:** o vértice do contorno sai deslocado ~5 px para dentro da tag e o `SUBPIX` com
     janela 5 não chega ao cruzamento em X. Há cantos com 6–7 px de erro (recortes em `diag/`).
3. O sintético da etapa 6 não pegou isso porque desenha um fio branco perfeito de 1 px entre a tag
   e o quadradinho.

### Correções testadas nos 19 pares (calibração de fábrica fixa)
| Configuração | Vistas ≥ 12 tags | RMS por vista (mediana / máximo) | Cantos > 1,5 px |
|---|---:|---:|---:|
| Atual (SUBPIX, janela 5, C=7) | 18/38 | 0,54 / 1,35 | 24 |
| Refino no cruzamento em X, C=7 | 18/38 | 0,48 / 0,66 | 7 |
| Refino no cruzamento em X, C=20 | 34/38 | 0,44 / 0,63 | 6 |
| Refino no cruzamento em X, C=25 | 34/38 | 0,44 / 0,61 | 3 |
| Erosão + refino no cruzamento em X | 5/38 | — | — (pior) |

Refino no cruzamento em X: detecção sem refino do ArUco (`CORNER_REFINE_NONE`) e depois
`cornerSubPix` na imagem original com janela `max(2, round(0.15 × lado da tag))`. A janela alcança o
cruzamento e não chega aos bits de dados, que começam a 0,2 × lado.

`CORNER_REFINE_APRILTAG` detecta 0–2 tags nesta imagem e está descartado. Os quadros 3 e 14 falham
em todas as configurações porque estão borrados (nitidez ~230 contra ~1000).

### Autocalibração com o refino no cruzamento em X e C=25
17 vistas por câmera, modelo racional:

| Câmera | RMS | Perto | Longe | fx | fy | cx | cy |
|---|---:|---:|---:|---:|---:|---:|---:|
| Esquerda | 0,336 px | 0,38 | 0,29 | 533,92 | 535,38 | 633,39 | 369,67 |
| Direita | 0,331 px | 0,38 | 0,29 | 534,33 | 535,81 | 643,31 | 371,21 |

O resultado é igual partindo da fábrica ou do zero. A detecção e a placa estão boas: o erro que
resta de perto com a fábrica (0,45–0,6 px) é do modelo de fábrica, não da detecção.

### Observação fora do escopo
`petro_data_replayer/calibrate_intrinsics.py` usa o mesmo detector (SUBPIX, janela 5, C padrão). As
calibrações `zed_opencv_calibration_cesar_00{1,2}.yaml` do `petro_localization` têm RMS de 0,94 px,
o que é compatível com esse mesmo viés. Vale avisar a equipe.

## Portão da etapa 8, segunda rodada (2026-10-20): passou

Com o refino no cruzamento em X (D11), a calibração de fábrica e 8 poses, incluindo inclinadas e
cortadas (prints `ag2_P*.png`):

| | Mínimo | Máximo |
|---|---:|---:|
| Left | 0,277 px | 0,471 px |
| Right | 0,268 px | 0,447 px |
| Stereo | 0,292 px | 0,525 px |

Left e Right ficam abaixo de 0,5 px em todas as poses, o que atende o critério revisto e até o
original. As poses cortadas (P7, P8) são aceitas. A numeração continua correta.

## Etapa 12, validação no ar (2026-10-20)

### Primeira captura (`calib_runs/2026-10-20_ar_aprilgrid/`): reprovada em fx/fy
- RMS de 0,32 / 0,32 / 0,33 px e baseline de 119,91 mm passaram.
- fx/fy ficaram de +0,57 % a +0,78 % acima do xadrez e da fábrica, fora do limite.
- Causa: as poses tinham inclinação mediana de **7°** (só 9 de 35 passavam de 25°), e a coleta
  terminou pelo máximo de amostras, sem completar as métricas. Com pouca inclinação, focal e
  distância se confundem. A validação cruzada não distingue as calibrações: todas reprojetam
  dados independentes com qualidade parecida. Não é viés da detecção: a autocalibração do
  diagnóstico, com a mesma detecção, deu fx +0,02 %.

### Segunda captura (`calib_runs/2026-10-20_ar_aprilgrid_v2/`), seguindo o roteiro de movimentos: aprovada
- 29 amostras; a coleta terminou pelas métricas ("All scores are above threshold"). Inclinação
  mediana de 35° (15 de 29 acima de 25°), distâncias de 0,49 a 1,29 m.
- RMS: L 0,314, R 0,309, Estéreo 0,316 px.

| Parâmetro | Fábrica | Xadrez | AprilGrid | vs xadrez | vs fábrica |
|---|---:|---:|---:|---:|---:|
| fx L | 533,820 | 533,039 | 534,478 | +0,270 % | +0,123 % |
| fy L | 533,845 | 533,388 | 535,576 | +0,410 % | +0,324 % |
| fx R | 533,320 | 532,800 | 534,015 | +0,228 % | +0,130 % |
| fy R | 533,435 | 533,021 | 535,205 | +0,410 % | +0,332 % |
| Baseline | 119,982 | 120,040 | 120,124 mm | +0,070 % | +0,118 % |
| cx L / cy L | 632,1 / 369,1 | 632,1 / 369,0 | 631,1 / 371,2 | reportado | |
| cx R / cy R | 641,6 / 370,3 | 642,9 / 370,0 | 641,1 / 372,3 | reportado | |

- Validação cruzada (RMS mediano Left / Right / Stereo):

  | Calibração | Xadrez de 16/10 | AprilGrid do diagnóstico |
  |---|---|---|
  | Fábrica | 0,240 / 0,237 / 0,342 | 0,430 / 0,441 / 0,495 |
  | Xadrez | 0,232 / 0,226 / 0,276 | 0,413 / 0,457 / 0,459 |
  | AprilGrid v2 | 0,242 / 0,241 / 0,327 | 0,372 / 0,365 / 0,366 |

- `--use_stored_values` reproduz o resultado ao vivo (fx L 534,47845676 nas duas execuções).
- 19 das 35 amostras da primeira captura tinham o alvo cortado pela borda da imagem, o que
  comprova a vista parcial.

### Etapa 14
- `--virtual --target_yaml`: as duas ferramentas aceitam a combinação e param no erro esperado,
  sem ZED X One.
- `--fisheye --target_yaml`: o programa aborta com `cv::Exception: CALIB_CHECK_COND -
  Ill-conditioned matrix` em `fisheye::stereoCalibrate`. **Isso já acontecia:** o xadrez aborta
  igual com as imagens de 16/10, porque é o modelo fisheye numa lente que não é fisheye e o código
  original não captura a exceção. Não é regressão (R15). Fica como observação para o upstream.
