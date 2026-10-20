# Decisões — Alvo AprilGrid nas ferramentas estéreo

> **Por que as escolhas técnicas foram feitas.** Estilo ADR leve. Durável: quando uma decisão
> mudar, registre a nova com o motivo.

### D1. Módulo AprilGrid compartilhado e só com OpenCV

- **Contexto:** `zed_stereo_calibration` e `zed_stereo_checker` precisam da mesma leitura de
  `target.yaml`, da mesma detecção e do mesmo mapeamento ID → posição 3D. O repositório tem o
  costume de duplicar código entre ferramentas: `calibration_checker.cpp` e `opencv_calibration.cpp`
  existem em cópia, e o checker tem a própria `StereoCalibData`.
- **Decisão:** criar `common/aprilgrid/` (um `.hpp` e um `.cpp`) incluído pelos `CMakeLists.txt` das
  duas ferramentas estéreo. O módulo depende **só de OpenCV**, sem `sl::`.
- **Por quê:** o mapeamento do Kalibr é exatamente o tipo de detalhe que dá calibração errada sem
  aviso. Duas cópias podem divergir sem ninguém perceber. Sem `sl::`, o módulo pode ser testado sem
  câmera e sem ZED SDK (D10).
  - *Descartado:* duplicar o código nas duas ferramentas, como o repositório costuma fazer. Isso
    deixa dois lugares para manter a convenção do Kalibr.
  - *Descartado:* criar uma biblioteca CMake formal. É mais estrutura do que o repositório usa hoje.
- **Consequências:** as ferramentas mono continuam com as cópias atuais, sem acesso ao módulo, o que
  está fora do escopo. A pasta `common/` é um padrão novo no repositório e precisa ser documentada
  no README.

### D2. O caminho do xadrez fica intocado: o AprilGrid entra como desvio nos pontos de chamada

- **Contexto:** o requisito é zero regressão no xadrez (R1), e o repositório não tem testes.
- **Decisão:** **não** abstrair o xadrez atrás de uma interface comum. Nos pontos onde há
  `findChessboardCorners`, entra um `if` para o AprilGrid. A única refatoração no código existente
  é extrair o cálculo das métricas a partir de 4 cantos externos (D6), sem mudar o resultado.
- **Por quê:** mexer em menos linhas do xadrez reduz o risco. A regressão é verificada recalculando
  as imagens de 16/10.
  - *Descartado:* uma interface `Target` com duas implementações. É mais limpo, mas mexe em todo o
    caminho do xadrez sem cobertura de teste.
- **Consequências:** há algum `if` repetido em `main.cpp` das duas ferramentas. É aceitável para
  duas ferramentas.

### D3. Ler o `target.yaml` com `cv::FileStorage` em memória, com cabeçalho prefixado

- **Contexto:** o `target.yaml` do Kalibr não tem a diretiva `%YAML:1.0`. Testado no container:
  `cv::FileStorage` (OpenCV 4.5.4) **recusa** o arquivo direto com `Input file is invalid`, mas lê
  corretamente quando o conteúdo é prefixado com `"%YAML:1.0\n---\n"` e aberto com
  `FileStorage::MEMORY`. Os comentários `#` e as aspas simples também são aceitos.
- **Decisão:** ler o arquivo para uma string, prefixar o cabeçalho e abrir em memória.
- **Por quê:** não traz dependência nova (R16) e usa o mesmo leitor YAML que o repositório já usa
  para os `.yml` de calibração.
  - *Descartado:* `yaml-cpp`, que existe na imagem, mas é uma dependência nova.
  - *Descartado:* um parser próprio de `chave: valor`, que seria frágil.
- **Consequências:** um YAML com estrutura que o `FileStorage` não aceite falha, e a falha vira o
  erro claro exigido em R2.

### D4. Milímetros por dentro e guarda contra a unidade errada

- **Contexto:** o Kalibr usa metros (`tagSize: 0.088`). Toda a ferramenta usa mm: `square_size`,
  `T` e `MIN_BASELINE = 30`. Um erro de escala já aconteceu com o xadrez e o RMS não denunciou.
- **Decisão:** converter para mm na leitura. Se `tagSize` não estiver em `(0, 1)` m, a ferramenta
  sai com erro, porque 88 mm escrito como `88` seria lido como 88 m. `tagSpacing` precisa ser
  maior que 0.
- **Por quê:** os pontos 3D ficam em mm, então o `.yml` de saída e as checagens de baseline não
  mudam. A guarda pega o erro de unidade mais provável.
- **Consequências:** nenhuma no formato de saída.

### D5. Convenção do Kalibr vinda da referência Python e validada pelo checker

- **Contexto:** a posição física de cada tag depende de três coisas: a borda das tags, a ordem dos
  IDs e a ordem em que o OpenCV devolve os 4 cantos. Se qualquer uma estiver errada, o resultado
  ainda sai, mas está errado. `petro_data_replayer/calibrate_intrinsics.py` traz uma convenção
  "empiricamente verificada contra o board Kalibr real":
  - `markerBorderBits = 2`;
  - IDs da esquerda para a direita e de baixo para cima;
  - cantos `[0]` BR, `[1]` BL, `[2]` TL, `[3]` TR, com Y para cima.
- **Decisão:** adotar essa convenção e **forçar** `markerBorderBits = 2`, mesmo que o arquivo de
  parâmetros (D9) diga outra coisa (com aviso). A convenção é validada **antes** de mexer na
  calibração: o checker AprilGrid, com os intrínsecos de fábrica, já validados com o xadrez, precisa
  dar Left e Right < 0,5 px.
- **Por quê:** com intrínsecos corretos, um mapeamento errado aparece na hora como erro de
  reprojeção alto. Uma tag espelhada ou fora de lugar não se ajusta a nenhuma pose. Isso transforma
  o risco silencioso num teste objetivo.
  - *Descartado:* confiar só na referência Python. Ela foi escrita para o mesmo alvo, mas não foi
    reverificada nesta câmera.
- **Consequências:** a etapa do checker é um **portão** no plano.

### D6. Métricas de cobertura com vista parcial: 4 cantos virtuais por homografia

- **Contexto:** `CalibrationChecker` calcula área, skew, X/Y de borda e o polígono de cobertura a
  partir dos 4 cantos externos do grid completo (`get_outside_corners`). Com vista parcial esses
  cantos não existem.
- **Decisão:**
  - ajustar uma homografia dos pontos 3D (plano XY) para os pontos 2D das tags detectadas;
  - projetar os 4 cantos do **retângulo envolvente, no tabuleiro, das tags detectadas**, obtendo os
    "cantos externos" da região coberta;
  - passar esses 4 pontos às funções existentes (`compute_area`, `compute_skew`, borda), extraindo
    de `getDetectedBoardParams` a parte que recebe os 4 cantos;
  - a posição média continua sendo a média dos pontos detectados.
- **Por quê:**
  - mantém a definição e a interface das métricas (MIN, MAX, COVERAGE, REQUIRED e SCORE) e os
    mesmos limites ideais;
  - o xadrez passa pelo mesmo código com os cantos de sempre;
  - a homografia ignora a distorção, mas as métricas só orientam a coleta, sem entrar no cálculo
    da calibração.
  - *Descartado:* usar a envoltória convexa dos pontos 2D. Ela não dá 4 cantos ordenados para o skew.
  - *Descartado:* exigir a pose por `solvePnP`, que precisa de intrínsecos que ainda não se tem
    durante a coleta.
- **Consequências:** o tamanho de uma amostra parcial mede só a área coberta pelas tags. Isso é o
  que R11 pede.

### D7. Detecção em resolução cheia, ao vivo e no recálculo

- **Contexto:** o xadrez é detectado ao vivo na imagem reduzida (720x404) e de novo na imagem
  cheia, com `cornerSubPix`, dentro de `calibrate()`. Tags de 88 mm a 1–2 m ficam com ~20–45 px em
  1280x720 e com metade disso na imagem reduzida, abaixo do que o detector decodifica bem.
- **Decisão:** detectar as tags na imagem **cheia**, em tons de cinza, tanto ao vivo quanto em
  `calibrate()`. O desenho na tela usa os cantos escalados para a imagem reduzida. `calibrate()`
  detecta de novo nas imagens salvas, como já faz com o xadrez.
- **Por quê:** a captura ao vivo e o `--use_stored_values` usam o mesmo código na mesma imagem, então
  R13 sai de graça.
- **Consequências:** custo de CPU por quadro maior. Isso é medido na etapa da ferramenta ao vivo. Se
  derrubar o fps, a mitigação é detectar a cada N quadros só para exibição.

### D8. Cruzamento de IDs na calibração, mas não no erro estéreo do checker

- **Contexto:** `cv::stereoCalibrate` exige os mesmos pontos 3D nas duas imagens de cada par. O erro
  estéreo do checker usa a pose da esquerda mais R e T para prever os pontos da direita.
- **Decisão:**
  - **Na calibração**, cada par usa só as tags comuns às duas câmeras. A calibração mono que roda
    antes também usa esse conjunto, como hoje, porque o código usa os mesmos vetores.
  - **No checker**, Left e Right usam todas as tags de cada câmera. O Stereo usa a pose estimada
    com as tags da esquerda para projetar as tags vistas pela direita, sem exigir cruzamento.
  - O mínimo de R7 vale para cada câmera em Left e Right, e para as tags comuns no Stereo.
- **Por quê:** é o mínimo que cada cálculo exige, e no checker aproveita mais pontos.
- **Consequências:** as amostras da calibração mono têm menos pontos do que poderiam. Aceito, porque
  separar os conjuntos mudaria o fluxo da calibração além do escopo.

### D9. Ajuste da detecção por arquivo de parâmetros do OpenCV

- **Contexto:** R6 pede ajustar a sensibilidade sem recompilar, pensando no uso submerso. O OpenCV
  4.5.4 tem `cv::aruco::DetectorParameters::readDetectorParameters(FileNode)`.
- **Decisão:** a opção `--detector_params <file.yml>`, no formato do OpenCV, sobrescreve os
  padrões. Os padrões vêm da referência Python, onde foram ajustados para ar e água:
  - `adaptiveThreshWinSizeMin` 5, `adaptiveThreshWinSizeMax` 21, `adaptiveThreshWinSizeStep` 4;
  - `minMarkerPerimeterRate` 0,02;
  - `polygonalApproxAccuracyRate` 0,03;
  - `minCornerDistanceRate` 0,05;
  - `minDistanceToBorder` 3;
  - `minMarkerDistanceRate` 0,05;
  - `CORNER_REFINE_SUBPIX`, com `cornerRefinementWinSize` 5 e 30 iterações.

  Tags com lado menor que 10 px são descartadas.
- **Por quê:** é um formato padrão e documentado, sem parser novo, e cobre também o método de
  refino dos cantos (`SUBPIX` ou `APRILTAG`), a principal alavanca se o RMS ficar alto.
  - *Descartado:* uma flag por parâmetro, que seria uma dúzia de flags.
- **Consequências:** `markerBorderBits` é sempre forçado a 2 (D5).

### D10. Testes automatizados opcionais, sem dependência nova

- **Contexto:** o repositório não tem testes. R16 proíbe dependência nova. O módulo de D1 é OpenCV
  puro e testável sem câmera.
- **Decisão:** criar `tests/`, com um executável de testes em C++ com `assert`/`CHECK` próprio,
  ligado ao CTest. Ele entra no build por `option(BUILD_TESTS ... OFF)` no `CMakeLists.txt` raiz, e
  o build padrão continua igual. Os testes cobrem:
  - leitura e validação do YAML;
  - geometria dos pontos 3D;
  - regra do mínimo;
  - cruzamento de IDs;
  - cantos virtuais;
  - detecção ponta a ponta numa imagem **sintética**, com as tags desenhadas por
    `cv::aruco::drawMarker` com borda de 2 bits.
- **Por quê:** é TDD onde o código é novo e mantido.
  - *Descartado:* GoogleTest, por ser dependência nova.
  - *Descartado:* não ter testes. A geometria e a regra do mínimo erram fácil sem aparecer.
- **Consequências:** a imagem sintética usa a **nossa** convenção, então prova a consistência
  interna, não a do Kalibr. A do Kalibr é provada pelo portão de D5, com o alvo real. O que depende
  de câmera continua com verificação manual.
