# Spec — Calibração do zero e relatório fiel na ferramenta C++

> **O quê, por quê e condições de implementação.** Origem:
> [`auditoria.md`](../../auditoria.md) §3.1, §3.4 e §3.5 e §6 (riscos 2, 3 e 5);
> [`brief.md`](../../product/brief.md) R4 (M1, filtro RMS < 0,5 px) e R5 (F como mais uma
> calibração). Depende de [`persistencia-captura`](../persistencia-captura/spec.md) (modo sessão).

## Problema

Três defeitos do relatório e da inicialização atuais afetam a metodologia:

- **A captura ao vivo parte da calibração de fábrica.** O `CALIB_USE_INTRINSIC_GUESS` usa os
  parâmetros raw como chute inicial (`stereo_calibration/src/main.cpp:526-532`;
  `opencv_calibration.cpp:159`); o recálculo parte do zero.
  - O brief trata F como **mais uma calibração**, a ser comparada com as nossas, então as nossas
    precisam ser independentes dela (risco 2).
- **O RMS "Left/Right" do relatório é o da etapa mono.** Os K e D gravados são os que o
  `stereoCalibrate` reotimizou (`opencv_calibration.hpp:186-188`). O filtro M1 (< 0,5 px) é
  aplicado a outro conjunto de parâmetros (risco 3).
- **O relatório não diz o que aconteceu.** Ficam de fora:
  - o erro por vista;
  - quais quadros a rejeição de outliers removeu;
  - os flags e o modelo;
  - a convenção do RMS: por canto, norma 2D (auditoria §3.4).

## Contexto

- **Sequência atual:** mono E/D → rejeição por quadro → `stereoCalibrate` com os intrínsecos
  livres → segunda rejeição → refaz tudo (auditoria §3.1).
- **OpenCV 4.5.4** tem `calibrateCameraExtended` (desvios-padrão e erro por vista) e
  `stereoCalibrateExtended`. A versão estéreo traz o erro por vista E/D, mas **não** os
  desvios-padrão nem as poses por vista.
- **O YAML lido pelo SDK não pode mudar:** chaves `Size`, `K_*`, `D_*`, `R` e `T`
  (`opencv_calibration.cpp:455-477`). O checker o carrega e reproduz erros baixos
  (`calib_runs/2026-11-03_camera_settings/checker_final.log:27`).

## Requisitos

- **R1. Do zero por padrão no modo sessão.** Com `--session`, a calibração parte do zero (flags
  sem `USE_INTRINSIC_GUESS` na etapa mono), como o recálculo de hoje.
  - O prior de fábrica fica disponível com `calibration.prior: factory` no arquivo de sessão.
  - O valor usado vai para o relatório.
- **R2. Parâmetros em arquivo.** No arquivo de sessão, a seção `calibration` define:
  - o modelo: `rational8` (padrão) ou `fisheye4`;
  - o RMS máximo (padrão 0,5 px);
  - o fator e o piso da rejeição de outliers (padrão 2,5 e 0,5 px);
  - o prior.

  Os padrões reproduzem o comportamento de hoje.
- **R3. RMS fiel aos parâmetros gravados.** O relatório traz, para os **intrínsecos finais**:
  - **RMS estéreo,** o valor devolvido pela otimização final;
  - **RMS por câmera no ajuste estéreo:** agregado do erro por vista E e D do
    `stereoCalibrateExtended`, ponderado pelo número de cantos;
  - **RMS por câmera com pose livre:** `solvePnP` por vista com K e D finais, sem R e T. Mede o
    ajuste dos intrínsecos sozinhos.

  O filtro de aceite (≤ RMS máximo) usa os valores do ajuste estéreo e o RMS estéreo, não os da
  etapa mono. O RMS mono continua no relatório, rotulado como "etapa mono, pré-estéreo".
- **R4. Convenção explícita.** Todo RMS no relatório indica a convenção
  `per_corner_2d` (√(Σ‖e‖²/N_cantos)). O relatório traz também o **MSE por coordenada**
  (RMS²/2), usado pelo Bias Ratio.
- **R5. Relatório por vista.** Para cada par usado:
  - `pose_id`, `n`, número de cantos E/D;
  - erro E/D no ajuste estéreo;
  - se foi removido pela rejeição de outliers, em qual etapa e com que erro.
- **R6. Incerteza da etapa mono.** Os desvios-padrão dos intrínsecos E e D vêm do
  `calibrateCameraExtended` na **última** calibração mono, rotulados assim. Os do estéreo não
  existem no OpenCV 4.5.4. O relatório registra a ausência; a incerteza estéreo vem do offline,
  spec [`nucleo-offline`](../nucleo-offline/spec.md).
- **R7. Arquivos.** No modo sessão, dentro de `captura_<XX>/`:
  - `calib_cpp_<modelo>.yml`: o mesmo YAML de hoje, aceito pelo SDK;
  - `calib_cpp_<modelo>.report.json`: o relatório, com `schema_version: 1`, os flags, os critérios
    de parada, o tamanho da imagem, o número de pares e o commit.
- **R8. Compatibilidade.** Sem `--session`, a saída, o arquivo, as mensagens e os números são os de
  hoje: regressão do xadrez idêntica ao `baseline.yml`.
- **R9. Recalcular uma captura de sessão sem a câmera** (acrescentado em 2026-11-03, para verificar
  este spec fora do laboratório). `zed_stereo_calibration --recalibrate <captura_XX>` refaz a
  calibração com:
  - os pares listados no `detections.json` da captura;
  - a seção `calibration` do `session.yaml` dela;
  - o alvo do `--target_yaml`.

  Ele grava os arquivos de R7 na mesma pasta e recusa sobrescrever.
  - Com `prior: factory` e sem câmera, o chute inicial vem da F exportada:
    `<output_root>/fabrica/SN<serial>/calib_factory_<modelo>.yml`, com o serial do `meta.json`.
    O `exportar-fabrica` provou que ela é igual ao que o SDK entrega.

## Condições de implementação

- **A ordem das etapas não muda.** Muda o que é registrado e o critério de aceite no modo sessão.
  Mexer na ordem (por exemplo, `CALIB_FIX_INTRINSIC` no estéreo) é decisão metodológica para o
  offline, onde os modelos são comparados. Não entra aqui.
- **A rejeição de outliers continua por quadro**, com os parâmetros da sessão, e é **registrada**.
  Quadros removidos em umas capturas e não em outras afetam a comparabilidade. O relatório permite
  auditar isso, e a análise (R6 do brief) pode repetir a calibração sem rejeição, offline.
- **O caminho fisheye recebe os mesmos campos de relatório** quando aplicáveis. Sem
  `stereoCalibrateExtended` fisheye, o erro por vista é calculado por projeção.
- **As partes puras ficam em funções testáveis:** agregação ponderada do erro por vista, conversão
  RMS → MSE por coordenada e montagem do relatório.

## Não-objetivos

- Mudar o modelo de distorção, a ordem das etapas ou os flags da otimização no modo antigo.
- Calcular a incerteza estéreo (fica no offline, por bootstrap).
- Mudar o checker.

## Critérios de aceitação

- [ ] Sem `--session`, a regressão do xadrez é idêntica ao `baseline.yml` e ao `baseline.log`.
- [ ] Com `--session` e o prior padrão, o relatório registra `prior: none`. A calibração das
      imagens de `2026-11-03_camera_settings` copiadas para uma sessão reproduz o recálculo
      `--use_stored_values` dessas imagens até 1e-6 relativo.
- [ ] Com `calibration.prior: factory`, o relatório registra o prior e o resultado coincide com a
      calibração ao vivo de hoje para as mesmas imagens.
- [ ] O RMS por câmera no ajuste estéreo, recalculado de forma independente em Python a partir do
      YAML e das detecções, coincide com o relatório até 1e-4 px.
- [ ] O relatório lista os quadros removidos e a etapa de cada remoção; `json.load` o lê.
- [ ] O YAML `calib_cpp_rational8.yml` é carregado pelo `zed_stereo_checker --calib_opencv`.
- [ ] Os testes das funções puras passam.

## Perguntas em aberto

Nenhuma.
