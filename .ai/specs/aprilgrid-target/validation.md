# Validação — calibração estéreo da ZED 2i: xadrez, AprilGrid e fábrica

> Evidência dos critérios de aceitação de [`spec.md`](spec.md). A Parte 1 é a linha de base com
> xadrez (anterior à feature); a Parte 2 valida o alvo AprilGrid.
>
> **Dados:** as imagens, logs e calibrações citadas ficam no workspace da bancada, em
> `ros2_ws/dev/calib_runs/` (fora deste repositório, ao lado dele) — não vêm com o clone.

# Parte 1 — Xadrez × fábrica (linha de base)

| | |
|---|---|
| **Data** | 2026-10-16 |
| **Câmera** | ZED 2i, S/N 35180697, USB 3 |
| **Resolução** | HD720 (1280x720) @ 15 fps (`RESOLUTION::AUTO` na ZED 2i) |
| **Meio** | ar, sem caixa estanque |
| **Tabuleiro** | 9x6 cantos internos, quadrado de **23,8 mm** (medido; ver [Escala do tabuleiro](#3-escala-do-tabuleiro)) |
| **Ferramenta** | `zed_stereo_calibration`, commit `fcfaeae`, modelo radial-tangencial |
| **Amostras** | 35/35 válidas |
| **Referência** | calibração de fábrica, `SN35180697.conf`, seções `*_CAM_HD` e `[STEREO]` |

Arquivos da execução em `calib_runs/2026-10-16_ar_9x6/`:

| Arquivo | Conteúdo |
|---|---|
| `recalib/zed_calibration_0.yml` | **a calibração válida** |
| `recalib/calib.log` | relatório da recalibração |
| `factory/SN35180697.conf` | backup da calibração de fábrica |
| `factory238_P*.png` | prints do checker com a fábrica, escala correta |
| `factory_P*.png` | prints do checker com a fábrica, escala errada (25,4 mm) |
| `zed_calibration_35180697.yml` | **escala errada (25,4 mm), não usar** |

## 1. Comparação de parâmetros: fábrica × ferramenta

### Intrínsecos e baseline

| Parâmetro | Unid. | Fábrica | Ferramenta | Δ | Variação |
|---|---|---:|---:|---:|---:|
| fx (esq.) | px | 533,820 | 533,039 | −0,781 | **−0,146 %** |
| fy (esq.) | px | 533,845 | 533,388 | −0,457 | **−0,086 %** |
| cx (esq.) | px | 632,115 | 632,103 | −0,012 | **−0,002 %** |
| cy (esq.) | px | 369,065 | 368,959 | −0,106 | **−0,029 %** |
| fx (dir.) | px | 533,320 | 532,800 | −0,520 | **−0,097 %** |
| fy (dir.) | px | 533,435 | 533,021 | −0,414 | **−0,078 %** |
| cx (dir.) | px | 641,645 | 642,923 | +1,278 | **+0,199 %** |
| cy (dir.) | px | 370,337 | 369,986 | −0,350 | **−0,095 %** |
| Baseline | mm | 119,982 | 120,040 | +0,058 | **+0,048 %** |

Todos os parâmetros desta tabela ficam a **menos de 0,2 %** da fábrica. O baseline difere 0,06 mm.

### Extrínsecos próximos de zero

Estes valores são quase nulos. Uma diferença ínfima vira centenas de por cento: o CV, por exemplo,
daria −712 %. Por isso a variação em % não diz nada aqui. A coluna final traz o efeito equivalente
na imagem ou o deslocamento comparado ao baseline.

| Parâmetro | Unid. | Fábrica | Ferramenta | Δ | Efeito equivalente |
|---|---|---:|---:|---:|---|
| TY | mm | −0,234 | −0,187 | +0,048 | 0,04 % do baseline |
| TZ | mm | +0,078 | −0,207 | −0,286 | 0,24 % do baseline |
| RX (inclinação) | rad | 0,00134 | 0,00089 | −0,00046 (0,026°) | ≈ 0,24 px vertical |
| CV (convergência) | rad | −0,00025 | −0,00206 | −0,00181 (0,104°) | ≈ 0,96 px horizontal |
| RZ (rolagem) | rad | 0,00210 | 0,00256 | +0,00046 (0,026°) | ≈ 0,29 px na borda da imagem |

A maior diferença entre as duas calibrações é horizontal: a convergência (~1 px) somada ao cx da
câmera direita (+1,3 px). É um viés de disparidade abaixo de ~2 px nos dois sentidos. Para a
profundidade, isso é desprezível frente ao ruído de correspondência estéreo.

### Distorção

Fábrica e ferramenta usam o modelo racional de 8 coeficientes (k1–k6, p1, p2). Os coeficientes
compensam uns aos outros, então dois conjuntos bem diferentes podem descrever a mesma lente. Não faz
sentido compará-los um a um. A distorção é validada pelo erro de reprojeção (seção 2).

## 2. Métricas que evidenciam o resultado

### 2.1 Erro de reprojeção da calibração (35 amostras)

| Etapa | RMS | Limite da ferramenta | Situação |
|---|---:|---:|---|
| Câmera esquerda | **0,237 px** | 0,5 px | aprovado |
| Câmera direita | **0,238 px** | 0,5 px | aprovado |
| Estéreo | **0,254 px** | 0,5 px | aprovado |

Erro abaixo de 0,5 px é o critério de boa calibração da própria ferramenta. Os três ficam com
**metade do limite**.

### 2.2 Concordância com uma referência independente

A calibração de fábrica foi feita pela Stereolabs, num processo independente deste. A da
ferramenta usou um tabuleiro impresso e uma bancada comum. Ainda assim, os dois processos chegaram a:

| Métrica | Resultado |
|---|---|
| Maior variação de distância focal (fx, fy) | **0,146 %** |
| Maior desvio de centro óptico (cx, cy) | **1,28 px** (0,2 % da largura) |
| Variação do baseline | **0,048 %** (0,06 mm) |
| Maior diferença de rotação entre as câmeras | **0,104°** |

### 2.3 O resultado não foi puxado pela calibração de fábrica

Na primeira execução, a ferramenta usou a fábrica como ponto de partida (`CALIB_USE_INTRINSIC_GUESS`).
A recalibração (`--use_stored_values`) rodou **do zero**, sem esse ponto de partida, e reproduziu os
mesmos intrínsecos até a 5ª casa decimal: fx esq. 533,03904 contra 533,03905. A concordância da
seção 2.2 vem dos dados, não da inicialização.

### 2.4 A calibração de fábrica também foi validada

`zed_stereo_checker --square_size 23.8`, com a calibração interna e 7 poses do tabuleiro:

| Pose | Esq. | Dir. | Estéreo |
|---|---:|---:|---:|
| P1 | 0,201 | 0,193 | 0,249 |
| P2 | 0,242 | 0,245 | 0,265 |
| P3 | 0,377 | 0,324 | 0,404 |
| P4 | 0,309 | 0,261 | 0,505 |
| P5 | 0,215 | 0,274 | 0,299 |
| P6 | 0,280 | 0,239 | 0,344 |
| P7 | 0,229 | 0,260 | 0,390 |

O erro estéreo médio ficou em ~0,35 px. As duas calibrações reprojetam bem: a nova com 0,25 px nas
35 amostras e a de fábrica com 0,25 a 0,5 px nas poses do checker.

Esta comparação não decide qual das duas é *melhor*. A nova não foi passada pelo checker nas mesmas
poses. O que os números mostram é que as duas são boas e que concordam entre si.

## 3. Escala do tabuleiro

A primeira execução usou o tamanho de quadrado padrão da ferramenta, 25,4 mm. O resultado parecia
bom (RMS de 0,25 px), mas o **baseline saiu 128,11 mm**, 6,8 % acima da fábrica. O tamanho do quadrado
escala só a translação, não fx, fy, cx e cy. Por isso intrínsecos certos com baseline errado
apontavam para um erro de escala do alvo.

Medida a folha (lado maior 23,75 mm, menor 23,85 mm, média **23,8 mm**), a recalibração com as mesmas
35 imagens deu baseline de **120,04 mm**. É o valor previsto por 128,11 × 23,8 / 25,4. Pelo mesmo
motivo, o checker de fábrica caiu de **6–11 px para 0,25–0,5 px** de erro estéreo.

| Quadrado declarado | Baseline obtido | Erro estéreo do checker (fábrica) |
|---|---:|---:|
| 25,4 mm (padrão, errado) | 128,11 mm | 6,3 – 10,9 px |
| 23,8 mm (medido) | 120,04 mm | 0,25 – 0,51 px |

Duas observações:
- O RMS de reprojeção **não denuncia** erro de escala: foi idêntico nas duas execuções. Só o baseline
  e o checker estéreo denunciam.
- A folha tem 0,4 % de diferença de escala entre os eixos. Isso limita a precisão do baseline com
  este tabuleiro a ~±0,2 %.

## 4. Conclusão

A ferramenta produziu uma calibração com erro de reprojeção de 0,24–0,25 px. Os intrínsecos ficaram a
menos de 0,2 % da fábrica e o baseline a 0,05 %, com reprodutibilidade confirmada sem inicialização
pela fábrica. A calibração de fábrica desta unidade também está boa. **Para uso no ar não há motivo
para substituí-la.**

O método fica validado para a próxima etapa, a calibração **submersa com a caixa estanque**. Nela a
refração altera os intrínsecos e a calibração de fábrica deixa de valer.

## 5. Como reproduzir

Dentro do container, como `admin`:

```bash
RUN=/workspaces/isaac_ros-dev/dev/calib_runs/<data>_<meio>
BIN=/workspaces/isaac_ros-dev/dev/zed-opencv-calibration/build
mkdir -p $RUN/factory && cd $RUN
cp /usr/local/zed/settings/SN<serial>.conf factory/       # a cópia do SDK some se o container for recriado

# Calibração (captura interativa; saídas no diretório corrente)
$BIN/stereo_calibration/zed_stereo_calibration --square_size 23.8 2>&1 | tee calib.log

# Recalcular a partir das imagens salvas, sem a câmera
$BIN/stereo_calibration/zed_stereo_calibration --use_stored_values --square_size 23.8

# Validação
$BIN/stereo_checker/zed_stereo_checker --square_size 23.8                                   # fábrica
$BIN/stereo_checker/zed_stereo_checker --square_size 23.8 --calib_opencv zed_calibration_<sn>.yml
```

Antes de calibrar, meça o tabuleiro **ao longo de vários quadrados** (do primeiro ao último canto
interno, dividido pelo número de quadrados). Medir um quadrado só tem ±0,5 mm de erro, ou seja, 2 %.

---

# Parte 2 — AprilGrid (Kalibr) × xadrez × fábrica

| | |
|---|---|
| **Data** | 2026-10-20 |
| **Câmera** | ZED 2i, S/N 35180697, HD720, no ar |
| **Alvo** | Kalibr AprilGrid 6x6, tag36h11, tag de 88 mm, espaço de 26,4 mm (medido), placa rígida |
| **Ferramenta** | `zed_stereo_calibration --target_yaml target.yaml` (branch `aprilgrid-target`) |
| **Execução** | `calib_runs/2026-10-20_ar_aprilgrid_v2/` |

29 amostras. A coleta terminou pelas métricas: X, Y, Size e Skew chegaram a 100 % antes do máximo de
amostras. A inclinação mediana foi de 35°, as distâncias ficaram entre 0,49 e 1,29 m, e 15 das 29
amostras têm o alvo cortado pela borda da imagem (vista parcial).

## Parâmetros

| Parâmetro | Unid. | Fábrica | Xadrez | AprilGrid | vs xadrez | vs fábrica |
|---|---|---:|---:|---:|---:|---:|
| fx (esq.) | px | 533,820 | 533,039 | 534,478 | +0,270 % | **+0,123 %** |
| fy (esq.) | px | 533,845 | 533,388 | 535,576 | +0,410 % | **+0,324 %** |
| fx (dir.) | px | 533,320 | 532,800 | 534,015 | +0,228 % | **+0,130 %** |
| fy (dir.) | px | 533,435 | 533,021 | 535,205 | +0,410 % | **+0,332 %** |
| cx / cy (esq.) | px | 632,1 / 369,1 | 632,1 / 369,0 | 631,1 / 371,2 | | |
| cx / cy (dir.) | px | 641,6 / 370,3 | 642,9 / 370,0 | 641,1 / 372,3 | | |
| Baseline | mm | 119,982 | 120,040 | 120,124 | +0,070 % | **+0,118 %** |

RMS de reprojeção: esquerda **0,314 px**, direita **0,309 px**, estéreo **0,316 px**.

Critério da spec: fx, fy e baseline a ≤ 0,5 % do xadrez e da fábrica, e RMS < 0,5 px. **Atendido.**

## Validação cruzada

As três calibrações, com intrínsecos fixos, foram avaliadas em imagens que nenhuma delas usou. A tabela
mostra o RMS mediano por vista (Left / Right / Stereo, em px):

| Calibração | Imagens de xadrez de 16/10 | Imagens AprilGrid do diagnóstico |
|---|---|---|
| Fábrica | 0,240 / 0,237 / 0,342 | 0,430 / 0,441 / 0,495 |
| Xadrez | 0,232 / 0,226 / 0,276 | 0,413 / 0,457 / 0,459 |
| AprilGrid | 0,242 / 0,241 / 0,327 | 0,372 / 0,365 / 0,366 |

A calibração AprilGrid reprojeta o alvo de xadrez tão bem quanto a fábrica, e o alvo AprilGrid melhor
que as outras duas.

## O que foi aprendido no caminho

- **Os quadradinhos do Kalibr enganam o detector do OpenCV.** Eles tocam as quinas das tags e se
  fundem com elas na detecção por contorno. De longe as tags somem; de perto os cantos saem ~5 px
  deslocados. A correção foi refinar os cantos no cruzamento em X e subir
  `adaptiveThreshConstant` para 25 (D11 em `.ai/specs/aprilgrid-target/decisions.md`). O erro dos
  cantos com a calibração de fábrica caiu de 0,4–1,7 px para 0,27–0,47 px.
- **Inclinação determina a focal.** Uma primeira captura, com amostras quase todas de frente
  (inclinação mediana de 7°), deu RMS de 0,32 px, mas fx 0,7 % alto. Um RMS baixo não garante uma
  focal certa.
- `petro_data_replayer/calibrate_intrinsics.py` usa o detector sem essa correção. As calibrações
  `zed_opencv_calibration_cesar_00{1,2}.yaml` (RMS de 0,94 px) provavelmente carregam esse viés.
