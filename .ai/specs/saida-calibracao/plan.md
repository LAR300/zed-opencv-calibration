# Plano — Calibração do zero e relatório fiel

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`persistencia-captura`](../persistencia-captura/plan.md) concluído: modo sessão e
  `detections.json`.
- Linha de base `calib_runs/regress_chess/`.
- Imagens de `calib_runs/2026-11-03_camera_settings/zed-images/` para as comparações. Elas só são
  lidas, nunca alteradas.

## Etapas

### Fase A — funções puras (TDD)

- [x] 1. **Agregação e convenção** (R3, R4). Em `common/calib_io/rms.hpp`: `aggregateRms`,
  `msePerCoordinate` e `rmsPerCorner`. Testes em `tests/test_rms.cpp` (133/133).

  Especificação original da etapa: `aggregateRms(per_view_rms, corners_per_view)` dá o
  RMS ponderado; `msePerCoordinate(rms)` dá RMS²/2.
  - Testes primeiro: duas vistas com o mesmo número de cantos dão a média quadrática; vistas com
    pesos diferentes dão o valor feito à mão; `msePerCoordinate(0,5) = 0,125`.
  - **Pronto quando:** `ctest` verde.
- [x] 2. **Seção `calibration` da sessão** (R2). `CalibrationConfig` em `session.hpp`. A seção é
  opcional, e cada campo ausente fica com o padrão (rational8; 0,5 px; 2,5; 0,5 px; prior `none`).
  Ela vai também para a cópia da sessão no `meta.json` e para o `config/session_example.yaml`.
  Testes em `test_session.cpp` e `test_meta_io.cpp` (138/138).

  Especificação original da etapa: Ela é lida e validada no `loadSession` de
  `capture_io`: `model` em {`rational8`, `fisheye4`}, `max_rms_px > 0`, `outlier_factor > 1`,
  `outlier_floor_px > 0`, `prior` em {`none`, `factory`}. Seção ausente usa os padrões.
  - Testes primeiro: sem a seção, os padrões; cada valor inválido é recusado.
  - **Pronto quando:** `ctest` verde.
- [x] 3. **Escritor do relatório** (R5–R7). `common/capture_io/calib_report.{hpp,cpp}`
  (`writeCalibrationReport`).
  - Cada RMS vem com a convenção `per_corner_2d`, e há um bloco paralelo `mse_per_coordinate_px2`.
  - Por vista: os cantos usados, o erro no ajuste estéreo e a remoção (etapa e erro).
  - Os desvios-padrão da última calibração mono, e `std_dev_stereo: null`.
  - Os marcadores de `null`/`true`/`false` saíram do `meta_io` para o `json_write.hpp`,
    compartilhado.

  Testes em `tests/test_calib_report.cpp` (143/143); o `json.load` do Python lê o arquivo.

  Especificação original da etapa: `CalibrationReportWriter` em JSON (`cv::FileStorage`),
  com os campos de R3 a R7.
  - Testes primeiro: um relatório com 2 vistas, uma removida, é relido com os mesmos campos; o
    `json.load` do Python lê o arquivo.
  - **Pronto quando:** `ctest` verde.

### Fase B — calibração

- [x] 3a. **`--recalibrate <captura>`** (R9). `runRecalibrate()` no `main.cpp`: lê o
  `session.yaml`, os pares do `detections.json` e o serial do `meta.json`. Com prior `factory`, usa a
  F exportada (`readOpenCVCalibration`, testado). Recusa sobrescrever. O fluxo de sessão fica em
  `stereo_calibration/src/session_calibration.cpp`; `calibrate()` só ganhou o desvio para ele depois
  da detecção, e a regressão do xadrez continua idêntica.

  **Verificado (2026-11-03),** com as 25 imagens de `2026-11-03_camera_settings` copiadas para
  `testes_persistencia/recalib_cs/captura_0{1,2}`
  (`calib_runs/tools/make_session_capture.py`, descartável; a pasta original ficou com o mesmo
  md5):
  - **prior `none`:** o YAML é idêntico ao do `--use_stored_values` dessas imagens (diferença
    relativa 0);
  - **prior `factory`:** o YAML é idêntico ao da calibração ao vivo gravada em
    `2026-11-03_camera_settings/zed_calibration_35180697.yml` (diferença 0);
  - os dois diferem só no RMS da etapa mono (16ª casa): o ajuste estéreo converge para o mesmo
    ponto. Nesta captura, partir da fábrica não muda o resultado (risco 2 da auditoria);
  - a segunda execução é recusada.

  Especificação original da etapa: Ele lê o `session.yaml`, o `meta.json` e o
  `detections.json` da captura e chama `calibrate()` com os pares dela. A F exportada, quando o
  prior é `factory`, vem de `readOpenCVCalibration()` (teste de ida e volta com
  `writeOpenCVCalibration`).
  - **Pronto quando:** numa captura montada com as imagens de `2026-11-03_camera_settings` (cópias
    em `left/free_<n>.png` e `right/`, com `detections.json` gerado pelo `--detections_out`), o
    `--recalibrate` com prior `none` dá o mesmo YAML que o `--use_stored_values` dessas imagens,
    até 1e-6 relativo; e uma segunda execução é recusada.

- [x] 4. **Prior configurável** (R1). No commit da etapa 3a.
  - No modo sessão, o prior vem de `calibration.prior` (padrão `none`): com `none`, o chute inicial
    da fábrica é descartado antes de calibrar.
  - No `--recalibrate`, o prior `factory` vem da F exportada.

  Verificado: a regressão do xadrez é idêntica, e a sessão coincide com o `--use_stored_values`
  (diferença 0).

  Especificação original da etapa: No modo sessão, `calibrate()` recebe `use_intrinsic_prior`
  da sessão, com padrão `none`. Fora do modo sessão, nada muda.
  - **Pronto quando:** a regressão do xadrez é idêntica, e a sessão com as imagens de
    `2026-11-03_camera_settings` coincide com o `--use_stored_values` até 1e-6 relativo.
- [x] 5. **Extended e registro das remoções** (R3, R5, R6). No commit da etapa 3a.
  - `calibrateCameraExtended` e `stereoCalibrateExtended` dão o mesmo resultado das chamadas
    simples: o `--recalibrate` coincide com o `--use_stored_values` com diferença 0.
  - **Remoção:** em `recalib_cs/captura_03`, a imagem direita do par 3 foi trocada pela do par 10.
    O par saiu na etapa estéreo, com RMS de 574,7 px (limiar 70,7 px), junto com o `free_22`
    (127,9 px). O relatório registra os dois, com etapa e erro, e a calibração foi recusada
    (código 1, sem YAML).
  - **Achado do algoritmo herdado**, que este spec não muda de propósito: depois da remoção
    estéreo, as calibrações mono são refeitas a partir dos intrínsecos que o par ruim já tinha
    distorcido, e o resultado ficou em 18 px. O código antigo faz o mesmo. Fica para o
    `nucleo-offline` comparar com uma variante que recalibra do zero depois da rejeição.

  Especificação original da etapa:
  - No modo sessão, a última calibração mono usa `calibrateCameraExtended`, e o estéreo usa
    `stereoCalibrateExtended`.
  - Os índices removidos em cada etapa de rejeição são guardados.
  - Fora do modo sessão, as chamadas atuais continuam.
  - **Pronto quando:** a regressão do xadrez é idêntica. Num teste de bancada, os K e D da sessão
    coincidem com os do caminho não-Extended até 1e-9 relativo, porque é a mesma otimização.
- [x] 6. **RMS fiel e aceite** (R3, R4). No commit da etapa 3a. O aceite no modo sessão usa o RMS
  do ajuste estéreo por câmera e o RMS estéreo.

  Verificado com `calib_runs/tools/check_report_rms.py` (descartável), que recalcula os RMS a partir
  do YAML e do `detections.json`: no `recalib_cs/captura_01`, o RMS com pose livre e o do ajuste
  estéreo das duas câmeras batem com o relatório com diferença ≤ 8,8e-7 px.
  - Medido nessa captura: etapa mono 0,305 / 0,310 px, ajuste estéreo 0,308 / 0,313 px, pose livre
    0,305 / 0,310 px.
  - O RMS que o relatório antigo chamava de "Left/Right" (etapa mono) é 0,003 px menor que o do
    ajuste estéreo dos parâmetros gravados.

  Especificação original da etapa: Calcula o RMS por câmera no ajuste estéreo e com pose livre
  (`solvePnP` com K e D finais). O aceite no modo sessão usa os valores do ajuste estéreo.
  - **Pronto quando:** um script Python independente, que lê o YAML e o `detections.json`,
    reproduz os dois RMS por câmera até 1e-4 px.
- [x] 7. **Arquivos na captura** (R7). `calib_cpp_rational8.yml` e `.report.json` em
  `captura_<XX>/`; o YAML só é gravado se a calibração for aceita, e o relatório sempre.

  Verificado: o `zed_stereo_checker --calib_opencv recalib_cs/captura_01/calib_cpp_rational8.yml`
  abre a câmera e registra a origem "OpenCV file" (`calib_runs/tools/bench_checker_load.sh`).

  Especificação original da etapa: `calib_cpp_<modelo>.yml` e `.report.json` em
  `captura_<XX>/`; nada no diretório corrente.
  - **Pronto quando:** o `zed_stereo_checker --calib_opencv captura_<XX>/calib_cpp_rational8.yml`
    abre a câmera e mostra erros, e o relatório tem todos os campos.

### Fase C — documentação

- [ ] 8. README (seção `calibration` da sessão, relatório e convenção do RMS) e
  `config/session_example.yaml`.
  - **Pronto quando:** o README explica a diferença entre o RMS da etapa mono, o do ajuste estéreo e
    o de pose livre.

## Riscos

- **`stereoCalibrateExtended` pode dar resultado numérico diferente** da chamada simples (outro
  critério de parada padrão). A etapa 5 compara as duas; se diferirem, passar o mesmo
  `TermCriteria` explicitamente.
- **Os índices das vistas mudam depois de cada rejeição.** Manter o mapa para o índice original
  (`pose_id`, `n`), testado na etapa 3.
