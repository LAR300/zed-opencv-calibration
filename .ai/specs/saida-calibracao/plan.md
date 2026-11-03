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

- [ ] 1. **Agregação e convenção** (R3, R4). `aggregateRms(per_view_rms, corners_per_view)` dá o
  RMS ponderado; `msePerCoordinate(rms)` dá RMS²/2.
  - Testes primeiro: duas vistas com o mesmo número de cantos dão a média quadrática; vistas com
    pesos diferentes dão o valor feito à mão; `msePerCoordinate(0,5) = 0,125`.
  - **Pronto quando:** `ctest` verde.
- [ ] 2. **Seção `calibration` da sessão** (R2). Ela é lida e validada no `loadSession` de
  `capture_io`: `model` em {`rational8`, `fisheye4`}, `max_rms_px > 0`, `outlier_factor > 1`,
  `outlier_floor_px > 0`, `prior` em {`none`, `factory`}. Seção ausente usa os padrões.
  - Testes primeiro: sem a seção, os padrões; cada valor inválido é recusado.
  - **Pronto quando:** `ctest` verde.
- [ ] 3. **Escritor do relatório** (R5–R7). `CalibrationReportWriter` em JSON (`cv::FileStorage`),
  com os campos de R3 a R7.
  - Testes primeiro: um relatório com 2 vistas, uma removida, é relido com os mesmos campos; o
    `json.load` do Python lê o arquivo.
  - **Pronto quando:** `ctest` verde.

### Fase B — calibração

- [ ] 4. **Prior configurável** (R1). No modo sessão, `calibrate()` recebe `use_intrinsic_prior`
  da sessão, com padrão `none`. Fora do modo sessão, nada muda.
  - **Pronto quando:** a regressão do xadrez é idêntica, e a sessão com as imagens de
    `2026-11-03_camera_settings` coincide com o `--use_stored_values` até 1e-6 relativo.
- [ ] 5. **Extended e registro das remoções** (R3, R5, R6).
  - No modo sessão, a última calibração mono usa `calibrateCameraExtended`, e o estéreo usa
    `stereoCalibrateExtended`.
  - Os índices removidos em cada etapa de rejeição são guardados.
  - Fora do modo sessão, as chamadas atuais continuam.
  - **Pronto quando:** a regressão do xadrez é idêntica. Num teste de bancada, os K e D da sessão
    coincidem com os do caminho não-Extended até 1e-9 relativo, porque é a mesma otimização.
- [ ] 6. **RMS fiel e aceite** (R3, R4). Calcula o RMS por câmera no ajuste estéreo e com pose livre
  (`solvePnP` com K e D finais). O aceite no modo sessão usa os valores do ajuste estéreo.
  - **Pronto quando:** um script Python independente, que lê o YAML e o `detections.json`,
    reproduz os dois RMS por câmera até 1e-4 px.
- [ ] 7. **Arquivos na captura** (R7). `calib_cpp_<modelo>.yml` e `.report.json` em
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
