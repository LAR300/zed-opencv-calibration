# Fluxo de coleta — capturar e calibrar

Como coletar os dados de uma calibração com `zed_stereo_calibration`: o que preparar, que comando
rodar, que parâmetros usar e o que fica gravado. Ao final da coleta a ferramenta **já calibra**,
como sempre fez. A análise do que foi coletado está em
[`fluxo-analise-offline.md`](fluxo-analise-offline.md); a visão geral, em
[`fluxo-calibracao.md`](fluxo-calibracao.md).

**Onde rodar:** tudo no container, como `admin` (`make exec` no host, ou
`docker exec -it --user admin isaac-ros-dev bash`). Caminhos do container
(`/workspaces/isaac_ros-dev` = `ros2_ws/` no host). Nos exemplos:

```bash
BIN=/workspaces/isaac_ros-dev/dev/zed-opencv-calibration/build
CFG=/workspaces/isaac_ros-dev/dev/zed-opencv-calibration/config
DADOS=/workspaces/isaac_ros-dev/dev/calib_runs/dados
```

## Os dois modos da ferramenta

| | Modo clássico (sem `--session`) | Modo sessão (`--session sessao.yaml`) |
|---|---|---|
| Para que serve | Uma calibração rápida | Calibrações que serão comparadas, repetidas ou reprocessadas |
| Poses | Livres, guiadas pelas métricas X, Y, Size, Skew | Roteiro fixo de 63 poses, com a pose pedida na tela |
| Quando salva | Na tecla `s`, se o par for diferente dos anteriores | Na tecla `s`, só com a pose na tolerância e o alvo parado e nítido |
| Fim da coleta | Métricas completas (entre 25 e 35 pares) ou `f` | Fim do roteiro |
| Onde grava | `zed-images/` do diretório atual, **apagada a cada coleta nova** | `<output_root>/<condição>/captura_XX/`, nunca sobrescrita |
| O que grava | Pares e o YAML da calibração | Pares, todas as detecções, metadados, YAML e relatório |
| Alvo | Xadrez ou AprilGrid | Só AprilGrid (`--target_yaml`) |

O modo clássico continua igual ao original. O resto deste documento trata do modo sessão, que é
o da metodologia.

## Preparação

### Uma vez por câmera

1. **Acesso ao SDK e ao HID** (temperaturas e IMU no `meta.json`). Roda **no host**, na raiz do
   repositório, porque inspeciona o USB e usa `docker exec`:

   ```bash
   scripts/bench/check_zed_access.sh --check
   ```

   Dentro do container, só as temperaturas: `$BIN/access_check/zed_access_check` (0: ok; 2: a
   câmera não abre; 3: sem HID). Correções em [`bench.md`](bench.md#permissões).

2. **Exportar a calibração de fábrica F.** É a referência de comparação e também a calibração que
   a tela do roteiro usa para medir a pose do alvo.

   ```bash
   $BIN/export_factory/zed_export_factory --output_root $DADOS --verify --expect_resolution 1280x720
   ```

   Grava `$DADOS/fabrica/SN<serial>/calib_factory_rational8.yml`, `meta.json`, a cópia do
   `SN<serial>.conf` e `export.log`. `--verify` reabre a câmera com o arquivo e confere que o SDK lê
   os mesmos parâmetros. A ZED 2i do laboratório (S/N 35180697) já está exportada.

### Uma vez por condição (A, B…)

3. **O alvo.** Qualquer AprilGrid Kalibr (`tag36h11`) descrito por um `target.yaml`. O do
   laboratório está em `config/aprilgrid_6x6_88mm.yaml`. Meça com paquímetro o lado de vários tags
   e o espaço entre eles; com régua e lâmina, a planicidade. Os valores vão para
   `target_measured` na sessão e, se diferirem do nominal, também para o `target.yaml`, que é o que
   a calibração usa. Cuidados:
   - **o mesmo alvo físico em todas as capturas comparadas**: o defeito do alvo entra na
     calibração;
   - **pelo menos ~6×6 tags**: cada par precisa de 12 tags nas duas câmeras, em 2 linhas e 2
     colunas, inclusive com metade do alvo fora da imagem;
   - **rígido e plano**. O alvo do laboratório tem os cantos ~0,4 mm fora da grade e ~0,23 % de
     esticamento em Y, o que aparece como viés (ver `fluxo-analise-offline.md`).

4. **Limiares de imobilidade.** Com câmera e alvo parados, a ~0,4 m, por 30 s, **com os mesmos
   controles de câmera da coleta**:

   ```bash
   cd $BIN/stereo_calibration
   ./zed_stereo_calibration --target_yaml $CFG/aprilgrid_6x6_88mm.yaml \
       --camera_settings $CFG/camera_settings_zed2i.yaml \
       --session sessao_ruido.yaml --measure_static_noise 30
   ```

   A sessão deste modo usa uma condição própria (`condition: ruido`) e `roteiro: null`. O
   terminal e o `noise.json` propõem `max_displacement_px` (p95 do deslocamento entre quadros) e
   `min_target_sharpness` (metade do p05 da nitidez); copie para a seção `static_capture` da sessão
   de coleta. Em 2026-11-04, com um AprilGrid 5×7: 0,50 px e 1428 com os controles fixos, contra
   0,78 px com a exposição automática.

5. **Tempo de aquecimento X** (uma vez, em três partidas a frio em dias diferentes; ainda não
   medido). Câmera e alvo fixos, câmera desligada há pelo menos 2 h:

   ```bash
   ./zed_stereo_calibration --target_yaml $CFG/aprilgrid_6x6_88mm.yaml \
       --camera_settings $CFG/camera_settings_zed2i.yaml \
       --session sessao_termico.yaml --timelapse 30 60
   ```

   Um par a cada 30 s por 60 min, sem calibrar (`condition: termico`, `roteiro: null`). A análise
   (`zedcal.thermal_command`) propõe X. Até lá, espere ~30 min com a câmera ligada.

## A sessão (`sessao.yaml`)

Ponto de partida: [`config/sessao_A_exemplo.yaml`](../../config/sessao_A_exemplo.yaml) (condição A
com o roteiro) ou [`config/session_example.yaml`](../../config/session_example.yaml) (todos os
campos comentados). Copie para fora do repositório, por exemplo `$DADOS/sessoes/sessao_A.yaml`.
**Todo campo é obrigatório**; use `null` para o que não se aplica e aspas em textos que começam
com dígito.

| Campo | O que é | Exemplo |
|---|---|---|
| `output_root` | Raiz das capturas | `/workspaces/isaac_ros-dev/dev/calib_runs/dados` |
| `condition` | Condição experimental (letras, dígitos, `_`, `-`) | `A_ar_sem_acrilico` |
| `capture` | Número da captura, 1–99; a pasta `captura_XX` não pode existir | `1` |
| `lens`, `note` | Lente e uma observação livre | `"2.1mm"`, `""` |
| `target_measured` | Medidas do alvo: `tag_size_mm`, `tag_spacing_mm`, `planarity_mm`, `measured_on`; as listas `tag_sides_mm`, `spacings_mm`, `board_side_mm`, `planarity_grid_mm` só na sessão do teste métrico | `88.0`, `26.4` |
| `port` | Porta plana à frente da câmera: `present`, `material`, `thickness_mm`, `lens_to_port_mm`, `tilt_deg`, `refractive_index`; tudo `null` no ar | `present: false` |
| `medium` | `type` (`air`, `fresh_water`, `sea_water`), temperatura, salinidade, índice | `type: air` |
| `calibration` | `model` (`rational8`/`fisheye4`), `max_rms_px` (aceite, 0,5), `outlier_factor` e `outlier_floor_px` (rejeição de vistas), `prior` (`none` = do zero; `factory` = parte de F) | os padrões |
| `static_capture` | `window_s` (tempo parado, 1 s), `max_displacement_px`, `min_target_sharpness` (0 desliga) | do `noise.json` |
| `roteiro` | Arquivo do roteiro; `null` = coleta livre | `.../config/roteiro_v1.yaml` |
| `pose_calibration` | Calibração usada só para medir a pose na tela; `null` = a F em `<output_root>/fabrica/SN<serial>/` | `null` |

Para cada captura nova, mude só `capture:`. A ferramenta copia a sessão usada para dentro da
pasta da captura.

## A coleta

Com a câmera aquecida:

```bash
cd $BIN/stereo_calibration
./zed_stereo_calibration --target_yaml $CFG/aprilgrid_6x6_88mm.yaml \
    --camera_settings $CFG/camera_settings_zed2i.yaml \
    --session $DADOS/sessoes/sessao_A.yaml
```

### Parâmetros da linha de comando

| Parâmetro | Para que serve |
|---|---|
| `--target_yaml <arquivo>` | O AprilGrid (obrigatório no modo sessão) |
| `--camera_settings <arquivo>` | Exposição, ganho, white balance etc. fixos durante a coleta, restaurados ao sair. Use sempre `config/camera_settings_zed2i.yaml` (ou uma cópia ajustada) |
| `--session <arquivo>` | Ativa o modo sessão |
| `--min_tags`, `--min_rows`, `--min_cols` | Mínimo de tags (12), linhas (2) e colunas (2) vistas nas duas imagens para um par valer |
| `--detector_params <arquivo>` | Ajuste fino do detector AprilGrid (campos do `cv::aruco::DetectorParameters`) |
| `--min_samples`, `--max_samples` | Só no modo clássico: mínimo para encerrar (25) e máximo antes de pausar (35, 0 = sem máximo) |

### A tela no modo roteiro

- **Imagem esquerda:** o contorno amarelo é o alvo **na pose pedida** e o `+` amarelo, o centro
  dele. Nas poses de borda e de perto o contorno sai da imagem e aparece cortado: o alvo deve
  ficar parcialmente fora da imagem.
- **Painel na imagem direita:** a pose pedida (linha branca: largura do alvo como fração da
  imagem, centro, rotação) e a medida (linha vermelha/verde: largura, centro, inclinações H e V,
  rotação e distância), o tempo parado e a nitidez. Verde = dentro da tolerância.
- **Painel de baixo** (X, Y, Size, Skew): é do modo clássico e não vale no modo roteiro.
- **Teclas:**

  | Tecla | Ação |
  |---|---|
  | `s` | Salva o par, só com todas as grandezas na tolerância, tags suficientes e o alvo parado e nítido; senão diz por quê |
  | `o` | Salva uma pose impossível de atingir, marcada `out_of_tolerance` (ainda exige tags, imobilidade e nitidez) |
  | `k` | Pula a pose |
  | `b` | Volta uma pose (nada é apagado; o novo par ganha o próximo número) |
  | `q` / ESC | Aborta sem calibrar |

- **O roteiro v1** (`config/roteiro_v1.yaml`, 63 poses) tem 4 blocos: 27 poses de frente em 3
  distâncias × 9 posições; 20 inclinadas ±35° nos dois eixos; 12 com o alvo girado 0/90/180/270°;
  4 com metade do alvo fora da imagem. As distâncias são frações da largura da imagem: perto
  70–80 %, médio 40–50 %, longe 20–25 %. Tolerâncias: 0,05 na fração, 0,08 na posição, 5° nos
  ângulos.

No fim do roteiro a ferramenta lista as poses salvas, fora da tolerância e puladas e, com pelo
menos 20 pares, **calibra e mostra o RMS e os parâmetros, como antes**.

### O que fica gravado

Em `$DADOS/<condição>/captura_XX/`:

| Arquivo | Conteúdo |
|---|---|
| `left/`, `right/` | Os pares brutos, sem retificação (`<pose_id>_<n>.png`) |
| `detections.json` | Todos os tags detectados por câmera, com os cantos antes e depois do refino |
| `meta.json` | Câmera, SDK, firmware, commit do código, controles da câmera no início e no fim; por par: timestamp, temperaturas, pose medida, imobilidade; os eventos do roteiro |
| `calib_cpp_rational8.yml` | A calibração no arquivo que o ZED SDK lê; só se aceita (RMS < `max_rms_px`) |
| `calib_cpp_rational8.report.json` | Como foi calculada e quão bem os parâmetros salvos ajustam: RMS estéreo, RMS de cada câmera com os parâmetros salvos (`stereo_fit_*`) e com pose livre (`free_pose_*`), erro por vista, vistas removidas, desvios-padrão |
| `session.yaml`, `capture.log` | A sessão usada e toda a saída do terminal |

Para usar a calibração no SDK, aponte `optional_opencv_calibration_file` para o
`calib_cpp_rational8.yml`.

## Aceitar ou refazer a captura

1. **Cobertura** (detalhes em `fluxo-analise-offline.md`, passo 3):

   ```bash
   cd /workspaces/isaac_ros-dev/dev/zed-opencv-calibration/analysis
   python3 -m zedcal.coverage_command --config cobertura_A01.yaml
   ```

   ```yaml
   # cobertura_A01.yaml
   detections: /workspaces/isaac_ros-dev/dev/calib_runs/dados/A_ar_sem_acrilico/captura_01/detections.json
   calibration: /workspaces/isaac_ros-dev/dev/calib_runs/dados/fabrica/SN35180697/calib_factory_rational8.yml
   output_dir: /workspaces/isaac_ros-dev/dev/calib_runs/analises/A_ar_sem_acrilico/cobertura_01
   ```

   Sai com 0 (aprovada) ou 1 (reprovada, com o motivo).
2. **Critérios de aceite** (spec `experimentos`, R3): cobertura aprovada; RMS de cada câmera e do
   estéreo < 0,5 px; metadados completos.
3. **Reprovada:** crie na pasta um arquivo `REPROVADA` com o motivo e repita com o número
   seguinte. Nada é apagado.

Entre duas capturas, retire o alvo da cena e recoloque; não mexa na câmera nem na porta. Repita
até ter **10 capturas aceitas** da condição.

## Calibrar sem a câmera

Três formas de calibrar dados já gravados, com a mesma ferramenta:

| Caso | Comando |
|---|---|
| Captura de sessão (`captura_XX/`) | `./zed_stereo_calibration --target_yaml <alvo> --recalibrate <captura_XX>` — usa a seção `calibration` do `session.yaml` da captura; não sobrescreve resultados |
| Captura do modo clássico (`zed-images/`) | Numa pasta de trabalho com um link `zed-images` para a pasta original: `./zed_stereo_calibration --use_stored_values --target_yaml <alvo> --detections_out detections.json` (xadrez: `--square_size <mm>` no lugar do alvo). Grava `zed_calibration_0.yml` e, com `--detections_out`, o `detections.json` que a análise offline usa |
| Gravação SVO | `./zed_stereo_calibration --svo <arquivo.svo2> --target_yaml <alvo>` (não combina com `--session` nem `--camera_settings`) |

As capturas antigas de `calib_runs/` (ver `calib_runs/README.md`) entram na análise pela segunda
forma; `analysis/exemplos/preparar.sh` faz isso para as quatro capturas AprilGrid.

## Pendências que afetam a coleta

- X do aquecimento e os limiares de imobilidade com o alvo do laboratório, nas distâncias P, M e L.
- A faixa "perto" do roteiro (70–80 % da largura) não cabe na altura da imagem com alvo quadrado;
  essas poses tendem a ser salvas com `o`. Com alvos pequenos (ex. 26 × 37 cm), as poses de perto
  nos cantos podem não ter 12 tags nas duas câmeras: pule com `k`.
- N mínimo de cantos por célula da cobertura: provisório em 1.

Lista completa: [`../product/pendencias-bancada.md`](../product/pendencias-bancada.md).
