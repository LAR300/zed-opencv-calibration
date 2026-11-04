# Fluxo de calibração — passo a passo

Este guia mostra **em que ordem** usar as ferramentas para fazer uma calibração rastreável e
comparável. A referência de cada flag e de cada arquivo está no [`README.md`](../../README.md)
(seções *Traceable Capture*, *Pose Script*, *Other Session Modes* e *Offline Analysis*). O protocolo
dos experimentos A, B e B′ está em [`../specs/experimentos/spec.md`](../specs/experimentos/spec.md).
O ambiente (permissões, container) está em [`bench.md`](bench.md).

Todos os comandos rodam no container, como `admin`:
`docker exec -it --user admin isaac-ros-dev bash`. Os caminhos são os do container
(`/workspaces/isaac_ros-dev` = `ros2_ws/` no host). Abaixo, `BIN` é
`/workspaces/isaac_ros-dev/dev/zed-opencv-calibration/build` e `DADOS` é
`/workspaces/isaac_ros-dev/dev/calib_runs/dados`.

## Antes × agora

**Sem `--session`, a ferramenta funciona exatamente como antes:** você captura, ela calibra e
mostra o RMS e os parâmetros. O fluxo novo acrescenta o que vem antes e depois da captura:

| | Antes | Agora |
|---|---|---|
| Ferramenta | `zed_stereo_calibration` | A mesma, com `--session sessao.yaml` |
| Poses | Livres | Roteiro fixo de 63 poses, pedidas pela tela |
| Quando salva | Na tecla | Só com o alvo parado, nítido e na pose pedida |
| Resultado | YAML + RMS no terminal | O mesmo, mais o relatório e todos os dados brutos |
| Onde grava | `zed-images/`, apagada a cada execução | Uma pasta por captura, que nunca é apagada |
| Depois | Nada | Cobertura, métricas M1–M4 e estatística, offline |
| Quantas | Uma | 10 por condição, para a estatística |

Por que isso: um RMS baixo numa calibração isolada não prova que ela está certa. Nas capturas
AprilGrid existentes o RMS era ~0,31 px, mas ~70 % do erro era viés do alvo
([`../specs/piloto-dados-existentes/resultado.md`](../specs/piloto-dados-existentes/resultado.md)).
Para comparar com a calibração de fábrica ou medir o efeito do acrílico, é preciso capturas
repetíveis, dados guardados e várias capturas por condição.

```
uma vez por câmera     uma vez por condição          cada captura (x10)                  no fim da condição
─────────────────     ────────────────────          ─────────────────────               ──────────────────
1 acesso ao HID   →   3 medir o alvo           →    5 sessao.yaml (capture: i)    →     8 M2, M3, M4
2 exportar F          4 aquecer a câmera            6 capturar com o roteiro            9 relatório e portão
                        (todo dia)                  7 cobertura: aprova ou refaz
```

## Uma vez por câmera

### 1. Acesso ao SDK e ao HID

Sem o HID não há temperatura do sensor no `meta.json`, nem IMU. A verificação completa roda **no
host**, fora do container: ela inspeciona os dispositivos USB e usa `docker exec`. Dentro do
container ela falha com `container isaac-ros-dev is not running`.

```bash
# no host, na raiz do repositório zed-opencv-calibration
scripts/bench/check_zed_access.sh --check
```

Dentro do container, só a leitura das temperaturas pelo SDK:
`$BIN/access_check/zed_access_check` (0: ok; 2: a câmera não abre; 3: sem acesso ao HID).

Se falhar, siga a seção *Permissões* de [`bench.md`](bench.md). A regra udev é instalada uma vez,
no host, com `sudo` (`scripts/bench/install_zed_udev.sh`).

### 2. Exportar a calibração de fábrica (F)

F é a referência de comparação, e também é usada pela tela do roteiro para medir a pose do alvo.

```bash
$BIN/export_factory/zed_export_factory --output_root $DADOS --verify --expect_resolution 1280x720
```

Grava `$DADOS/fabrica/SN<serial>/calib_factory_rational8.yml`. Uma pasta existente nunca é
sobrescrita.

## Uma vez por condição

### 3. Medir o alvo

Com paquímetro: o lado do tag e o espaço entre tags (vários, em lugares diferentes do alvo), e a
planicidade com régua e lâmina. Os valores vão para `target_measured` na sessão. Se diferirem do
`target.yaml`, corrija também o `target.yaml`: é ele que define a geometria usada na calibração.

### 4. Aquecer a câmera (todo dia de captura)

A câmera tem de estar ligada há pelo menos X minutos antes da primeira captura. **X ainda não foi
medido**: a medição é feita uma vez, com três partidas a frio em dias diferentes, câmera e alvo
fixos:

```bash
./zed_stereo_calibration --target_yaml target.yaml --session sessao_termico.yaml --timelapse 30 60
python3 -m zedcal.thermal_command --config termico.yaml      # propõe X
```

As sessões desses dois modos não podem ter `roteiro` (deixe `roteiro: null`) e usam uma condição
própria, por exemplo `termico` e `ruido`. Na mesma ocasião, meça os limiares de imobilidade (alvo
e câmera parados, 30 s) e copie os valores sugeridos no `noise.json` para a seção `static_capture`
da sessão:

```bash
./zed_stereo_calibration --target_yaml target.yaml --session sessao_ruido.yaml --measure_static_noise 30
```

Até lá, os limiares de `static_capture` são provisórios.

## Cada captura

### 5. Preparar a sessão

Copie [`../../config/sessao_A_exemplo.yaml`](../../config/sessao_A_exemplo.yaml) para fora do
repositório, por exemplo `$DADOS/sessoes/sessao_A.yaml`, e preencha as medidas do alvo. Para cada
captura nova, mude só `capture:` (1, 2, 3…). A ferramenta guarda uma cópia da sessão usada dentro
da pasta da captura.

### 6. Capturar

Com a câmera aquecida:

```bash
cd $BIN/stereo_calibration
./zed_stereo_calibration --target_yaml <target.yaml> \
    --camera_settings ../../config/camera_settings_zed2i.yaml \
    --session $DADOS/sessoes/sessao_A.yaml
```

- A imagem esquerda mostra onde o alvo deve ficar. O painel na direita mostra a pose pedida e a
  medida, em verde dentro da tolerância e em vermelho fora.
- Teclas:
  - `s`: salva. Só salva com todas as grandezas na tolerância, tags suficientes e o alvo parado e
    nítido;
  - `o`: salva uma pose impossível de atingir, marcada como fora da tolerância;
  - `k`: pula a pose;
  - `b`: volta uma pose (nada é apagado);
  - `q`: aborta.
- No fim do roteiro a ferramenta lista as poses salvas, fora da tolerância e puladas, e **calibra,
  como antes**, se houver pelo menos 20 pares. O máximo de amostras não se aplica no modo roteiro.
- O terminal mostra o RMS e os parâmetros, como antes.

O resultado fica em `$DADOS/A_ar_sem_acrilico/captura_01/`:

| Arquivo | O que é |
|---|---|
| `calib_cpp_rational8.yml` | A calibração no formato que o ZED SDK lê; só é gravada se aceita (RMS < `max_rms_px`) |
| `calib_cpp_rational8.report.json` | RMS do estéreo e de cada câmera com os parâmetros salvos, erro por vista, vistas removidas, desvios-padrão |
| `left/`, `right/` | Os pares brutos, sem retificação |
| `detections.json` | Todos os tags detectados por câmera, com os cantos antes e depois do refino |
| `meta.json` | Câmera, SDK, commit, temperaturas, a pose medida de cada par e os eventos do roteiro |
| `session.yaml`, `capture.log` | A sessão usada e toda a saída do terminal |

### 7. Conferir a cobertura

```yaml
# cobertura_A01.yaml
detections: /workspaces/isaac_ros-dev/dev/calib_runs/dados/A_ar_sem_acrilico/captura_01/detections.json
calibration: /workspaces/isaac_ros-dev/dev/calib_runs/dados/fabrica/SN<serial>/calib_factory_rational8.yml
output_dir: /workspaces/isaac_ros-dev/dev/calib_runs/dados/A_ar_sem_acrilico/captura_01/cobertura
```

```bash
cd /workspaces/isaac_ros-dev/dev/zed-opencv-calibration/analysis
python3 -m zedcal.coverage_command --config cobertura_A01.yaml
```

Sai com 0 se aprovada e 1 se reprovada; o motivo é impresso e fica em `coverage.json`, e
`coverage.png` mostra os cantos por célula e as inclinações. **Reprovada:** crie na pasta da
captura um arquivo `REPROVADA` com o motivo e repita como uma nova captura, com o número seguinte.
Nada é apagado.

Uma captura é aceita quando a cobertura passa, o RMS de cada câmera e do estéreo fica abaixo de
0,5 px e os metadados estão completos (spec `experimentos`, R3). Entre duas capturas, retire o alvo
da cena e recoloque; não mexa na câmera.

Repita 5 a 7 até ter **10 capturas aceitas** da condição.

## No fim da condição

Todos os comandos seguem o mesmo padrão: `python3 -m zedcal.<comando> --config <arquivo.yaml>`,
rodado de `analysis/`. Cada comando documenta a configuração no próprio início do arquivo
(`analysis/zedcal/<comando>.py`), copia a configuração para a pasta de saída e nunca sobrescreve
uma pasta existente. Uma demonstração completa, sobre as capturas do piloto, está em
`calib_runs/demo/` (`rodar.sh` e `configs/`).

| Ordem | Comando | O que responde |
|---|---|---|
| 8a | `m1_command` | Em cada captura: RMS, mapa de resíduos e Bias Ratio (o erro é ruído ou viés?) |
| 8b | `m2_command` | Quanto as 10 calibrações diferem entre si e de F; gera a referência média |
| 8c | `m3_command` | Cada calibração nas capturas que ela não usou (generalização) |
| 8d | `m4_command` | Erro epipolar, erro métrico em %, resíduo de plano e viés de disparidade |
| 9 | `report_command` | Estatística da condição ou comparação A × B; responde aos portões |

O teste métrico em distâncias conhecidas é uma sessão à parte, com
`roteiro: .../config/roteiro_metrico_v1.yaml` e todas as medidas de `target_measured`
preenchidas, analisada com `metric_test_command`.

## O alvo

Qualquer AprilGrid no formato Kalibr serve (`target.yaml` com `tagRows`, `tagCols`, `tagSize`,
`tagSpacing`, família `tag36h11`, numeração do Kalibr). O roteiro é definido em fração da largura
da imagem, então vale para alvos de outro tamanho. Cuidados:

- **O mesmo alvo físico em todas as capturas que serão comparadas** (as 10 de A e as 10 de B). O
  viés do alvo entra na calibração, e trocar de alvo no meio confunde o efeito do alvo com o da
  condição.
- **Pelo menos ~6×6 tags.** Cada par precisa de 12 tags vistos pelas duas câmeras, em 2 linhas e 2
  colunas, inclusive nas poses com metade do alvo fora da imagem (bloco 4).
- **Rígido e plano, e medido** (passo 3). O alvo atual tem cantos ~0,4 mm fora da grade e está
  ~0,23 % esticado em Y; isso aparece como viés na calibração.
- **O roteiro métrico supõe o alvo de 660 mm.** Para outro tamanho, mude `board_mm` em
  `config/make_roteiro_v1.py` e gere os roteiros de novo.

## Pendências que afetam este fluxo

- X do aquecimento e os limiares de `static_capture`: medir na bancada (passo 4).
- N mínimo de cantos por célula da cobertura: provisório em 1.
- Margens do TOST no relatório: provisórias (`margins_approved: false`).
- A faixa "perto" do roteiro (70–80 % da largura) não cabe na altura da imagem com alvo quadrado;
  essas poses tendem a ser salvas com `o`.

A lista completa está em [`../product/pendencias-bancada.md`](../product/pendencias-bancada.md).
