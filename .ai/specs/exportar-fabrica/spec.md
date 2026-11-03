# Spec — Calibração de fábrica "F" no formato OpenCV (R5)

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R5; [`auditoria.md`](../../auditoria.md) §1.4.

## Problema

O brief trata a calibração de fábrica como **mais uma calibração**, "F", a passar por **todas** as
métricas (M1–M4, R6). Hoje F só é lida dentro das ferramentas, como chute inicial
(`stereo_calibration/src/main.cpp:532`) ou para o checker (`stereo_checker/src/main.cpp:406`).
- Não existe um arquivo F no mesmo formato das nossas calibrações.
- O `SN<serial>.conf` copiado de `/usr/local/zed/settings/` está em outro formato, com várias
  resoluções. Convertê-lo à mão arrisca errar a resolução ou o sinal de T.

## Contexto

- **O que o SDK entrega:** `calibration_parameters_raw` são os parâmetros **não retificados** na
  resolução aberta.
  - A ferramenta abre em `RESOLUTION::AUTO`, que na ZED 2i é HD720, a mesma do SLAM
    (`main.cpp:392-394`).
  - Com `camera_disable_self_calib = true`, o SDK usa os parâmetros do arquivo de fábrica sem
    ajuste (documentação do SDK, em [`referencias.md`](../../product/referencias.md) §E).
- **A conversão para OpenCV já existe e é usada há meses:**
  - `CameraCalib::setFrom` (`opencv_calibration.hpp:37-62`): 8 coeficientes, ou 4 se fisheye;
  - `StereoCalib::setFrom` (`:141-156`): Tx negado e R pelo vetor de rotação.
- **O formato de saída é o de `saveCalibOpenCV`** (`opencv_calibration.cpp:455-477`), aceito pelo
  SDK.
- **A câmera de bancada não é a do robô** ([`vision.md`](../../product/vision.md)). Cada câmera tem
  o seu F, identificado pelo serial.

## Requisitos

- **R1.** Uma ferramenta, `zed_export_factory`, abre a câmera conectada exatamente como
  `zed_stereo_calibration`:
  - mesma resolução e fps;
  - autocalibração desligada;
  - mesmo seletor de câmera.

  Ela grava a calibração raw no mesmo YAML que a calibração produz.
- **R2.** A saída vai para `<raiz>/fabrica/SN<serial>/`, com a raiz dada por `--output_root`:
  - `calib_factory_<modelo>.yml`: o YAML OpenCV;
  - `meta.json`: o mesmo esquema de [`persistencia-captura`](../persistencia-captura/spec.md),
    sem `frames` e com `session: null`;
  - `SN<serial>.conf`: cópia do arquivo de fábrica do SDK, se existir.

  Uma pasta existente e não vazia é recusada.
- **R3.** **Se a distorção da fábrica tiver termos que o modelo de 8 coeficientes não representa**
  (`disto[8..11]` diferentes de zero, os termos thin-prism do SDK), a ferramenta **para com erro**.
  Assim F nunca é truncada em silêncio.
- **R4.** **Verificação de ida e volta.** `--verify` reabre a câmera com
  `optional_opencv_calibration_file` apontando para o arquivo gerado e compara os
  `calibration_parameters_raw` relidos com os originais. Diferença acima de 1e-4 relativo é erro.
- **R5.** Se a resolução aberta não for a pedida pelo arquivo de sessão ou pela linha de comando, a
  ferramenta para com erro. Isso protege contra um modo AUTO que mude de valor.

## Condições de implementação

- **A conversão e a escrita do YAML vêm de um lugar só,** compartilhado com
  `stereo_calibration`. Uma segunda conversão poderia divergir em sinal ou ordem.
- **O fisheye é tratado como na conversão atual**, pela regra p1 = p2 = 0 e k3, k4 ≠ 0. A ZED 2i
  usa o modelo racional.

## Não-objetivos

- Exportar outras resoluções além da aberta.
- Mudar a calibração de fábrica ou o arquivo do SDK.

## Critérios de aceitação

- [ ] `zed_export_factory` grava `calib_factory_rational8.yml` para a S/N 35180697, com fx L 533,82
      e baseline 119,982 mm em HD720, conforme `SN35180697.conf` `[LEFT_CAM_HD]` e `[STEREO]`.
- [ ] `--verify` passa: os parâmetros relidos pelo SDK a partir do arquivo coincidem até 1e-4
      relativo.
- [ ] O `zed_stereo_checker --calib_opencv calib_factory_rational8.yml` mostra, numa mesma cena
      parada, os mesmos erros que o checker com a calibração interna, a menos do ruído entre
      quadros.
- [ ] Uma segunda execução para a mesma câmera é recusada; a primeira saída fica intacta.
- [ ] `meta.json` tem serial, firmware, versão do SDK, resolução e commit.

## Perguntas em aberto

Nenhuma.
