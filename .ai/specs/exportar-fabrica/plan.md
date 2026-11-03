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
- [ ] 2. **Ferramenta `zed_export_factory`** (R1, R2, R5). Abertura igual à da calibração, saída em
  `fabrica/SN<serial>/` e recusa de sobrescrever.
  - **Pronto quando:** na bancada, o arquivo da S/N 35180697 traz fx L 533,82, cy L 369,065 e
    `T[0] = −119,982`.
- [ ] 3. **Ida e volta** (R4). `--verify` reabre com `optional_opencv_calibration_file` e compara.
  - **Pronto quando:** a verificação passa na S/N 35180697. Uma cópia do YAML com fx alterado em 1 %
    faz a verificação falhar, como prova de que ela detecta divergência.
- [ ] 4. **Checker com F** (critério 3). Com a câmera parada diante do alvo, rodar o checker com a
  calibração interna e depois com o YAML exportado, e registrar os erros no `capture.log` da pasta.
  - **Pronto quando:** os erros medianos de 30 quadros diferem menos que a dispersão quadro a
    quadro.
- [ ] 5. **Documentação** no README e em `.ai/docs/bench.md`.
  - **Pronto quando:** a seção explica onde F fica e por que ela não é a "verdade".

## Riscos

- **O SDK pode reinterpretar o arquivo,** por exemplo reescalando a resolução. A etapa 3 detecta
  isso. Se acontecer, registrar e investigar antes de usar F nas métricas.
