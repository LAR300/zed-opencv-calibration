# Plano — Núcleo offline em Python

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir. Todos os comandos rodam no
> container, como `admin`: `cd $REPO && python3 -m pytest analysis/tests`.

## Pré-requisitos

- Formatos de [`persistencia-captura`](../persistencia-captura/spec.md) congelados (versão 1).
- Para o critério de compatibilidade (etapa 7): o `--detections_out` de `persistencia-captura`
  e o relatório de `saida-calibracao`.

## Etapas

- [x] 1. **Esqueleto** (condições de implementação). `analysis/zedcal/`, `analysis/tests/`,
  `analysis/configs/`, `README.md` e `pytest.ini`. O `pytest.ini` desliga o plugin `anyio` do
  container, que não carrega com o pytest 6.2.5. `tests/test_no_detection.py` falha se o pacote
  usar `aruco`, `findChessboardCorners`, `detectMarkers` ou `findCirclesGrid`. Rodar com
  `cd analysis && python3 -m pytest`.

  Especificação original da etapa: Criar `analysis/zedcal/`, `analysis/tests/` e
  `analysis/configs/`, com `README.md` curto. Um teste verifica que nenhum módulo importa
  `cv2.aruco` nem usa `findChessboardCorners` (inspeção do código-fonte).
  - **Pronto quando:** `pytest` roda e esse teste passa.
- [x] 2. **Alvo e pontos 3D** (R3). `zedcal/target.py`: `Target.tag_corners` (BR, BL, TL, TR, como o
  C++), `checkerboard_points` (a ordem do `calibrate()`) e `with_measured` (dimensões medidas).
  Testes em `tests/test_target.py`, com os valores de `tests/test_geometry.cpp`.

  Especificação original da etapa: `target.object_points(target, tag_id)` e a versão para o
  xadrez.
  - Testes primeiro: os valores esperados de `tests/test_geometry.cpp` para as tags 0, 5, 30 e 35; o xadrez 9×6 de
    23,8 mm.
  - **Pronto quando:** `pytest` verde.
- [x] 3. **Leitura de `detections.json` e `meta.json`** (R1). `zedcal/io.py`:
  - `load_detections`, que recusa outra `schema_version`;
  - `mono_correspondences`, com todas as tags da câmera;
  - `common_correspondences`, com as tags comuns em ordem de id, como o `matchCommonTags` do C++;
  - `load_meta`, que transforma o `timestamp_ns` (texto) em `int` exato.

  AprilGrid e xadrez. Testes em `tests/test_io_detections.py` (15/15 no total). O `detections.json`
  real da `v2` é lido: no par 0, 17 tags comuns e 21 na esquerda.

  Especificação original da etapa: Validação da versão, quadros por
  câmera e correspondências mono e comuns.
  - Testes primeiro: com um arquivo pequeno de fixture, as tags só de uma câmera aparecem no mono e
    não nas comuns; `schema_version: 2` é recusado.
  - **Pronto quando:** `pytest` verde.
- [x] 4. **YAML do SDK** (R2). `zedcal/calib_file.py`: `read_calibration` (D com 5, 8 ou 14 colunas,
  ou `_FE`; recusa termos não nulos além do 8º) e `write_calibration` (as chaves do C++; recusa
  sobrescrever). Testes em `tests/test_calib_file.py` (23/23 no total).
  - **Achado:** o `cv2` do Python grava uma tupla como matriz 2×1, e o SDK recusa esse `Size`
    ("INVALID CALIBRATION FILE"). A escrita usa uma sequência (`Size: [ 1280, 720 ]`), como o C++,
    e um teste confere isso.
  - **Bancada:** o `zed_stereo_checker --calib_opencv` carregou um YAML gravado pelo pacote
    (`bench_checker_load.sh`).

  Especificação original da etapa: Leitura com D de 5, 8 ou 14 colunas ou `_FE`; escrita igual à do
  C++.
  - Testes primeiro: ida e volta sem perda; D 1×14 com zeros vira 8 coeficientes.
  - **Pronto quando:** `pytest` verde, e o checker carrega um YAML gravado pelo pacote (manual, na
    bancada).
- [x] 5. **Projeção, resíduos e PnP** (R9). `zedcal/residuals.py`: `project`, `residuals` (com
  sinal, x e y separados), `rms_per_corner`, `pose` e `right_pose`. Testes em
  `tests/test_residuals.py` (28/28 no total).
  - Desvio do spec: no lugar do `SOLVEPNP_IPPE_SQUARE`, `solvePnPGeneric` com `SOLVEPNP_IPPE`. O
    IPPE_SQUARE exige os cantos num referencial centrado na tag e numa ordem própria. O IPPE geral
    devolve as mesmas duas soluções do caso planar. A de menor resíduo é refinada com
    `solvePnPRefineLM`, que é o que a armadilha 5 de Hagemann pede.
  - No fisheye, o PnP roda nos pontos normalizados.
  - **Correção (2026-11-03, spec `cobertura`):** com o alvo inclinado 35° na borda da imagem, o
    `pose` errava (196 px de resíduo). A desdistorção interna do `solvePnPGeneric` é grosseira na
    borda da lente grande-angular. Agora o `pose` testa três candidatos — IPPE nos pontos
    desdistorcidos com `undistortPointsIter` (critério apertado), o IPPE como o OpenCV faz e o PnP
    iterativo —, refina cada um com LM nos pontos distorcidos e fica com o de menor resíduo.
    Teste de regressão em `test_residuals.py`. Os resultados reais do M1 e do M3 do piloto não
    mudaram.

  Especificação original da etapa: Resíduos com sinal, por canto; pose livre (IPPE ou
  IPPE_SQUARE + LM) e pose estéreo.
  - Testes primeiro: com dados sintéticos sem ruído, os resíduos são < 1e-9 px. Para uma tag
    isolada, a solução escolhida é a de menor resíduo entre as duas do IPPE.
  - **Pronto quando:** `pytest` verde.
- [x] 6. **Gerador sintético** (R11). `zedcal/synthetic.py`:
  - `zed_like_rig` (f ≈ 534 px, baseline 120 mm, 1280 × 720);
  - `random_poses` (inclinação, posição e distância sorteadas);
  - `generate` (ruído gaussiano por coordenada, alvo empenado com flecha no centro, escala do alvo
    errada). O arquivo declara sempre o alvo nominal, como uma captura real;
  - `write`.

  Testes em `tests/test_synthetic.py` (32/32 no total): o arquivo passa no leitor; com σ = 0,1 px,
  o desvio medido sobre > 4 000 coordenadas fica em 0,1 ± 0,005; sem ruído, o resíduo é < 1e-9; a
  flecha e a escala mudam a imagem.

  Especificação original da etapa: Câmera, poses, ruído e vieses (flecha do alvo, escala); a
  saída é um `detections.json`.
  - Testes primeiro: o arquivo gerado passa no leitor da etapa 3; com σ = 0,1 px, o desvio dos
    resíduos medido sobre 10⁴ cantos fica em 0,1 ± 0,005 px.
  - **Pronto quando:** `pytest` verde.
- [x] 7. **Calibração offline** (R4–R8). `zedcal/calib.py`:
  - **Variantes:** `mono_points` `all`/`common`, `stereo` `joint`/`fixed`, `rejection`
    `none`/`cpp`, modelos `rational8`/`radtan5`;
  - **Funções:** `cpp_compatible()`, `calibrate()`, que devolve a calibração e o relatório (RMS
    nas quatro formas, MSE por coordenada, desvios-padrão mono, vistas removidas), e `save()`,
    que grava `calib_<modelo>_<variante>.yml` e `.report.json` e recusa sobrescrever;
  - **Pontos em `float32`,** como no C++.

  Testes em `tests/test_calib.py` (40/40 no total).
  - **Paridade com o C++:** o modo de compatibilidade, com as detecções reais de
    `recalib_cs/captura_01`, reproduz o `calib_cpp_rational8.yml`. K e T até 1e-6 relativo, D até
    1e-6 absoluto, R até 1e-9.
  - **Critério corrigido:** "recuperar até 1e-6" não se aplica ao `float32`, que tem ~1e-5 px de
    quantização, nem aos coeficientes do modelo racional, que são correlacionados. O teste dos
    dados exatos passou a exigir K e T até 1e-5, R até 1e-7, RMS < 1e-4 px e a mesma projeção do
    alvo até 1e-3 px.
  - **Limitação:** o `cv2.fisheye.stereoCalibrate` do OpenCV 4.5.4 falha no binding Python (asserção
    `!fixedSize()` em todos os formatos testados). O mono fisheye funciona; o estéreo fisheye levanta
    `NotImplementedError` com o motivo. A ZED 2i usa o modelo racional.
  - O NumPy devolve `D` com 14 coeficientes para o modelo racional; `_trim` corta no tamanho do
    modelo e confere que o resto é zero.

  Especificação original da etapa: Mono por câmera (`calibrateCameraExtended`), estéreo
  `joint` ou `fixed` (`stereoCalibrateExtended`), rejeição configurável, modo de compatibilidade,
  YAML e relatório.
  - Testes primeiro: dados sintéticos sem ruído recuperam os parâmetros até 1e-6 relativo;
    com ruído, o RMS por canto fica ≈ σ·√2.
  - **Pronto quando:** `pytest` verde, e o modo de compatibilidade com as detecções reais de
    `2026-11-03_camera_settings` reproduz o YAML do C++ até 1e-6 relativo.
- [x] 8. **Bootstrap** (R10), **com o critério estatístico não atendido** (registrado).
  `zedcal/bootstrap.py`: reamostra as vistas com reposição, com semente fixa, e recalibra cada
  amostra. Devolve as amostras dos parâmetros e o desvio-padrão. A mesma semente dá o mesmo
  resultado (teste rápido).
  - **Medido (2026-11-03,** dados sintéticos com 25 vistas e σ = 0,3 px): dispersão real de fx entre
    30 capturas independentes de 0,27 px; estimativas por bootstrap de 5 capturas: 0,46, 0,21,
    0,43, 0,57 e 0,31 px.
    - O bootstrap **superestima em ~1,6×** (mediana).
    - A estimativa de uma única captura **varia ~2×** de captura para captura.
  - **Causa provável:** cada amostra deixa de fora ~37 % das vistas, entre elas as inclinadas que
    fixam a focal. Hagemann também relata superestimação com poucas imagens.
  - **Consequência:** a incerteza dos experimentos vem da dispersão entre as 10 capturas (EME
    empírico); o bootstrap de uma captura serve como indicação conservadora.
  - O teste estatístico (`ZEDCAL_SLOW=1`, ~15 min) passou a só conferir a ordem de grandeza
    (razão entre 0,7 e 2,5).

  Especificação original da etapa: Reamostragem das vistas com semente fixa.
  - Testes primeiro: a mesma semente dá o mesmo resultado; em dados sintéticos, o desvio-padrão de
    fx por bootstrap fica a menos de 30 % do desvio observado em 50 conjuntos sintéticos
    independentes.
  - **Pronto quando:** `pytest` verde.
- [x] 9. **Configuração e reprodutibilidade** (R12).
  - `zedcal/config.py` cria a pasta de saída sem sobrescrever e copia a config para dentro dela.
  - `python3 -m zedcal.calibrate --config ...` (`zedcal/calibrate.py`) roda uma ou mais variantes,
    com prior opcional e bootstrap opcional. Exemplo em `configs/calibrate_example.yaml`.
  - Testes em `tests/test_command_calibrate.py`: duas execuções da mesma config dão o mesmo YAML e
    o mesmo relatório (exceto o caminho da config). O relatório traz o commit e as versões. Uma
    pasta já usada é recusada.

  Especificação original da etapa: A config YAML é copiada na saída; o relatório
  traz o commit, as versões e a semente.
  - **Pronto quando:** rodar duas vezes a mesma config dá arquivos idênticos, exceto a data.

## Riscos

- **O modo de compatibilidade pode não bater** por diferença de ordem dos pontos ou de critério de
  parada. Comparar vista a vista; usar o mesmo `TermCriteria` do C++.
- **O teste de calibração do bootstrap é estatístico.** Fixar sementes e usar uma tolerância larga
  para não ficar intermitente.
