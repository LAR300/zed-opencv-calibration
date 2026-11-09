# Fluxo de análise offline — avaliar o que foi coletado

Depois da coleta ([`fluxo-coleta.md`](fluxo-coleta.md)) a ferramenta já entregou uma calibração e um
RMS. Este fluxo responde ao que o RMS não responde: o erro é ruído ou viés, a captura cobre a
imagem, a calibração generaliza, quanto ela erra em milímetros e se é melhor que a de fábrica. O
que cada métrica é, e de onde vem, está em [`funcionalidades.md`](funcionalidades.md).

Tudo roda **sem a câmera**, a partir dos arquivos gravados, no pacote Python `analysis/zedcal`.

## Como os comandos funcionam

```bash
cd /workspaces/isaac_ros-dev/dev/zed-opencv-calibration/analysis     # no container, como admin
python3 -m zedcal.<comando> --config <arquivo.yaml>
```

- Cada comando lê **uma configuração YAML** com os caminhos de entrada e a pasta de saída. O
  formato de cada uma está no início do arquivo do comando (`zedcal/<comando>.py`).
- A configuração é copiada para a pasta de saída, e **uma pasta existente nunca é sobrescrita**:
  para refazer, apague a saída.
- Entrada principal: o `detections.json` de cada captura (cantos detectados pelo C++) e os
  arquivos de calibração (`.yml` no formato do ZED SDK). O Python **não detecta cantos**: usa sempre
  o detector da ferramenta, para medir o mesmo que ela.
- Convenção do RMS: por canto (norma 2D), como o OpenCV. O erro por coordenada é RMS² / 2.

| Comando | Responde | Quantas capturas |
|---|---|---|
| `calibrate` | A calibração em Python, com variantes (confere a paridade com o C++) | 1 |
| `coverage_command` | A captura cobre a imagem e as inclinações? | 1 |
| `m1_command` | M1: o erro é ruído ou viés? Onde na imagem? | 1 |
| `m4_command` | M4: erro epipolar, erro métrico, ruído de profundidade, viés de disparidade | 1 ou mais |
| `m2_command` | M2: as calibrações concordam entre si e com F? | 2 ou mais (10 no protocolo) |
| `m3_command` | M3: cada calibração funciona nas capturas que não usou? | 2 ou mais |
| `report_command` | Estatística de uma condição, ou comparação A × B (portões) | 3 ou mais por condição |
| `metric_test_command` | Teste métrico em distâncias medidas | sessão métrica |
| `thermal_command` | Tempo até o regime térmico | registros `--timelapse` |

## O exemplo: capturas já gravadas

O exemplo usa as capturas AprilGrid de `calib_runs/` (ver `calib_runs/README.md`) e a calibração de
fábrica F exportada. As configurações estão versionadas em `analysis/exemplos/`, e as saídas vão
para `calib_runs/analises/exemplo/` (apague essa pasta para refazer).

| Nome | Captura | Situação |
|---|---|---|
| `v1` | `2026-10-20_ar_aprilgrid` (35 pares) | Ar; poses pouco inclinadas |
| `v2` | `2026-10-20_ar_aprilgrid_v2` (29 pares) | Ar; a captura analisada em detalhe |
| `controles` | `2026-11-03_camera_settings` (25 pares) | Ar; controles de câmera fixos |
| `acrilico` | `2026-10-23_acrilico_ar_aprilgrid` (35 pares) | Através da porta de acrílico |

Tudo de uma vez: `bash analysis/exemplos/rodar.sh` (1–2 min). Passo a passo:

```bash
cd /workspaces/isaac_ros-dev/dev/zed-opencv-calibration/analysis
E=exemplos
A=/workspaces/isaac_ros-dev/dev/calib_runs/analises/exemplo
```

As capturas antigas não seguiram o roteiro nem o modo sessão. Os números abaixo ilustram como ler
cada saída; as conclusões da metodologia virão das 10 capturas por condição.

---

## Parte A — uma captura (v2)

### Passo 1 — Detectar e calibrar a partir das imagens (C++)

```bash
bash $E/preparar.sh
```

Para cada captura, cria `$A/<nome>/` com um link para o `zed-images/` original e roda, sem câmera,
`zed_stereo_calibration --use_stored_values --target_yaml target.yaml --detections_out detections.json`.
Saem o `detections.json` (todos os cantos) e o `zed_calibration_0.yml` (a calibração, igual à
que a ferramenta faria ao vivo). Numa captura de sessão esses arquivos já existem na pasta da
captura e este passo não é necessário.

Saída da v2 (`$A/v2/calib.log`):

```
 * Valid samples: 29/29
   * Left:   0.314231 px   * Right:  0.308765 px   * Stereo: 0.315665 px
* Intrinsic mat left:  [534.478, 0, 631.119; 0, 535.576, 371.162; 0, 0, 1]
* Translation: [-120.124; -0.112; 0.405]      * Rotation: [0.00123; -0.00054; 0.00251]
```

**Como ler:**
- **RMS de reprojeção** (0,31 px): distância média entre o canto detectado e o previsto. Abaixo de
  0,5 px a ferramenta marca GOOD. Ele **não** diz se o erro é ruído ou viés (passo 4). Sem
  `--session`, "Left/Right" é o RMS da etapa mono, antes do ajuste estéreo; o RMS com os
  parâmetros salvos aparece no passo 2 e no `report.json` das capturas de sessão.
- **Intrínsecos** `[fx 0 cx; 0 fy cy]`: focal em pixels e centro óptico. fy/fx = 1,0021, 0,2 %
  acima de 1; a fábrica dá 1,0000. Num sensor de pixels quadrados isso indica erro nos dados: é a
  assinatura do alvo esticado em Y.
- **Distorção** (8 coeficientes do modelo racional): não se interpreta coeficiente a coeficiente,
  porque eles se compensam. Compare distorções pelo M2.
- **Extrínsecos:** −120,12 mm em x é o baseline (fábrica: 119,98 mm); a rotação, em radianos
  (Rodrigues), dá 0,07°, −0,03° e 0,14°: câmeras quase paralelas.

### Passo 2 — Paridade Python × C++

```bash
python3 -m zedcal.calibrate --config $E/01_calibrate_v2.yaml
```
```
common_joint_cpp   stereo 0.3157 px, fit L/R 0.3184 / 0.3129 px, 29 of 29 views
```

**Para que serve:** as métricas seguintes precisam do que a ferramenta não guarda (pose de cada
vista, resíduo de cada canto) e recalculam isso a partir do `detections.json`. O RMS estéreo igual
ao do C++ (0,3157 px) garante que avaliam a mesma calibração. `fit L/R` é o RMS de cada câmera com
os parâmetros salvos, o número certo. O mesmo comando calibra com variantes (`mono_points:
all|common`, `stereo: joint|fixed`, `rejection: cpp|none`, modelo `rational8|radtan5`, `prior`
com uma calibração inicial, `bootstrap: {n: 200}`) sem mexer no C++.

### Passo 3 — Cobertura

```bash
python3 -m zedcal.coverage_command --config $E/02_cobertura_v2.yaml
```
```
coverage PASSED
```

`coverage.png` tem quatro gráficos:
- **`corners per cell, left/right camera`:** a imagem dividida em 8 × 8 células de 160 × 90 px
  (linha 0 no alto, coluna 0 à esquerda), com o número de cantos detectados em cada uma somando
  todas as vistas. Onde não há cantos a distorção é extrapolada. Na v2: centro com 90–150 cantos
  por célula, cantos da imagem com 6–14 (esquerda) e 3–12 (direita).
- **`tilt about the horizontal/vertical axis`:** histograma das vistas pela inclinação do alvo
  (horizontal: borda de cima mais longe ou mais perto; vertical: lado direito mais longe ou mais
  perto), com as linhas vermelhas em ±30°. Vistas inclinadas separam a focal da distância. Na v2:
  vertical equilibrado (6 vistas além de 30° de cada lado); horizontal com 4 de um lado e só 1 do
  outro.

**Critério** (`coverage.json`): toda célula com ≥ N cantos (N provisório = 1); ≥ 2 vistas além de
30° em cada eixo, com os dois sinais. A v2 passa no limite; a `controles` reprova ("horizontal tilt
insufficient: +0 / −2"). Com um N maior, a v2 reprovaria nos cantos da imagem. Sai com 0
(aprovada) ou 1 (reprovada).

### Passo 4 — M1 da calibração própria

```bash
python3 -m zedcal.m1_command --config $E/03_m1_v2_propria.yaml
```
```
left  BR 0.701  eps_bias 0.190 px  sigma_d 0.124 px  RMS free pose 0.315 px
right BR 0.694  eps_bias 0.186 px  sigma_d 0.123 px  RMS free pose 0.309 px
stereo RMS 0.3157 px, filter passed
```

**Como ler:**
- **σ_d** (0,124 px): ruído do detector, aleatório. Medido recalculando a pose de cada tag
  isolado (4 cantos) com os intrínsecos fixos. Parecido com o do xadrez (0,14 px): o detector
  está bom.
- **ε_bias** (0,190 px): a parte sistemática do erro, que o modelo não explica.
- **BR, Bias Ratio** (0,70): a fração do erro que é viés. Conta aproximada, por coordenada:
  0,315² / 2 = 0,050 = 0,124² (0,015) + 0,190² (0,036), e 0,036 / 0,050 ≈ 0,70. Perto de 0 é bom;
  o xadrez do laboratório dá 0,24–0,31.
- **Por que importa:** mais imagens reduzem o ruído, não o viés. Com BR 0,70, capturar mais com o
  mesmo alvo converge para uma calibração errada; é preciso tirar a causa.
- **`filter passed`:** RMS < `max_rms_px` (0,5).

`m1_<calib>_left.png`:
- **`RMS per corner`:** o RMS em cada célula 8 × 8. Na v2: 0,25–0,33 px no centro, até 0,57 px na
  borda de cima e ~0,45 px na de baixo.
- **`mean signed residual`:** uma seta por célula, a média de (previsto − detectado). Ruído puro dá
  setas pequenas e em direções aleatórias; setas grandes e alinhadas entre vizinhas são viés. Na
  v2: pequenas no centro, grandes e horizontais nas linhas 0 e 7.

### Passo 5 — M1 da calibração de fábrica nas mesmas imagens

```bash
python3 -m zedcal.m1_command --config $E/04_m1_v2_fabrica.yaml
```
```
left  BR 0.749  eps_bias 0.217 px  sigma_d 0.125 px  RMS free pose 0.350 px
right BR 0.744  eps_bias 0.211 px  sigma_d 0.124 px  RMS free pose 0.341 px
stereo RMS 0.3725 px, filter passed
```

**Como ler:**
- **σ_d igual** ao do passo 4: o ruído é das imagens e do detector, não da calibração.
- **F tem ~14 % mais viés e mais RMS**, mas F não foi ajustada nestas imagens, e a calibração
  própria "absorveu" parte do defeito do alvo (o fy/fx 1,002 compensa o esticamento em Y). Aqui
  "F pior" **não** quer dizer que F seja pior para a câmera.
- **As setas têm o mesmo desenho** do passo 4 (cuidado: a escala de cor das figuras é diferente).
  Duas calibrações com o mesmo padrão de erro mostram que a causa está nos dados: o alvo impresso.

### Passo 6 — M4, o que o SLAM sente

```bash
python3 -m zedcal.m4_command --config $E/05_m4_v2.yaml
```
```
calibration   epi_rms    <0.25     <0.1    sides%    pitch%     span%  plane_mm
F               0.198     0.80     0.38    +0.429    +0.356    +0.353     2.446
propria         0.161     0.87     0.50    +0.177    +0.109    +0.141     2.496
disparity bias of propria vs F: v2 -0.046 px (dZ/Z at 3 m +0.22 %)
```

Cada par é retificado com cada calibração, como o SLAM faz. **Colunas:**
- **`epi_rms`, `<0.25`, `<0.1`:** depois da retificação um canto deve estar na mesma linha nas
  duas imagens; é o desalinhamento vertical médio e a fração dos cantos abaixo de 0,25 e 0,1 px.
  O matching estéreo depende dele. As duas ficam abaixo de 0,2 px.
- **`sides%`, `pitch%`, `span%`:** os cantos triangulados em 3D; o lado dos tags, o passo entre
  tags vizinhos (88 + 26,4 mm) e o vão de ponta a ponta, contra o **tamanho do alvo informado**.
  F mede +0,35 % no vão (~2,3 mm em 660 mm); a própria, +0,14 %. Ressalva: a referência é o alvo
  nominal, que tem defeito, e a própria foi ajustada a ele. Quem decide a escala real é o teste
  métrico, com distâncias medidas.
- **`plane_mm`:** dispersão dos pontos 3D em torno do plano do alvo. É ruído de triangulação
  (cresce com a distância ao quadrado) e sai igual para as duas.
- **`disparity bias`:** a profundidade vem de Z = f·B/d. A 3 m, d ≈ 534 × 120 / 3000 ≈ 21 px; uma
  diferença de 0,046 px entre as calibrações dá 0,046 / 21 ≈ 0,22 % (~7 mm a 3 m). Cresce com a
  distância.

`m4_<calib>.png`: o erro epipolar e o erro do passo por região da imagem. Na v2 o epipolar fica em
0,1–0,2 px, pior nos cantos (até 0,31 px); o passo por par chega a 3–4,5 % na coluna da esquerda,
por isso valem as médias e o vão.

**Conclusão da parte A:** a calibração da v2 passa nos critérios clássicos (RMS 0,31 px), mas 70 %
do erro é viés, e o mesmo viés aparece com F: vem do alvo impresso e das bordas pouco visitadas.
Com uma captura só não dá para dizer se ela é melhor que a de fábrica.

---

## Parte B — várias capturas

Uma condição do protocolo tem 10 capturas. O exemplo usa as três capturas no ar (v1, v2,
controles) e a do acrílico como contraste.

### Passo 7 — M2, as calibrações concordam?

```bash
python3 -m zedcal.m2_command --config $E/06_m2.yaml
```
```
left  full_image          EME 1.479 px  (each: 1.310, 1.483, 1.626)  F 3.363 px (p100)
left  full_image_rotation EME 0.930 px  (each: 1.188, 1.016, 0.387)  F 3.110 px (p100)
left  covered             EME 1.475 px  ...
left  covered_rotation    EME 0.927 px  ...
```

O **erro de mapeamento** entre duas calibrações: uma grade de pixels é desprojetada em raios com
uma e projetada com a outra; é a distância média, em px, entre o pixel de saída e o de chegada. Não
depende de imagens. A **referência** é a calibração "média" da condição, calculada no espaço da
imagem (`calib_reference.yml`).

- **`each`:** o erro de cada calibração (v1, v2, controles) até a referência; **EME** é a média
  quadrática deles, a dispersão da condição.
- **`_rotation`:** permite uma rotação comum dos raios antes de comparar, que é o que os
  extrínsecos compensam no estéreo. É a forma do artigo de Hagemann, e a mais justa.
- **`covered`:** só a região da imagem que tem cantos; `full_image`, a imagem inteira.
- **`F … (p100)`:** a distância de F até a referência e o seu percentil entre as calibrações
  próprias (100 = mais longe que todas).

**Na leitura:** com rotação, controles fica a 0,39 px da referência e v1 a 1,19 px. A v1 é a captura
com poses pouco inclinadas, e o M2 a aponta como a mais diferente. F fica a 3,1 px, mais longe que
qualquer calibração própria: F é sistematicamente diferente delas. O M2 não diz quem está certo;
isso fica para o M3, o M4 e o teste métrico. `m2_factory_*.png` mostra onde na imagem F difere da
referência.

### Passo 8 — M3, cada calibração nas capturas que não usou

```bash
python3 -m zedcal.m3_command --config $E/07_m3.yaml
```
```
== left (rows: calibrations, columns: captures)
                v1      v2   controles  acrilico
v1           0.387   0.329     0.331     0.384
v2           0.391   0.320     0.318     0.382
controles    0.402   0.330     0.306     0.388
F            0.418   0.353     0.363     0.414
acrilico     0.403   0.339     0.327     0.358
   train 0.343  test 0.360  ratio 1.05
== right_transfer
                v1      v2   controles  acrilico
v1           0.442   0.355     0.355     0.674
v2           0.448   0.341     0.345     0.667
controles    0.460   0.366     0.324     0.660
F            0.543   0.454     0.484     0.826
acrilico     0.670   0.608     0.602     0.391
   train 0.374  test 0.518  ratio 1.38
```

Linhas são calibrações; colunas, capturas. Cada célula é o RMS da calibração nas imagens daquela
captura (na diagonal, a própria captura, usada no ajuste).
- **`left`:** a pose do alvo é estimada na imagem esquerda com os intrínsecos fixos; mede os
  intrínsecos da esquerda.
- **`right_transfer`:** a pose da esquerda é levada à direita pelos extrínsecos R, T e projetada;
  mede os extrínsecos e a direita.
- **`stereo`:** o mesmo valor do `zed_stereo_checker`.
- **`ratio`:** teste / treino. Perto de 1, a calibração generaliza.

**Na leitura:** na esquerda, as calibrações no ar generalizam bem (razão 1,05). Na transferência
para a direita, as calibrações sem acrílico vão a 0,66–0,67 px na captura com acrílico, contra
0,39 px da própria, e a calibração do acrílico vai a 0,60–0,67 px nas capturas no ar: **a porta de
acrílico muda a calibração**, sobretudo os extrínsecos. F fica atrás em todas as colunas, mas são
imagens do alvo com defeito; em imagens de xadrez as calibrações AprilGrid ficaram 5–11 % acima de F
(`.ai/specs/validacao-cruzada/plan.md`).

### Passo 9 — Estatística da condição

```bash
python3 -m zedcal.report_command --config $E/08_relatorio.yaml
```

`report.md` tem uma linha por parâmetro (n = 3):

| Grandeza | Média | IC95 % | DP | CV | F | F no IC95 % | Percentil de F |
|---|---:|---|---:|---:|---:|---|---:|
| fx_left | 535,61 | [532,64; 538,58] | 1,2 | 0,00223 | 533,82 | sim | 0 |
| fy_left | 536,68 | [534,23; 539,14] | 0,99 | 0,00184 | 533,85 | não | 0 |
| fy_over_fx_left | 1,0020 | [0,99994; 1,0041] | 0,00083 | 0,00083 | 1,00005 | sim | 0 |
| baseline_mm | 120,09 | [119,69; 120,48] | 0,159 | 0,00132 | 119,98 | sim | 33,3 |

- **Média, IC95 % (t de Student), DP:** onde estão os parâmetros da condição e quanto variam entre
  capturas.
- **CV** (DP / média): variação relativa; "não informativo" para grandezas perto de zero
  (ty, tz, rotações), em que se olha o DP.
- **F no IC95 % e percentil de F:** onde a fábrica cai entre as calibrações próprias. É informativo,
  não critério de aceite.

**Na leitura:** fx e fy de F estão abaixo das três calibrações próprias (percentil 0), e fy de F
fica fora do intervalo. Com n = 3 os intervalos são largos (fx ± 3 px); o protocolo pede 10
capturas. `parameters.png` mostra cada parâmetro por captura, com F.

**Comparação A × B** (os portões do protocolo): o mesmo comando com duas condições na
configuração; acrescenta a permutação sobre a matriz de mapeamento, a aplicação cruzada, o TOST
com margens e a comparação das dispersões (Brown–Forsythe e razão F):

```yaml
conditions:
  A: {calibrations: {c01: .../A_ar_sem_acrilico/captura_01/calib_cpp_rational8.yml, ...}, detections: [...]}
  B: {calibrations: {c01: .../B_ar_com_acrilico/captura_01/calib_cpp_rational8.yml, ...}, detections: [...]}
factory: .../dados/fabrica/SN35180697/calib_factory_rational8.yml
margins: {fx_left: 2.7, baseline_mm: 0.6, metric_span_pct: 0.5}   # provisórias
margins_approved: false
alpha: 0.05
output_dir: .../analises/A_x_B
```

---

## Parte C — análises especiais

| Análise | Dados | Comando | Saída |
|---|---|---|---|
| Teste métrico | Sessão com `roteiro: config/roteiro_metrico_v1.yaml` (36 poses a 0,5, 1, 2 e 3 m) e `target_measured` completo | `metric_test_command` | Erro métrico por distância e posição; distância triangulada do plano contra a medida, com um deslocamento d0 comum (o ponto de medição fica à frente do centro óptico, em posição desconhecida) |
| Aquecimento | 3 registros `--timelapse 30 60` a frio | `thermal_command` | Curvas da pose e da transferência estéreo no tempo; instante em que cada série estabiliza; X proposto = o maior, arredondado para cima |
| Incerteza dos parâmetros | Uma captura | `calibrate` com `bootstrap: {n: 200}` | Desvio-padrão de cada parâmetro reamostrando as vistas |

## Para uma condição nova

Copie as configurações de `analysis/exemplos/` e troque os caminhos:
- `detections` → `.../dados/<condição>/captura_XX/detections.json`;
- `calibration` própria → `.../captura_XX/calib_cpp_rational8.yml`;
- F → `.../dados/fabrica/SN<serial>/calib_factory_rational8.yml`;
- `target_measured` do M4 pode sair da configuração: o `meta.json` da sessão já tem as medidas;
- `output_dir` → `.../analises/<condição>/<análise>`.

Ordem sugerida: passo 3 (cobertura) logo após cada captura; no fim da condição, os passos 4–6 por
captura e 7–9 para o conjunto.
