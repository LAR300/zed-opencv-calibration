# Plano — Calibração de fábrica "F" no formato OpenCV

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`persistencia-captura`](../persistencia-captura/plan.md), etapas 2, 4 e 5 (caminhos, `meta.json`,
  commit).
- ZED 2i conectada, com o `admin` no grupo `zed` (`.ai/docs/bench.md`).

## Etapas

- [x] 1. **Conversão e escrita compartilhadas.** Feito em `common/calib_io/`:
  - `opencv_calib_file.{hpp,cpp}`, só com OpenCV: `convertRawParameters`, `writeOpenCVCalibration`,
    `sameRawParameters` e `modelName`. A recusa dos termos de prisma vale para F. O chute inicial da
    calibração pode descartá-los;
  - `zed_raw_parameters.hpp`: copia o `sl::CalibrationParameters` campo a campo.

  O `StereoCalib::setFrom` e o `saveCalibOpenCV` do `zed_stereo_calibration` passaram a usar esse
  módulo. Os testes estão em `tests/test_calib_file.cpp` (8 casos; 125/125). A regressão do xadrez
  continua idêntica, o que mostra que o YAML gravado não mudou.

  Especificação original da etapa: Mover `CameraCalib::setFrom`,
  `StereoCalib::setFrom` e `saveCalibOpenCV` para um ponto que as duas ferramentas incluem, sem
  mudar o comportamento. Acrescentar a verificação de `disto[8..11]` (R3).
  - Testes primeiro: uma `sl::CalibrationParameters` montada à mão com Tx = 120 dá `T[0] = −120`;
    `disto[9] ≠ 0` é recusado.
  - **Pronto quando:** `ctest` verde e a regressão do xadrez é idêntica ao `baseline.yml`.
- [x] 2. **Ferramenta `zed_export_factory`** (R1, R2, R5). Em `export_factory/`. A abertura da
  câmera agora vem de `common/calib_io/zed_open.hpp` (`setCalibrationOpenParameters`), compartilhada
  com o `zed_stereo_calibration`.
  - A saída vai para `<raiz>/fabrica/SN<serial>/`: `calib_factory_<modelo>.yml`, `meta.json` (com
    `"session": null`), a cópia do `SN<serial>.conf` e o `export.log`.
  - Uma pasta existente com conteúdo é recusada (`prepareEmptyDir`, testado).
  - `--expect_resolution WxH` para se a câmera abrir em outra resolução.

  **Bancada (2026-11-03), S/N 35180697:**
  - fx L 533,820, cy L 369,065 e `T[0] = −119,982` em 1280 × 720 @ 15 fps;
  - a distorção é a do modelo racional, da seção `[LEFT_DISTO]` do `.conf` (k1 = −0,432559), e não
    o k1/k2 de `[LEFT_CAM_HD]`;
  - a segunda exportação é recusada e a pasta fica intacta;
  - o `meta.json` está completo.

  Especificação original da etapa: Abertura igual à da calibração, saída em
  `fabrica/SN<serial>/` e recusa de sobrescrever.
  - **Pronto quando:** na bancada, o arquivo da S/N 35180697 traz fx L 533,82, cy L 369,065 e
    `T[0] = −119,982`.
- [x] 3. **Ida e volta** (R4). `--verify` reabre a câmera com o arquivo gravado como
  `optional_opencv_calibration_file` e compara os `calibration_parameters_raw` relidos com os
  originais (`sameRawParameters`, 1e-4 relativo). `--verify_file <yml>` faz só essa comparação.

  **Bancada:** passou na S/N 35180697. Uma cópia com fx L 1 % maior falhou com
  `left.fx: 533.82 vs 539.158`, código 1.

  Especificação original da etapa: `--verify` reabre com `optional_opencv_calibration_file` e compara.
  - **Pronto quando:** a verificação passa na S/N 35180697. Uma cópia do YAML com fx alterado em 1 %
    faz a verificação falhar, como prova de que ela detecta divergência.
- [x] 4. **Checker com F** (critério 3), com o critério ajustado pelo usuário em 2026-11-03: em
  casa, sem o alvo, a comparação foi feita nos pares gravados, e não ao vivo.
  - `calib_runs/tools/checker_errors` (descartável) calcula os erros do `zed_stereo_checker` com a
    calibração que o SDK carrega: a interna, ou o arquivo F via `optional_opencv_calibration_file`.
  - Nos 25 pares de `2026-11-03_camera_settings`, a diferença por par entre as duas foi
    **0,0 px**. Medianas com F: esquerda 0,324 px, direita 0,313 px, estéreo 0,421 px.
  - Isso é coerente com a etapa 3: o SDK devolve os mesmos parâmetros, e o checker calcula a partir
    deles.
  - **Pendência de bancada:** repetir ao vivo com o alvo, com o checker de verdade, na próxima
    sessão no laboratório.

  Especificação original da etapa: Com a câmera parada diante do alvo, rodar o checker com a
  calibração interna e depois com o YAML exportado, e registrar os erros no `capture.log` da pasta.
  - **Pronto quando:** os erros medianos de 30 quadros diferem menos que a dispersão quadro a
    quadro.
- [x] 5. **Documentação.** Seção "Factory Calibration Export" no `README.md`; `.ai/docs/bench.md`
  com o uso, a origem da distorção e o `checker_errors`.

  Especificação original da etapa:
  - **Pronto quando:** a seção explica onde F fica e por que ela não é a "verdade".

## Riscos

- **O SDK pode reinterpretar o arquivo,** por exemplo reescalando a resolução. A etapa 3 detecta
  isso. Se acontecer, registrar e investigar antes de usar F nas métricas.
