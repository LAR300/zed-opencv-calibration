# Plano — Alvo AprilGrid nas ferramentas estéreo

> **Como implementar**, em micro-tarefas verificáveis. Marque `[x]` ao concluir. O porquê das
> escolhas está em [`decisions.md`](decisions.md) (Dn); os requisitos em [`spec.md`](spec.md) (Rn).

## Pré-requisitos

- Container `isaac-ros-dev` de pé (modo GUI), entrada sempre como `admin`.
  `RUN_DIR=/workspaces/isaac_ros-dev/dev/calib_runs`, `REPO=/workspaces/isaac_ros-dev/dev/zed-opencv-calibration`.
- Build: `cd $REPO/build && cmake .. [-DBUILD_TESTS=ON] && make -j$(nproc)`.
- Imagens da linha de base de xadrez em `$RUN_DIR/2026-10-16_ar_9x6/zed-images/` (35 pares).
- `target.yaml` do alvo. **Antes da etapa 12**: `tagSize`/`tagSpacing` conferidos com régua na
  placa (pendência da spec).
- ZED 2i S/N 35180697 conectada (etapas 8, 11, 12).

## Etapas

### Fase A — linha de base e estrutura

- [ ] 1. **Linha de base da regressão.** Antes de qualquer mudança, em `$RUN_DIR/regress_chess/`
  (cópia de `zed-images/`): `zed_stereo_calibration --use_stored_values --square_size 23.8 | tee
  baseline.log`.
  - **Verificação:** `baseline.log` traz RMS 0,237077 / 0,238066 / 0,253679 e T_x −120,04 —
    iguais ao `results.md`. Guardar `zed_calibration_0.yml` como `baseline.yml`.
- [ ] 2. **Esqueleto de testes** (D10): `option(BUILD_TESTS OFF)` no `CMakeLists.txt` raiz,
  `tests/CMakeLists.txt` com executável `aprilgrid_tests` + `add_test`, macro `CHECK` mínima, um
  teste trivial.
  - **Verificação:** `cmake .. -DBUILD_TESTS=ON && make && ctest --output-on-failure` passa; build
    sem a opção compila exatamente os 5 alvos de antes.

### Fase B — módulo `common/aprilgrid` (TDD, sem câmera)

- [ ] 3. **Leitura do `target.yaml`** (D3, D4; R2). `struct AprilGridTarget {rows, cols, tag_size_mm,
  spacing_ratio}` + `bool loadKalibrTarget(path, AprilGridTarget&, std::string& error)`.
  - Testes primeiro: YAML válido do Kalibr (com comentários) → 6/6/88,0/0,3; arquivo inexistente;
    campo ausente; `target_type: 'checkerboard'`; `tagCols: 0`; `tagSize: 88` (guarda de unidade);
    `tagSpacing: 0`. Cada erro com mensagem específica.
  - **Verificação:** `ctest` verde.
- [ ] 4. **Geometria dos pontos 3D** (D5). `tagCorners3D(id)` → 4 `cv::Point3f` em mm na convenção
  Kalibr (IDs esquerda→direita, baixo→cima; cantos BR, BL, TL, TR; Y para cima); `rowOf(id)`,
  `colOf(id)`; dimensões físicas (pitch 114,4 mm; lado 660,0 mm).
  - Testes primeiro: cantos das tags 0, 5, 30 e 35; pitch; lado total; IDs fora do grid rejeitados.
  - **Verificação:** `ctest` verde.
- [ ] 5. **Parâmetros do detector** (D9; R6). Padrões da referência; `--detector_params` lido por
  `readDetectorParameters`; `markerBorderBits` forçado a 2 com aviso.
  - Testes primeiro: padrões; arquivo que altera `cornerRefinementMethod`; arquivo com
    `markerBorderBits: 1` → fica 2 + aviso; arquivo inválido → erro.
  - **Verificação:** `ctest` verde.
- [ ] 6. **Detecção** (D7; R4, R5). `detectAprilGrid(gray, target, params)` → mapa
  `id → 4 cantos 2D` só com IDs do grid e lado ≥ 10 px.
  - Teste primeiro: imagem sintética com as 36 tags desenhadas por `cv::aruco::drawMarker`
    (borda 2) na nossa convenção, com escala e rotação conhecidas → 36 IDs, cada canto a < 0,5 px
    do previsto; imagem com metade do alvo cortada → só as tags inteiras; tag de ID 40 colada →
    ignorada.
  - **Verificação:** `ctest` verde.
- [ ] 7. **Cruzamento, regra do mínimo e cantos virtuais** (D6, D8; R7, R9, R11).
  - `matchCommonTags(det_l, det_r)` → `obj`, `img_l`, `img_r` alinhados, ordem por ID.
  - `meetsMinimum(ids, target, min_tags, min_rows, min_cols)` + motivo da recusa.
  - `outsideCorners(det, target)` → 4 cantos (UL, UR, DR, DL na imagem) do retângulo envolvente
    das tags detectadas, por homografia.
  - Testes primeiro: interseção de conjuntos parciais; 12 tags em 2×6 aceitas; 11 tags recusadas;
    12 tags numa só linha recusadas; cantos virtuais de um alvo sintético inteiro batem com os
    cantos externos a < 1 px; alvo parcial dá o retângulo da parte vista.
  - **Verificação:** `ctest` verde.

### Fase C — checker (portão da convenção Kalibr)

- [ ] 8. **`zed_stereo_checker` com AprilGrid** (D5, D8; R1–R3, R12, R14, R15).
  `CMakeLists.txt` inclui `common/aprilgrid`; flags `--target_yaml`, `--detector_params`,
  `--min_tags`, `--min_rows`, `--min_cols` no `Args::parse` e no `--help`; impressão do alvo no
  lugar de "Expected checkerboard features"; laço com detecção em resolução cheia, Left/Right por
  câmera (todas as tags), Stereo pela pose da esquerda; desenho com `cv::aruco::drawDetectedMarkers`
  (IDs visíveis) escalado para a tela. Caminho de xadrez sem alteração.
  - **Verificação (portão — não seguir se falhar):** com a calibração interna (de fábrica) e
    `target.yaml` do alvo, em ≥ 5 poses incluindo ≥ 2 com o alvo parcialmente fora do quadro:
    Left e Right < 0,5 px e Stereo < 0,5 px; IDs desenhados batem com a placa; com < 12 tags o
    erro não é exibido. Xadrez: rodar o checker com `--square_size 23.8` e conferir a mesma faixa
    de antes (0,25–0,51 px). Se Left/Right ficarem altos, diagnosticar a convenção (verbose com
    erro por tag) antes de prosseguir.

### Fase D — calibração

- [ ] 9. **Métricas a partir de 4 cantos** (D6). Extrair de
  `CalibrationChecker::getDetectedBoardParams` a parte que recebe os 4 cantos externos;
  `testSample(corners, image_size)` do xadrez passa a chamá-la com `get_outside_corners`; nova
  sobrecarga `testSample(corners, outside_corners, image_size)` para o AprilGrid.
  - **Verificação:** etapa 1 de novo → `diff` do relatório com `baseline.log` vazio e
    `zed_calibration_0.yml` idêntico a `baseline.yml`.
- [ ] 10. **`calibrate()` com AprilGrid** (D7, D8; R9, R13). Parâmetro opcional com o alvo
  (ausente = xadrez); no laço de detecção, ramo AprilGrid: detectar L e R, `matchCommonTags`,
  `meetsMinimum`, montar `object_points`/`pts_l`/`pts_r`; frame recusado reporta o motivo. Resto
  (mono, rejeição de outliers, estéreo, relatório, `.yml`) intocado.
  - **Verificação:** etapa 1 de novo (regressão idêntica). Ramo AprilGrid verificado na etapa 12.
- [ ] 11. **`zed_stereo_calibration` ao vivo com AprilGrid** (R1–R3, R7, R8, R10–R12, R15).
  Flags e `--help` como na etapa 8; impressão do alvo; detecção em resolução cheia; aceitação pelas
  tags comuns + mínimo, com nova mensagem de recusa (motivo + nº de tags) no padrão das existentes;
  `checker.testSample` com os cantos virtuais; `addNewCheckerboardPoly` com os cantos virtuais;
  desenho com IDs; `calibrate()` recebe o alvo; `--use_stored_values` repassa o alvo.
  - **Verificação (câmera):** amostra com < 12 tags ou uma linha só → recusada com a mensagem;
    amostra com o alvo parcialmente fora do quadro → aceita; métricas X/Y/Size/Skew sobem e a
    coleta termina; fps da janela anotado (risco de D7). Xadrez: uma captura curta continua
    funcionando como antes.

### Fase E — validação e acabamento

- [ ] 12. **Validação no ar** (critérios de aceitação). Em `$RUN_DIR/<data>_ar_aprilgrid/`, com o
  `target.yaml` medido: calibração completa ao vivo; depois `--use_stored_values` sobre as mesmas
  imagens em subpasta; depois checker com `--calib_opencv` do resultado e com a calibração interna.
  - **Verificação:** RMS L/R/Stereo < 0,5 px; fx, fy (L e R) ≤ 0,5 % do xadrez (`baseline.yml`) **e**
    da fábrica (`SN35180697.conf`); baseline ≤ 0,5 % de 120,04 e de 119,98 mm; cx/cy reportados;
    ≥ 1 amostra aceita com alvo parcial; recálculo reproduz o resultado ao vivo; checker com a
    fábrica dá Stereo < 0,5 px. Registrar a tabela de comparação no `results.md` (fora do controle
    de versão), em nova seção.
- [ ] 13. **Erros de entrada pela CLI** (R2). `--target_yaml` inexistente, sem campo, com
  `target_type: 'checkerboard'`, com `tagSize: 88`, nas duas ferramentas.
  - **Verificação:** mensagem clara e `echo $?` ≠ 0, sem abrir a câmera.
- [ ] 14. **`--fisheye` e `--virtual` com `--target_yaml`** (R15).
  - **Verificação:** o parse aceita as combinações, imprime o alvo e o modo e segue até a abertura
    da câmera (falha esperada em `--virtual` sem ZED X One, com a mensagem atual do SDK).
- [ ] 15. **README** (R17). Seção "AprilGrid target" nas duas ferramentas: formato do
  `target.yaml`, exemplos, mínimo de tags e flags, `--detector_params`, recomendação de medir o
  alvo impresso; nota sobre `common/` e `BUILD_TESTS`.
  - **Verificação:** os exemplos do README rodam como escritos (copiar e colar no container).
- [ ] 16. **Fechamento.** Build limpo (`rm -rf build`) das 5 ferramentas com e sem `BUILD_TESTS`;
  `ctest` verde; regressão da etapa 1 idêntica; revisão dos critérios de aceitação da spec um a um.
  - **Verificação:** todos os itens de "Critérios de aceitação" da spec marcados.

## Riscos

| Risco | Efeito | Mitigação |
|---|---|---|
| Convenção de IDs/cantos do Kalibr diferente da referência | calibração errada silenciosa | portão da etapa 8 com intrínsecos de fábrica (D5); verbose com erro por tag para diagnosticar |
| Cantos de tag menos precisos que os do xadrez | RMS > 0,5 px e a ferramenta recusa o resultado | trocar `cornerRefinementMethod` (SUBPIX ↔ APRILTAG) e janela via `--detector_params` (D9), sem recompilar |
| Detecção em resolução cheia lenta | interface travando, coleta penosa | medir fps na etapa 11; se necessário detectar a cada N quadros só para exibição (D7) |
| Alvo de 66 cm limita poses próximas e a métrica Size | coleta não chega a 100 % | vista parcial permite aproximar; se não bastar, a coleta termina pelo máximo de amostras como hoje |
| `tagSize` impresso ≠ nominal | baseline errado com RMS bom | pendência da spec bloqueia a etapa 12; impressão das dimensões ao iniciar (R3); critério do baseline contra xadrez **e** fábrica |
| Regressão no xadrez por mexer em `CalibrationChecker` e `calibrate()` | resultados do xadrez mudam | etapa 1 repetida após 9, 10 e 16 com `diff` do relatório e do `.yml` |
| OpenCV com API de `aruco` diferente (4.7+ mudou para `ArucoDetector`) | não compila em outra imagem | alvo é a 4.5.4 do container (amd64 e arm64 usam o mesmo pacote); documentar a versão no README |
