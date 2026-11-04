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
- [ ] 4. **YAML do SDK** (R2). Leitura com D de 5, 8 ou 14 colunas ou `_FE`; escrita igual à do
  C++.
  - Testes primeiro: ida e volta sem perda; D 1×14 com zeros vira 8 coeficientes.
  - **Pronto quando:** `pytest` verde, e o checker carrega um YAML gravado pelo pacote (manual, na
    bancada).
- [ ] 5. **Projeção, resíduos e PnP** (R9). Resíduos com sinal, por canto; pose livre (IPPE ou
  IPPE_SQUARE + LM) e pose estéreo.
  - Testes primeiro: com dados sintéticos sem ruído, os resíduos são < 1e-9 px. Para uma tag
    isolada, a solução escolhida é a de menor resíduo entre as duas do IPPE.
  - **Pronto quando:** `pytest` verde.
- [ ] 6. **Gerador sintético** (R11). Câmera, poses, ruído e vieses (flecha do alvo, escala); a
  saída é um `detections.json`.
  - Testes primeiro: o arquivo gerado passa no leitor da etapa 3; com σ = 0,1 px, o desvio dos
    resíduos medido sobre 10⁴ cantos fica em 0,1 ± 0,005 px.
  - **Pronto quando:** `pytest` verde.
- [ ] 7. **Calibração offline** (R4–R8). Mono por câmera (`calibrateCameraExtended`), estéreo
  `joint` ou `fixed` (`stereoCalibrateExtended`), rejeição configurável, modo de compatibilidade,
  YAML e relatório.
  - Testes primeiro: dados sintéticos sem ruído recuperam os parâmetros até 1e-6 relativo;
    com ruído, o RMS por canto fica ≈ σ·√2.
  - **Pronto quando:** `pytest` verde, e o modo de compatibilidade com as detecções reais de
    `2026-11-03_camera_settings` reproduz o YAML do C++ até 1e-6 relativo.
- [ ] 8. **Bootstrap** (R10). Reamostragem das vistas com semente fixa.
  - Testes primeiro: a mesma semente dá o mesmo resultado; em dados sintéticos, o desvio-padrão de
    fx por bootstrap fica a menos de 30 % do desvio observado em 50 conjuntos sintéticos
    independentes.
  - **Pronto quando:** `pytest` verde.
- [ ] 9. **Configuração e reprodutibilidade** (R12). A config YAML é copiada na saída; o relatório
  traz o commit, as versões e a semente.
  - **Pronto quando:** rodar duas vezes a mesma config dá arquivos idênticos, exceto a data.

## Riscos

- **O modo de compatibilidade pode não bater** por diferença de ordem dos pontos ou de critério de
  parada. Comparar vista a vista; usar o mesmo `TermCriteria` do C++.
- **O teste de calibração do bootstrap é estatístico.** Fixar sementes e usar uma tolerância larga
  para não ficar intermitente.
