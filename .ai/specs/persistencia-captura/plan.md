# Plano — Persistência da captura

> Micro-tarefas em ordem. Marque `[x]` ao concluir. Requisitos em [`spec.md`](spec.md) (Rn);
> formatos em spec § Formatos. Cada etapa tem o seu critério de pronto.

## Pré-requisitos

- Branch `persistencia-captura` criada a partir de `develop`.
- Container `isaac-ros-dev`, entrada como `admin`. Build:
  `cd build && cmake .. -DBUILD_TESTS=ON && make -j$(nproc) && ctest`.
- Linha de base em `calib_runs/regress_chess/` (`baseline.yml`, `baseline.log`).
- Para a temperatura (etapa 8): o spec [`ambiente-sensores`](../ambiente-sensores/spec.md)
  aplicado, ou a validação com `null`.

## Etapas

### Fase A — módulo puro `common/capture_io/` (TDD, sem câmera)

- [x] 1. **Leitura da sessão** (R2). Feito em `common/capture_io/session.{hpp,cpp}`, testes em
  `tests/test_session.cpp` (17 casos). Duas limitações do `cv::FileStorage` 4.5.4 apareceram e
  foram tratadas:
  - um valor que começa por dígito e não é número (`2.1mm`, `2026-11-03`) quebra a leitura, então
    precisa de aspas; a mensagem de erro diz isso;
  - um comentário no fim de um texto sem aspas vira parte do texto; os comentários são removidos
    antes da leitura, respeitando as aspas.

  Testes rodados com `cmake -S tests -B build/unit_tests && make -C build/unit_tests &&
  build/unit_tests/aprilgrid_tests`: 80/80.

  Especificação original da etapa: `loadSession(path, Session&, error)` lê o YAML (com ou sem o
  cabeçalho `%YAML`) e valida:
  - campos obrigatórios presentes;
  - `condition` em `[A-Za-z0-9_-]+`;
  - `capture` ≥ 1;
  - `medium.type` em `air | fresh_water | sea_water`.

  Campos de porta e meio podem ser `null`.
  - Testes primeiro: o arquivo de exemplo é aceito; cada campo obrigatório removido é recusado com
    mensagem; uma condição com espaço é recusada; um `null` é preservado.
  - **Pronto quando:** `ctest` verde, com os casos acima.
- [x] 2. **Caminhos e recusa de sobrescrever** (R3–R5). Feito em
  `common/capture_io/capture_dir.{hpp,cpp}`, testes em `tests/test_capture_dir.cpp` (8 casos;
  88/88 no total). A pasta é recusada também quando só tem `left/` vazio, e quando no lugar dela há
  um arquivo. O `_` fica reservado como separador em `<pose_id>_<n>.png`, então os ids de pose
  aceitam só letras, dígitos e `-`, o que cobre todos os ids do roteiro.

  Especificação original da etapa:
  - `captureDir(session)` devolve `<raiz>/<condicao>/captura_<XX>`.
  - `prepareCaptureDir(dir, error)` cria `left/` e `right/`, ou recusa se a pasta existe e não está
    vazia.
  - `imagePath(dir, side, pose_id, n)` monta o caminho de cada imagem.
  - Testes primeiro: XX com dois dígitos (1 → `01`, 12 → `12`); uma pasta vazia existente é
    aceita; uma pasta com um arquivo é recusada e o arquivo continua lá; os nomes saem
    `left/free_0.png`.
  - **Pronto quando:** `ctest` verde; nenhum teste apaga arquivo.
- [x] 3. **Escrita de `detections.json`** (R6, R7). Feito em
  `common/capture_io/detections_io.{hpp,cpp}` (escrita e leitura), testes em
  `tests/test_detections_io.cpp` (8 casos; 96/96 no total). O `json.load` do Python lê o arquivo,
  com os cantos preservados até 1e-9 px. A lista de parâmetros do detector vem de
  `detectorParameterList()`, extraída de `loadDetectorParameters` para que a leitura e o registro
  usem os mesmos campos. A gravação passa por `<arquivo>.tmp` e um `rename`, então o arquivo em
  disco está sempre completo, mesmo regravado a cada par.

  Especificação original da etapa: `DetectionsWriter` acumula quadros e grava com
  `cv::FileStorage` em JSON, no formato de spec § Formatos.
  - Testes primeiro: um quadro com 2 tags à esquerda e 1 à direita é gravado e relido pelo próprio
    `FileStorage`, com os mesmos IDs, a ordem dos cantos e a precisão ≥ 9 dígitos.
  - **Pronto quando:** `ctest` verde, e `python3 -c "import json; json.load(open(...))"` lê o
    arquivo gerado pelo teste.
- [x] 4. **Escrita de `meta.json`** (R8, R9). Feito em `common/capture_io/meta_io.{hpp,cpp}`,
  testes em `tests/test_meta_io.cpp` (10 casos; 106/106 no total). O `json.load` do Python lê o
  arquivo.
  - O `cv::FileStorage` não escreve `null`, `true` nem `false` em JSON. A escrita usa marcadores de
    texto (`__ZEDCAL_*`) e os troca pelos literais no fim. Um texto do usuário com esse prefixo é
    recusado, para não virar `null` sem querer.
  - `timestamp_ns` vai como texto (19 dígitos não cabem num `double`). O spec § Formatos foi
    ajustado.
  - Também ficaram aqui `utcNow()` e `readHostUptime()`, que a etapa 8 usa.

  Especificação original da etapa: `MetaWriter`, com os campos fixos e uma lista de
  quadros. Campo ausente é gravado como `null`.
  - Testes primeiro: com as temperaturas ausentes, o JSON tem `null` nos três campos; a sessão
    aparece copiada.
  - **Pronto quando:** `ctest` verde e `json.load` lê o arquivo.
- [x] 5. **Commit no build** (condições de implementação). Feito em `cmake/BuildInfo.cmake`
  (`zedcal_add_build_info(<alvo>)`) e `cmake/write_build_info.cmake`. Um alvo customizado regrava
  `build_info.hpp` a cada build, mas só quando o conteúdo muda, e define `ZEDCAL_GIT_COMMIT` e
  `ZEDCAL_GIT_DIRTY`. A flag "suja" ignora arquivos não versionados. Os testes em
  `tests/test_build_info.cpp` conferem que o commit gravado é o `HEAD` e que a flag bate com o
  `git status` (108/108). O commit desta etapa, seguido de um novo build, mudou o SHA gravado e
  zerou a flag, como esperado. A ferramenta de calibração passa a usar o módulo na etapa 6.

  Especificação original da etapa: O CMake gera `build_info.hpp` a cada
  build, com o SHA e a flag de árvore suja.
  - **Pronto quando:** dois builds seguidos, com um commit vazio entre eles, mostram SHAs
    diferentes no binário. Conferir com `strings` ou com a impressão do `--help`.

### Fase B — integração no `zed_stereo_calibration`

- [x] 6. **Flag `--session`** (R1–R3). No `stereo_calibration/src/main.cpp`, a sessão é lida e a
  pasta é preparada antes de abrir a câmera. O módulo `capture_io` e o commit no build entraram no
  CMake da ferramenta. Verificado com a ferramenta compilada:
  - sessão sem `capture`, `--session` com `--use_stored_values`, `--session` sem `--target_yaml` e
    pasta já usada saem com código 1, sem abrir a câmera; a pasta usada fica intacta;
  - o `--help` documenta a flag;
  - a regressão do xadrez é idêntica ao `baseline.yml`, com o log sem diferença.

  Especificação original da etapa: Parse no `Args`. A sessão é carregada e a pasta preparada
  **antes de abrir a câmera**. Se a flag não for passada, o caminho antigo não muda.
  - **Pronto quando:**
    - a regressão do xadrez é idêntica ao `baseline.yml` (`diff` e `cmp`);
    - uma sessão inválida e uma pasta já usada saem com código 1, sem abrir a câmera;
    - o `--help` documenta a flag.
- [x] 7. **Gravação dos pares e das detecções** (R4–R7). No modo sessão:
  - os pares vão para `left/` e `right/` da captura, sem apagar nem criar `zed-images/`;
  - os PNGs são relidos, as tags de cada câmera são detectadas de novo com os cantos antes e depois
    do refino (`detectAprilGrid` ganhou o parâmetro opcional `unrefined`), e o `detections.json` é
    regravado a cada par;
  - o `session.yaml` é copiado;
  - o terminal vai para o `capture.log` desde a primeira linha. O `LogTee` guarda em memória o que
    sai antes de a pasta existir;
  - a calibração lê os pares da captura e grava o YAML na pasta dela.

  Partes puras em `common/capture_io/capture_support.{hpp,cpp}`, testadas em
  `tests/test_capture_support.cpp` (113/113). A regressão do xadrez continua idêntica.

  **Bancada (2026-11-03),** com a ZED 2i S/N 35180697, alvo parado, display virtual e
  `calib_runs/tools/bench_session_capture.sh`. A captura `testes_persistencia/teste_etapa7/captura_02`
  salvou 1 par com 25 + 26 tags:
  - 7 tags vistas por uma câmera só ficaram registradas;
  - `calib_runs/tools/check_detections` detectou os PNGs de novo, com diferença máxima de 0 px nos
    dois conjuntos de cantos;
  - o `capture.log` é idêntico à saída do terminal;
  - o `json.load` do Python lê o arquivo.

  Com a cena parada, só 1 par passa no filtro de dissimilaridade; os outros são recusados como
  "parecidos demais". O critério original de 3 pares fica para uma captura com o alvo em
  movimento: o usuário escolheu verificar com o alvo parado.

  Especificação original da etapa: No modo sessão:
  - o par vai para `left/` e `right/`;
  - o PNG é relido e detectado de novo por câmera (todas as tags), e o resultado vai para o
    `DetectionsWriter`;
  - `session.yaml` é copiado;
  - o terminal é duplicado em `capture.log`.
  - **Pronto quando:** numa captura de bancada com 3 pares, `detections.json` tem 3 quadros, inclui
    tags vistas por uma câmera só, e os cantos batem (|Δ| < 1e-6 px) com uma detecção independente
    do PNG pelo C++.
- [x] 8. **Metadados ao vivo** (R8, R9). No modo sessão, o `meta.json` é preenchido ao abrir a
  câmera e regravado a cada par e no fim da coleta, também quando ela é abortada.
  - Câmera: modelo, serial, firmware, resolução, fps e lente.
  - Versão do SDK, commit e flag de árvore suja (de `build_info.hpp`), e o host.
  - A autocalibração desligada e a sessão copiada.
  - Os controles de captura no início e no fim, o horário UTC e o motivo do fim.
  - Por par: timestamp da imagem (do `grab`), uptime do host, tempo desde a abertura e as
    temperaturas `ONBOARD_LEFT/RIGHT` e `IMU`, lidas com `getSensorsData`.

  **Bancada (2026-11-03),** `testes_persistencia/teste_etapa8/captura_01`: todos os campos
  preenchidos (SDK 5.1.0, firmware 1523, temperaturas 29,4 / 30,2 / 30,4 °C, controles iguais no
  início e no fim, motivo "aborted by the user").
  - O HID estava acessível nesta sessão, por uma correção manual anterior. Por isso o caminho sem
    permissão (temperatura `null` e um aviso único) **não foi exercitado na bancada**. A escrita
    de `null` está coberta por `meta_writes_nulls_for_missing_temperatures`. Ele será exercitado
    quando o `ambiente-sensores` for validado.

  Especificação original da etapa: Timestamp do SDK, uptime de `/proc/uptime`, tempo desde a
  abertura, temperaturas com `getSensorsData`, controles de captura no início e no fim, motivo do
  encerramento.
  - **Pronto quando:** numa captura de bancada todos os campos estão preenchidos. Com o HID sem
    permissão, as temperaturas saem `null` e o aviso aparece uma única vez no `capture.log`.

### Fase C — capturas antigas e documentação

- [x] 9. **`--use_stored_values --detections_out`** (R10). A flag grava o `detections.json` das
  imagens de `zed-images/` (`pose_id = legacy`). Ela recusa um arquivo existente e só funciona com
  `--use_stored_values`.
  - A detecção do xadrez saiu de `calibrate()` para `detectCheckerboard()`, compartilhada com a
    exportação, para que os dois usem os mesmos cantos. A regressão do xadrez continua idêntica.
  - Os tamanhos do alvo, que são `float` nas ferramentas, vão para o JSON como o decimal escrito
    (23,8, não 23,7999992).

  **Verificado (2026-11-03),** numa cópia de trabalho com link para as imagens originais
  (`testes_persistencia/legacy_v2`):
  - **`2026-10-20_ar_aprilgrid_v2`:** 29 pares exportados; a calibração é idêntica com e sem a
    flag; a segunda execução é recusada; o md5 de todos os arquivos da pasta original não mudou;
    `check_detections` deu 0 px de diferença; o Python lê o arquivo.
  - **Achado:** 281 das 1 511 tags (18,6 %) foram vistas por uma câmera só e são descartadas pela
    calibração atual. É o risco 4 da auditoria, agora medido.
  - **Xadrez `regress_chess`:** 35 pares, todos com 54 + 54 cantos, e o YAML idêntico ao
    `baseline.yml`.

  Especificação original da etapa:
  - **Pronto quando:**
    - o arquivo de `2026-10-20_ar_aprilgrid_v2` é gerado e o Python o lê;
    - a calibração impressa é idêntica à do recálculo sem a flag;
    - a regressão do xadrez continua idêntica.
- [ ] 10. **Documentação.** `config/session_example.yaml` comentado; seção no `README.md`;
  `.ai/docs/bench.md` atualizado (sessão, nada apagado).
  - **Pronto quando:** um usuário segue o README e grava uma captura de teste sem consultar o
    código.

## Riscos

- **O `cv::FileStorage` em JSON** pode escrever algo que o `json` do Python recuse (números
  especiais, chaves). A etapa 3 testa a leitura em Python logo no início. Plano B: um escritor
  JSON mínimo próprio.
- **Reler o PNG e detectar de novo custa tempo** a cada amostra. São ~2 detecções a 1280×720,
  aceitável na coleta manual; medir na etapa 7.
- **`getSensorsData` pode bloquear ou falhar** sem o HID. Tratar como indisponível, sem abortar.
