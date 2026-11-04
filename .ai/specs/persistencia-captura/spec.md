# Spec — Persistência da captura: pares brutos, detecções e metadados (R1, R8)

> **O quê, por quê e condições de implementação.** Requisitos de origem:
> [`brief.md`](../../product/brief.md) R1 e R8. Lacunas e riscos citados:
> [`auditoria.md`](../../auditoria.md) §1.1, §1.2, §5 e §6 (riscos 4, 6 e 8).
> **Este spec define os formatos de dados** (§ Formatos) que todos os módulos offline consomem.

## Problema

Uma captura hoje deixa só PNGs em `zed-images/`. A pasta é apagada na captura seguinte
(`stereo_calibration/src/main.cpp:563-577`). Não ficam salvos:
- **as detecções**, que a calibração refaz e descarta;
- **os metadados**, que só são impressos no terminal;
- **a condição** em que a captura foi feita.

Sem isso não dá para:
- reprocessar offline com outros modelos (R1), inclusive refrativos na fase da água (R8);
- rastrear as 10 capturas de cada condição;
- alimentar as métricas com o mesmo detector da calibração (risco 6).

## Contexto

- O detector C++ com o refino D11 (`common/aprilgrid/aprilgrid.cpp:239-283`) é a referência de
  cantos. Hagemann (2022) exige o mesmo detector para o σ_d do Bias Ratio
  ([`referencias.md`](../../product/referencias.md) §A, armadilha 4).
- A calibração de cada câmera hoje usa só as tags comuns às duas (`opencv_calibration.cpp:98`), o
  que empobrece as bordas (risco 4). Para corrigir isso offline, as detecções salvas precisam ser
  **por câmera**.
- O ZED SDK 5.1 tem:
  - `getTimestamp(TIME_REFERENCE::IMAGE)`;
  - `getSDKVersion()`;
  - firmware em `getCameraInformation().camera_configuration.firmware_version`;
  - temperatura `SENSOR_LOCATION::ONBOARD_LEFT/RIGHT/IMU` em `getSensorsData()`
    (`Camera.hpp:4407-4411`). A temperatura depende do acesso ao HID; ver o spec
    [`ambiente-sensores`](../ambiente-sensores/spec.md).
- Os dados ficam fora do repositório, em `ros2_ws/dev/calib_runs/dados/` (decisão do usuário em
  [`vision.md`](../../product/vision.md)).
- O roteiro de poses (spec [`roteiro-poses`](../roteiro-poses/spec.md)) fornece o `pose_id`. Este
  spec só reserva o campo e usa `free` quando não há roteiro.

## Requisitos

### Sessão (modo novo)
- **R1.** Uma nova opção, `--session <arquivo.yaml>`, liga o modo de captura rastreável. Sem ela,
  `zed_stereo_calibration` se comporta exatamente como hoje: `zed-images/`, `zed_calibration_<sn>.yml`
  no diretório corrente e as mesmas mensagens. O modo sessão exige o alvo AprilGrid
  (`--target_yaml`), o da metodologia, e não se combina com `--use_stored_values`, porque ele grava
  uma captura nova.
- **R2.** O arquivo de sessão contém:
  - a raiz de saída e a condição (ex.: `A_ar_sem_acrilico`);
  - o número da captura e a lente;
  - as medidas do alvo, feitas com paquímetro (R7 do brief);
  - a geometria da porta: presente ou não, distância lente–acrílico, espessura, inclinação,
    material e índice;
  - o meio: tipo, temperatura e salinidade da água, índice;
  - uma nota livre.

  Campos desconhecidos (ar, sem porta) são `null`, nunca omitidos. Um campo obrigatório ausente
  gera erro antes de abrir a câmera.
- **R3.** **A saída é a pasta `<raiz>/<condicao>/captura_<XX>/`**, com XX em dois dígitos.
  - Se a pasta já existir e não estiver vazia, a ferramenta **recusa** e encerra antes de abrir a
    câmera. Nada é apagado.
  - Nada é gravado no diretório corrente.

### Estrutura de uma captura
- **R4.** Dentro de `captura_<XX>/`:
  - `left/<pose_id>_<n>.png` e `right/<pose_id>_<n>.png`: o par bruto (não retificado, PNG, BGR,
    resolução cheia), como hoje;
  - `detections.json` e `meta.json` (§ Formatos);
  - `session.yaml`: cópia do arquivo de sessão usado;
  - `capture.log`: a saída do terminal da execução;
  - a calibração da ferramenta. O conteúdo e o nome são do spec
    [`saida-calibracao`](../saida-calibracao/spec.md).
- **R5.** `n` é o índice do par dentro do mesmo `pose_id`, a partir de 0. Sem roteiro, `pose_id` é
  `free` e `n` é sequencial.

### Detecções
- **R6.** Para cada par salvo, `detections.json` guarda as tags **de cada câmera
  separadamente**, com **todas as tags detectadas**, não só as comuns.
  - Cada tag traz os 4 cantos refinados e os 4 cantos antes do refino, na ordem de
    `tagCorners3D()` (BR, BL, TL, TR).
  - As detecções gravadas são as **da imagem salva**, relidas do PNG gravado. Assim o arquivo
    reproduz exatamente o que a calibração usa.
- **R7.** `detections.json` registra:
  - os parâmetros do detector;
  - o alvo (linhas, colunas, tag, espaçamento, unidade, convenção dos IDs);
  - o tamanho da imagem.

  Com isso, os pontos 3D são recalculados sem consultar o código.

### Metadados
- **R8.** `meta.json` registra, uma vez por captura:
  - câmera: modelo, serial, firmware, resolução, fps e lente;
  - versão do ZED SDK;
  - commit do código e se a árvore estava suja;
  - nome do host;
  - início e fim (UTC) e motivo do encerramento;
  - autocalibração desligada (`true`);
  - controles de captura no início e no fim;
  - o conteúdo da sessão (condição, captura, alvo, porta, meio).
- **R9.** `meta.json` registra, por par salvo:
  - `pose_id`, `n` e o timestamp da imagem (ns, do SDK);
  - o uptime do host (s), lido de `/proc/uptime`;
  - o tempo desde a abertura da câmera (s);
  - as temperaturas `ONBOARD_LEFT`, `ONBOARD_RIGHT` e `IMU` (°C).

  Temperatura indisponível é `null`, com um aviso único no início da captura.

### Recálculo de capturas antigas
- **R10.** `--use_stored_values --detections_out <arquivo>` grava o `detections.json` das imagens
  de `zed-images/`, com `pose_id = legacy` e `n` = o índice antigo. A calibração e as saídas atuais
  do recálculo não mudam.
  - Com o xadrez, o arquivo traz `target.type = checkerboard` (cantos internos por linha e coluna,
    e o tamanho do quadrado). Cada câmera tem a lista `corners`, os cantos internos em ordem de
    linha, depois do `cornerSubPix`, como em `opencv_calibration.cpp:114-133`.
  - Isso põe o xadrez de 16/10 no piloto, como contraste do AprilGrid.

### Compatibilidade
- **R11.** Sem `--session` e sem `--detections_out`, o recálculo de
  `calib_runs/regress_chess/` dá resultado idêntico ao `baseline.yml` (`.ai/docs/bench.md`).
- **R12.** `zed_stereo_checker` não muda neste spec.

## Formatos

Versão 1. Todo arquivo traz `schema_version: 1`. Uma mudança incompatível sobe a versão, e os
leitores recusam versões que não conhecem.

**Arquivo de sessão (YAML, entrada).** Exemplo em `config/session_example.yaml`:

```yaml
output_root: /workspaces/isaac_ros-dev/dev/calib_runs/dados
condition: A_ar_sem_acrilico     # [A-Za-z0-9_-]+
capture: 1                       # -> captura_01
lens: 2.1mm
note: ""
target_measured:                 # paquímetro, antes da sessão (R7 do brief)
  tag_size_mm: 88.0
  tag_spacing_mm: 26.4
  planarity_mm: null             # maior desvio do plano medido
  measured_on: 2026-11-03
  # opcionais (ausente = null); obrigatórios na sessão métrica (validacao-metrica R4)
  tag_sides_mm: null             # lista: lados de >= 6 tags
  spacings_mm: null              # lista: espaços de >= 6 pares de tags vizinhas
  board_side_mm: null            # lado total da grade
  planarity_grid_mm: null        # lista: 9 desvios com sinal, grade 3x3, por linha
port:                            # R8 do brief; null no ar sem porta
  present: false
  material: null                 # ex.: PMMA
  thickness_mm: null
  lens_to_port_mm: null
  tilt_deg: null
  refractive_index: null
medium:
  type: air                      # air | fresh_water | sea_water
  water_temperature_c: null
  salinity_psu: null
  refractive_index: null
roteiro: null                    # opcional: modo roteiro (roteiro-poses R5)
pose_calibration: null           # opcional: calibração do PnP de orientação; null = F
```

**`detections.json`.** Uma entrada por par salvo:

```json
{
  "schema_version": 1,
  "image_size": [1280, 720],
  "target": {"type": "aprilgrid", "rows": 6, "cols": 6, "tag_size_mm": 88.0,
             "tag_spacing_ratio": 0.3, "family": "tag36h11", "id_convention": "kalibr",
             "corner_order": ["BR", "BL", "TL", "TR"], "source": "target.yaml"},
  "detector": {"refinement": "xjunction_cornersubpix", "params": {"adaptiveThreshConstant": 25}},
  "frames": [
    {"pose_id": "B1-P-TL", "n": 0,
     "files": {"left": "left/B1-P-TL_0.png", "right": "right/B1-P-TL_0.png"},
     "left":  {"tags": [{"id": 12, "corners": [[x, y], [x, y], [x, y], [x, y]],
                         "corners_unrefined": [[x, y], [x, y], [x, y], [x, y]]}]},
     "right": {"tags": []}}
  ]
}
```

O exemplo de `params` está abreviado. O arquivo traz todos os campos de `DetectorParameters`
usados.

**`meta.json`:**

```json
{
  "schema_version": 1,
  "camera": {"model": "ZED 2i", "serial": 35180697, "firmware": 1523,
             "resolution": [1280, 720], "fps": 15, "lens": "2.1mm"},
  "sdk_version": "5.1.x",
  "code": {"commit": "<sha>", "dirty": false},
  "host": {"hostname": "<nome>"},
  "self_calibration_disabled": true,
  "session": {"...": "cópia do arquivo de sessão"},
  "camera_settings": {"start": {"EXPOSURE": 50}, "end": {"EXPOSURE": 50}},
  "started_utc": "2026-11-03T14:00:00Z", "ended_utc": "...", "end_reason": "...",
  "frames": [
    {"pose_id": "B1-P-TL", "n": 0, "timestamp_ns": "1762178400123456789", "host_uptime_s": 0.0,
     "since_camera_open_s": 0.0,
     "temperature_c": {"onboard_left": null, "onboard_right": null, "imu": null}}
  ]
}
```

`timestamp_ns` é gravado como **texto**: com 19 dígitos, ele não cabe num `double` sem perder os
nanossegundos, e é assim que leitores como o `json` do Python e o `cv::FileStorage` tratam números.

## Condições de implementação

- **C++17, sem dependência nova.** JSON pelo `cv::FileStorage` em modo JSON. Ele já está no
  OpenCV 4.5.4; tags vão em listas, porque chaves do `FileStorage` não podem começar por dígito. O
  YAML de sessão também é lido pelo `FileStorage`.
- **As partes puras ficam em `common/capture_io/`, testáveis sem câmera:** leitura e validação da
  sessão, montagem dos caminhos, recusa de sobrescrever e escrita de `detections.json` e
  `meta.json`. É o mesmo padrão de `common/aprilgrid` (D1 de `aprilgrid-target`) e de
  `common/camera_settings`.
- **O commit entra por um header gerado pelo CMake a cada build**, não só na configuração. Um
  commit lido só na configuração fica desatualizado.
- **A detecção gravada é refeita sobre a imagem relida do PNG,** e não sobre o buffer da câmera.
  Garante a igualdade com o que a calibração e o offline leem. A conversão para cinza difere entre
  `imread(GRAYSCALE)` e `cvtColor` (auditoria §3.1).
- **Precisão dos cantos no JSON:** `double`, com no mínimo 9 dígitos significativos.

## Não-objetivos

- O roteiro de poses, a imobilidade e a nitidez por região (specs
  [`roteiro-poses`](../roteiro-poses/spec.md) e [`captura-estatica`](../captura-estatica/spec.md)).
- O conteúdo da calibração gravada e o prior de fábrica (spec
  [`saida-calibracao`](../saida-calibracao/spec.md)).
- O checker, as ferramentas mono e o visualizador.
- Compactar ou converter imagens.

## Critérios de aceitação

> Validação: ver as etapas de [`plan.md`](plan.md), com o registro de bancada de 2026-11-03.

- [x] Sem `--session`, a regressão do xadrez é idêntica ao `baseline.yml` e o fluxo antigo não
      muda. Conferido depois de cada etapa da Fase B e da C.
- [x] Com `--session`, uma captura ao vivo cria `captura_<XX>/` com `left/`, `right/`,
      `detections.json`, `meta.json`, `session.yaml` e `capture.log`. Nada é gravado no diretório
      corrente. Bancada: `testes_persistencia/teste_etapa8/captura_01`.
      - O YAML da calibração vai para a pasta da captura, mas isso **não foi exercitado ao vivo**:
        a calibração exige 20 pares, e a cena parada só deu 1. Fica para a primeira captura
        completa com o alvo em movimento.
- [x] Uma segunda execução com a mesma sessão é recusada antes de abrir a câmera, e a primeira
      captura fica intacta.
- [x] Sessão com campo obrigatório ausente, condição inválida ou valor inválido gera erro claro e
      código diferente de zero, sem abrir a câmera. O arquivo de sessão não tem
      `schema_version`; a versão desconhecida é recusada nos leitores de `detections.json`.
- [x] `detections.json` contém as tags de cada câmera, inclusive as vistas por uma só. Os cantos
      coincidem com uma nova detecção do PNG salvo: 0 px de diferença, ao vivo e na exportação.
- [x] `json.load` do Python lê `detections.json` e `meta.json` sem erro.
- [x] `meta.json` tem todos os campos de R8 e R9.
      - Com o HID sem permissão, as temperaturas são `null` e o aviso aparece uma vez. Isso está
        coberto pelo teste unitário, mas **não foi exercitado na bancada**, porque o HID estava
        acessível. Fica para o `ambiente-sensores`.
- [x] `--use_stored_values --detections_out` gera o `detections.json` de
      `calib_runs/2026-10-20_ar_aprilgrid_v2/` (29 pares, numa pasta de trabalho), e a calibração
      do recálculo não muda.
- [x] Os testes de `common/capture_io/` passam com `-DBUILD_TESTS=ON` (117/117 no total).

## Perguntas em aberto

- NEEDS CLARIFICATION: **Nome das condições.** Proposta: `A_ar_sem_acrilico`,
  `B_ar_com_acrilico`, `metrico_A`, `metrico_B`. O nome é livre no código.
