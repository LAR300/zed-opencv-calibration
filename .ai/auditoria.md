# Auditoria — `zed-opencv-calibration` contra o brief metodológico

> Leitura do repositório, sem alterações no código. Branch `develop`, commit `66812e0`, em
> 2026-11-03. Caminhos relativos à raiz do repositório; `arquivo:linha` aponta para esse commit.
> Contexto: [`product/vision.md`](product/vision.md), [`product/backlog.md`](product/backlog.md)
> e [`product/referencias.md`](product/referencias.md).
>
> **Escopo:** `stereo_calibration/`, `stereo_checker/`, `common/` e `tests/` foram lidos inteiros.
> Ferramentas que não servem à ZED 2i foram só conferidas: `monocular_*` usa `sl::CameraOne`
> (ZED X One); `stereo_reprojection_viewer` mostra a nuvem de pontos. Também foram consultados
> `calib_runs/` (fora do repositório) e o fonte do OpenCV 4.5.4 (`modules/calib3d/src/calibration.cpp`,
> tag `4.5.4` no GitHub), para a convenção do RMS.

## Resumo

- **A base é boa para o que o brief pede.**
  - Captura de imagens brutas sem perdas.
  - Calibração de fábrica lida raw na mesma resolução.
  - AprilGrid com vista parcial e refino subpixel.
  - Saída aceita pelo ZED SDK.
  - Autocalibração desligada nos caminhos que usam a ZED 2i, exceto o visualizador.
- **Não há persistência para reprocessar.** As detecções e os metadados não são salvos, e
  `zed-images/` é apagado a cada captura.
- **Não há verificação de cobertura no sentido do R3,** nem imobilidade, nem roteiro de poses.
- **Seis pontos podem invalidar resultados** se não forem tratados antes da condição A (§6):
  1. o filtro de "variedade" rejeita poses repetidas do roteiro, como a rotação no plano;
  2. a captura ao vivo parte da calibração de fábrica;
  3. o RMS por câmera no relatório não é o dos intrínsecos salvos;
  4. a calibração de cada câmera usa só as tags vistas pelas duas;
  5. o RMS do OpenCV é por canto, o que importa para o Bias Ratio;
  6. as análises offline existentes refazem a detecção em Python.

---

## 1. Captura e dados

### 1.1 O que é salvo

| Item | Situação | Onde |
|---|---|---|
| Pares de imagens | **Sim.** PNG (sem perdas), BGR, **não retificado** (`LEFT/RIGHT_UNRECTIFIED_BGR`), resolução cheia | `stereo_calibration/src/main.cpp:618-619` (retrieve), `:981-986` (`imwrite`) |
| Nome e pasta | `zed-images/image_{left,right}_<n>.png`, com n sequencial. **Não há id de pose nem de captura** | `main.cpp:31`, `:981-986` |
| Pasta apagada a cada captura ao vivo | **Sim**, `fs::remove_all(zed-images/)` | `main.cpp:563-577` |
| Detecções (IDs e cantos) | **Não são salvas.** A detecção ao vivo só decide se aceita o quadro. A calibração **detecta de novo** a partir dos PNGs | `main.cpp:641-656`; `stereo_calibration/src/opencv_calibration.cpp:86-143` |
| Arquivo de calibração | `zed_calibration_<serial>.yml` no **diretório corrente** | `opencv_calibration.cpp:455-477` |

Ponto positivo: como a calibração sempre detecta a partir do PNG, a imagem salva é a fonte
única. As capturas antigas de `calib_runs/` podem ser reprocessadas, mesmo sem detecções salvas.

### 1.2 Metadados

Nenhum metadado é gravado em arquivo. A ferramenta só **imprime na saída padrão** (registrado
quando a rodada usa `| tee calib.log`):

| Campo pedido | Situação | Onde |
|---|---|---|
| Serial | impresso; também no nome do YAML | `main.cpp:500`; `opencv_calibration.cpp:456-457` |
| Resolução | impressa; `Size` no YAML | `main.cpp:502-504`; `opencv_calibration.cpp:461` |
| Controles de captura | impressos no início e no fim | `main.cpp:518-522`, `:1064` |
| Lente, condição, id da captura, id da pose | **ausentes** | — |
| Timestamp | **ausente** (o SDK oferece `getTimestamp`) | — |
| Temperatura do sensor | **ausente.** O SDK 5.1 tem `SENSOR_LOCATION::ONBOARD_LEFT/RIGHT`, mas a leitura exige acesso ao HID (`.ai/docs/bench.md`) | `/usr/local/zed/include/sl/Camera.hpp:4407-4411` |
| Tempo desde que ligou | **ausente** | — |
| Versão do SDK / firmware | **ausente** | — |
| Commit do código | **ausente** | — |

Com `--use_stored_values` a câmera não é aberta: `zed_info` fica no valor padrão e o arquivo sai
como `zed_calibration_0.yml` (`main.cpp:378`, `:1074-1078`). Por isso
`calib_runs/2026-10-16_ar_9x6/recalib/zed_calibration_0.yml` tem serial 0.

### 1.3 Como um quadro é aceito

O usuário aperta `s` ou espaço (`main.cpp:937`). O quadro corrente passa por estes filtros, em
ordem:

1. **Tags mínimas, nas duas câmeras ao mesmo tempo:** ≥ 12 tags comuns, em ≥ 2 linhas e ≥ 2
   colunas (`common/aprilgrid/aprilgrid.hpp:79-81`; `aprilgrid.cpp:329-351`; uso em `main.cpp:646-651`).
2. **Nitidez:** a variância do Laplaciano da **imagem inteira** precisa ser ≥ 100, nas duas
   câmeras (`main.cpp:78`, `:579-586`, `:963-965`). O cálculo não se restringe à região do alvo:
   um fundo texturizado passa no teste com o alvo borrado.
3. **Área mínima:** o quadrilátero do alvo precisa cobrir ≥ 2 % da imagem
   (`main.cpp:76`; `stereo_calibration/src/calibration_checker.cpp:173-186`).
4. **Dissimilaridade:** a soma |Δsize| + |Δskew| + |Δx| + |Δy| em relação a **cada** amostra
   anterior precisa ser ≥ 0,15. Caso contrário o quadro é rejeitado como "parecido demais"
   (`calibration_checker.cpp:243-271`).

As métricas de cobertura X, Y, Size e Skew são só **critério de parada**, não de aceite
(`calibration_checker.cpp:273-363`; limiares em `main.cpp:63-74`; decisão em
`stereo_calibration/include/sample_limits.hpp:66-73`).
- X e Y medem a faixa de posições do canto do alvo mais próximo da borda (`calibration_checker.cpp:203-232`).
- **Skew é um ângulo na imagem, não uma inclinação 3D.** É o maior desvio de 90° entre os lados
  do quadrilátero projetado, normalizado por 90° (`calibration_checker.cpp:74-119`). Ele não
  distingue inclinação horizontal de vertical e não dá o ângulo do alvo.
- Para o AprilGrid, o quadrilátero é a caixa das tags **detectadas**, projetada por homografia
  (`aprilgrid.cpp:353-381`; decisão D6).

**Imobilidade:** não há checagem. O quadro aceito é o do instante da tecla. Não se compara o
movimento dos cantos entre quadros, e o rolling shutter da ZED 2i não é considerado.

**Grade de densidade e histograma de inclinação (R3):** não existem.

### 1.4 Resolução, fps e calibração de fábrica

- **Abertura da câmera:** `RESOLUTION::AUTO` a 15 fps, profundidade desligada
  (`main.cpp:390-398`). Na ZED 2i, AUTO dá **HD720** (log em
  `calib_runs/2026-11-03_camera_settings/calib.log`: "1280 x 720"). É a resolução do SLAM. O valor
  não é configurável por linha de comando.
- **A fábrica é lida raw:** `calibration_parameters_raw` (`main.cpp:532`;
  `stereo_checker/src/main.cpp:406`). Vem de `getCameraInformation()` sem argumento, ou seja, na
  resolução aberta: **a mesma da calibração.**
- **Conversão:** K, D (8 coeficientes: k1, k2, p1, p2, k3, k4, k5, k6), T com Tx negado, e R pelo
  vetor de rotação (`stereo_calibration/include/opencv_calibration.hpp:37-62`, `:141-156`).
- **Na captura ao vivo, a fábrica é o chute inicial da calibração** (`main.cpp:526-532`;
  `opencv_calibration.cpp:159`, `CALIB_USE_INTRINSIC_GUESS`). Com `--use_stored_values`, a
  calibração parte do zero (`main.cpp:377`). Ver o risco 2 em §6.
- **Não existe exportação da fábrica** para o YAML OpenCV como arquivo próprio ("F", R5).

### 1.5 `camera_disable_self_calib` em cada caminho

| Caminho | Câmera | Self-calib desligada? | Onde |
|---|---|---|---|
| `zed_stereo_calibration`, ao vivo | ZED 2i | **sim** | `stereo_calibration/src/main.cpp:397` |
| `zed_stereo_calibration --svo` | — | **a flag não faz nada**: é lida (`:123-124`), mas nunca chega a `init_params`. A ferramenta abre a câmera ao vivo | `main.cpp:102`, `:123-124` |
| `zed_stereo_calibration --use_stored_values` | nenhuma | não se aplica | — |
| `zed_stereo_checker`, ao vivo e com `--svo` | ZED 2i | **sim** (o SVO é aplicado em `:330-331`) | `stereo_checker/src/main.cpp:327` |
| `stereo_reprojection_viewer` | ZED 2i | **não**: `InitParameters` sem a opção, ou seja, autocalibração ativa | `stereo_reprojection_viewer/src/main.cpp:308-338` |
| `zed_monocular_calibration` / `_checker` | `sl::CameraOne` (ZED X One) | não se aplica à ZED 2i | `monocular_calibration/src/main.cpp:190-195` |

A afirmação do brief ("já desabilitada no código") vale para calibração e checker. **Não vale
para o visualizador** nem para um futuro caminho de SVO na calibração.

---

## 2. Detecção AprilGrid

| Item | Valor | Onde |
|---|---|---|
| Biblioteca | OpenCV 4.5.4 `cv::aruco::detectMarkers` | `common/aprilgrid/aprilgrid.cpp:248-253` |
| Família | `DICT_APRILTAG_36h11`, borda de 2 bits (Kalibr) | `aprilgrid.cpp:121`, `:139`, `:248-249` |
| Tag e espaçamento | Lidos do `target.yaml` do Kalibr (`tagSize` em m → mm; `tagSpacing` como razão). Guarda contra a unidade errada (0 < tagSize < 1). Alvo atual: 88 mm, razão 0,3 (26,4 mm) | `aprilgrid.cpp:9-96`; `calib_runs/2026-11-03_camera_settings/calib.log` |
| Parâmetros do detector | Padrão próprio, com `adaptiveThreshConstant = 25` e `CORNER_REFINE_NONE`. Pode ser sobrescrito por `--detector_params` (YAML, campos do OpenCV) | `aprilgrid.cpp:123-141`, `:143-237` |
| Refino subpixel | **Sim**: `cornerSubPix` no cruzamento em X com os quadradinhos do Kalibr; meia janela de 0,15 × o menor lado da tag; 40 iterações; eps 0,001 (decisão D11) | `aprilgrid.cpp:271-278` |
| Filtros | Tags com ID fora do grid, IDs duplicados (reflexo) e tags com lado < 10 px são descartados | `aprilgrid.cpp:256-269`; `aprilgrid.hpp:67` |
| Detecção parcial | **Aceita.** Qualquer subconjunto de tags completas serve. Teste: `tests/test_detection.cpp:123` | `aprilgrid.cpp:239-283` |
| IDs → pontos 3D | Convenção do Kalibr (D5): IDs da esquerda para a direita e de baixo para cima; origem no canto externo da tag 0; X para a direita, Y para cima, em mm; passo = tag × (1 + spacing). Cantos na ordem do OpenCV: BR, BL, TL, TR | `aprilgrid.cpp:98-119`; `aprilgrid.hpp:37-47` |
| Pontos usados na calibração | **Só as tags vistas pelas duas câmeras**, tanto no mono quanto no estéreo (D8) | `aprilgrid.cpp:303-327`; uso em `opencv_calibration.cpp:92-112` |

O detector não depende do modelo de câmera, então serve igualmente na água.

---

## 3. Calibração

### 3.1 Sequência e flags (modelo radial-tangencial, o padrão)

1. **Leitura** dos PNGs em tons de cinza (`IMREAD_GRAYSCALE`) e detecção de novo
   (`opencv_calibration.cpp:38-143`). São necessários ≥ 20 pares válidos (`MIN_IMAGE`,
   `opencv_calibration.hpp:11`; `opencv_calibration.cpp:147-153`).
2. **Calibração mono, esquerda e direita:** `cv::calibrateCamera`.
   - Flags `CALIB_RATIONAL_MODEL`, mais `CALIB_USE_INTRINSIC_GUESS` se houver prior de fábrica
     (`opencv_calibration.hpp:79-99`; `opencv_calibration.cpp:159`, `:169-176`).
   - **Nenhuma** flag `FIX_*`: aspecto, ponto principal, k-coeficientes e tangenciais ficam livres.
3. **Primeira rejeição de outliers** por quadro: `solvePnP` (ITERATIVE, padrão) com os mono, e
   depois refaz as calibrações mono com o chute inicial (`opencv_calibration.cpp:178-258`).
4. **Estéreo:** `cv::stereoCalibrate` com `CALIB_USE_INTRINSIC_GUESS | CALIB_RATIONAL_MODEL`
   (`opencv_calibration.cpp:264-267`; `opencv_calibration.hpp:160-188`). **Os intrínsecos são
   otimizados junto com R e T.** Não há `CALIB_FIX_INTRINSIC`. A função escreve K e D por cima,
   no próprio objeto.
5. **Segunda rejeição de outliers,** com a pose da esquerda e R, T para a direita. Se remover
   quadros, refaz mono e depois estéreo (`opencv_calibration.cpp:271-349`).
6. **Aceite:** RMS esquerda, direita e estéreo ≤ 0,5 px (`main.cpp:59`;
   `opencv_calibration.cpp:363-375`); Tx < 0 (`:387-396`); |Tx| ≥ 30 mm (`:398-410`).

**Com `--fisheye`:**
- mono com `fisheye::calibrate` e as flags `RECOMPUTE_EXTRINSIC | FIX_SKEW`
  (`opencv_calibration.hpp:105-108`);
- estéreo com `fisheye::stereoCalibrate` e `CALIB_CHECK_COND` (`:195-198`);
- sem rejeição de outliers.

### 3.2 Modelo de distorção

**Racional com 8 coeficientes** (k1–k6, p1, p2), o mesmo da fábrica (`opencv_calibration.hpp:28-30`,
`:85-86`). Fisheye de 4 coeficientes só com `--fisheye`. Não há modelo de 5 coeficientes nem
thin-prism, e a escolha não é configurável além dessas duas opções.

### 3.3 Versões Extended

**Não são usadas.** As chamadas são `calibrateCamera` e `stereoCalibrate` simples. Por isso não
há **erro por vista** (`perViewErrors`) nem **desvio-padrão dos parâmetros** (`stdDeviations*`).

### 3.4 Como os RMS são calculados e reportados

**Convenção do OpenCV 4.5.4, conferida no fonte:**
- `calibrateCamera` devolve `sqrt(Σ‖e‖² / N_cantos)` (`calibration.cpp:1868`, com
  `total += ni` em `:1486`);
- `stereoCalibrate` devolve `sqrt(Σ(‖e_L‖² + ‖e_R‖²) / (2·N_cantos))` (`calibration.cpp:2443`).

Os dois são **por canto, com norma 2D**. Portanto MSE por coordenada = RMS²/2. É a armadilha 1
do Bias Ratio (`product/referencias.md` §A).

**Os RMS "Left" e "Right" reportados vêm da etapa mono**, isto é, das variáveis `rms_l` e `rms_r`
de `opencv_calibration.cpp:169-176`, `:246-251` e `:334-337`. Não vêm dos intrínsecos finais.
- Os K e D **salvos** são os que `stereoCalibrate` reotimizou (`opencv_calibration.hpp:186-188`).
- Portanto o "Left 0,31 px" do relatório não é o erro do K_LEFT que vai para o YAML.
- O "Stereo" é o RMS do OpenCV sobre as duas câmeras.

O `zed_stereo_checker` calcula o seu RMS por vista com a mesma convenção por canto
(`stereo_checker/src/main.cpp:127-146`). O estéreo dele é `sqrt((e_L² + e_R²)/2)` (`:572-574`).

### 3.5 Rejeição de outliers

- **Por quadro inteiro, não por canto.**
  - Erro do quadro = RMS conjunto esquerda + direita.
  - Limiar = max(2,5 × mediana, 0,5 px) (`opencv_calibration.cpp:205-220`, `:296-311`).
  - Com a mediana em ~0,3 px, só quadros acima de ~0,75 px são removidos.
- Não há kernel robusto (Huber ou Cauchy) na otimização.
- Só no modelo radial-tangencial.

### 3.6 Arquivo de saída e compatibilidade com o ZED SDK

- **Conteúdo:**
  - `Size`;
  - `K_LEFT` e `K_RIGHT`;
  - `D_LEFT` e `D_RIGHT` (1×8), ou `D_LEFT_FE` e `D_RIGHT_FE` com fisheye;
  - `R` (vetor de Rodrigues 3×1);
  - `T` (3×1, mm, Tx < 0).

  Fonte: `opencv_calibration.cpp:455-477`.
- **Compatibilidade verificada:** o `zed_stereo_checker --calib_opencv` carrega o arquivo via
  `optional_opencv_calibration_file` e reproduz erros baixos (`stereo_checker/src/main.cpp:333-334`,
  `:406`; `calib_runs/2026-11-03_camera_settings/checker_final.log:27`).
  - O arquivo em uso pelo SLAM tem `D_*` 1×14
    (`petro_localization/config/zed/zed_opencv_calibration_cesar_002.yaml:22-24`), e o SDK aceita os
    dois tamanhos.
  - Não verificado: a variante `_FE`.
- **Sem metadados no YAML:** nem RMS, nem número de amostras, nem data, nem modelo.
- O `.conf` do SDK (`SN<serial>.conf`) só é escrito para ZED X One em dual-mono
  (`opencv_calibration.cpp:440-450`, `:499-663`).

---

## 4. Avaliação existente

| Ferramenta | O que mede | Sobre quais imagens | Onde |
|---|---|---|---|
| `zed_stereo_checker` | Por quadro, ao vivo:<br>• RMS da esquerda e da direita (pose por `solvePnP` em cada câmera, com todas as tags de cada uma);<br>• erro estéreo (pose da esquerda + R, T projetando na direita, com as tags da direita). Também testa os extrínsecos.<br>Lê a calibração **pelo SDK**, então testa a interpretação que o SDK faz do YAML | **Só ao vivo ou SVO.** Nada é salvo, nada é agregado; o número fica só na tela | `stereo_checker/src/main.cpp:519-575`, `:404-406` |
| `stereo_reprojection_viewer` | Visual: nuvem de pontos com `DEPTH_MODE::NEURAL`. Nenhuma métrica | Ao vivo ou SVO; **autocalibração ativa** | `stereo_reprojection_viewer/src/main.cpp:308-338`, `:420-470` |
| Scripts em `calib_runs/tools/` (fora do repositório, descartáveis) | Validação cruzada com intrínsecos fixos (`crossval.py`, `partial.py`, `crossval_acrilico.py`) e diagnósticos de outliers | PNGs das rodadas. **Refazem a detecção em Python**, com dicionário e geometria próprios (ex.: `analyze_outliers.py`, `PITCH = 88.0 * 1.3` fixo no código) | `calib_runs/tools/*.py` |

**Testes automatizados:**
- 9 arquivos em `tests/`, só com OpenCV, ativados por `-DBUILD_TESTS=ON` (`CMakeLists.txt:15-19`;
  `tests/CMakeLists.txt`).
- Cobrem o `target.yaml`, a geometria, a detecção em imagens sintéticas, os parâmetros do
  detector, as regras e os limites de amostras e o arquivo de controles de captura.
- **Não há teste de `calibrate()`** nem do checker.
- A suíte não foi executada nesta auditoria: o `build/` atual foi configurado sem os testes.

---

## 5. Lacunas em relação aos requisitos

| Req. | Pedido | Situação atual | Lacuna |
|---|---|---|---|
| R1 | Pares brutos, `detections.json`, `meta.json` e `calib_<modelo>.yml` por condição e captura | Pares brutos ok; o resto não | Estrutura de pastas; detecções **por câmera, com todas as tags** (não só as comuns); `meta.json`; nada apagado; saída fora do diretório corrente |
| R2 | Roteiro de 63 poses com IDs; distância pela fração da largura; captura estática | Coleta livre; aceite por dissimilaridade | Roteiro em YAML; indicação da pose na GUI; **o filtro de dissimilaridade precisa ser contornado no modo roteiro** (risco 1); fração da largura pela caixa do **alvo inteiro** projetada; imobilidade |
| R3 | Grade 8×8 de cantos por câmera; histograma de inclinação nos dois eixos | Só X, Y, Size e Skew na imagem | Tudo; a inclinação precisa da pose (PnP), não do ângulo na imagem |
| R4 M1 | RMS mono e estéreo, mapa de resíduos, Bias Ratio | RMS mono não corresponde ao K final; sem erro por vista | RMS dos intrínsecos finais; resíduos por região; BR (convenção por coordenada) |
| R4 M2 | Erro de mapeamento e EME empírico | Inexistente | Tudo (offline) |
| R4 M3 | Leave-one-capture-out 10×10 | Só ao vivo, no checker, ou em scripts descartáveis | Tudo (offline, com as detecções salvas) |
| R4 M4 | Erro epipolar retificado, erro métrico, plano | Inexistente | Tudo (offline) |
| R5 | Fábrica raw exportada como "F" | Lida, mas só como chute inicial | Exportação para YAML; F em todas as métricas; cx retificado E × D |
| R6 | Estatística | Inexistente | Tudo (offline) |
| R7 | Teste métrico em distâncias conhecidas | Inexistente | Procedimento + análise; distância medida no `meta.json` |
| R8 | Geometria da porta e da água no `meta.json` | Inexistente | Campos no `meta.json` (não precisa de código de calibração agora) |
| Restrição | "Nada fixo no código" | Limiares no código: RMS 0,5, nitidez 100, área 2 %, dissimilaridade 0,15, X, Y, Size, Skew, resolução AUTO, 15 fps | Levar para YAML **no modo novo**, mantendo os valores atuais como padrão |

---

## 6. Riscos que podem invalidar resultados

Por gravidade:

1. **O filtro de dissimilaridade rejeita o roteiro.** Ele recusa qualquer amostra a menos de 0,15
   de **todas** as anteriores (`calibration_checker.cpp:243-271`).
   - A rotação do alvo no plano (bloco 3) não muda posição, tamanho nem "skew". As poses a
     90/180/270° seriam recusadas.
   - O mesmo vale para poses próximas do bloco 1.
   - Sem tratar isso, o R2 não pode ser cumprido, e o teste do fy/fx girando o alvo fica
     impossível.
2. **A captura ao vivo parte da calibração de fábrica.** O `CALIB_USE_INTRINSIC_GUESS` tem como
   chute inicial a calibração de fábrica (`main.cpp:526-532`; `opencv_calibration.cpp:159`).
   - O brief trata F como mais uma calibração a comparar, então as nossas precisam partir do
     zero.
   - Na Parte 1 de `aprilgrid-target/validation.md` (§2.3) o efeito foi nulo nos intrínsecos,
     até a 5ª casa decimal. Mas isso foi visto numa rodada só, com xadrez.
   - Hoje "ao vivo" e "recálculo" seguem caminhos diferentes.
3. **O RMS mono do relatório não é o dos intrínsecos salvos** (§3.4). O filtro M1 (< 0,5 px) e
   qualquer tabela com "RMS Left/Right" usariam números de outro conjunto de parâmetros.
4. **A calibração mono só usa as tags vistas pelas duas câmeras** (`opencv_calibration.cpp:98`).
   - Perto da borda da imagem, justamente onde a distorção é maior, muitas tags só aparecem numa
     câmera.
   - O bloco 4 (metade do alvo fora) perde a maior parte dos pontos por câmera.
   - O mapa de cobertura R3 por câmera ficaria artificialmente pobre nas bordas.
5. **Convenção do RMS do OpenCV** (§3.4). Usar o RMS² do OpenCV como MSE por coordenada no Bias
   Ratio dá BR ≈ 0,5 numa calibração sem viés.
6. **Detector diferente nas análises.** Os scripts de `calib_runs/tools/` detectam em Python,
   sem o refino D11. As métricas precisam consumir as detecções do C++. Pela armadilha 4 de
   Hagemann, σ_d tem de ser estimado com o mesmo detector da calibração.
7. **Sem imobilidade, com rolling shutter.** A nitidez da imagem inteira não detecta um alvo
   borrado diante de um fundo nítido (`main.cpp:579-586`). Um alvo movido à mão durante a leitura
   do sensor deforma os cantos sem borrar a imagem.
8. **Rastreabilidade.**
   - `zed-images/` é apagado a cada captura.
   - O YAML vai para o diretório corrente; o recálculo grava o serial 0.
   - Não há metadados nem commit.

   Fácil sobrescrever ou misturar capturas entre as 10 de uma condição.
9. **Autocalibração no visualizador** (§1.5). Qualquer número tirado dele vem de uma calibração
   ajustada pelo SDK.
10. **`--svo` sem efeito na calibração** (§1.5). Quem passar um SVO calibra a câmera conectada,
    ou falha se não houver câmera, sem aviso.
11. **Viés fy/fx em aberto** (Parte 4 de `validation.md`). Todas as calibrações AprilGrid têm
    fy/fx +0,2–0,3 %. Se for viés de detecção, as 10 capturas da condição A herdam o problema. O
    piloto, com BR e o alvo girado, decide isso antes de capturar.
12. **Temperatura** depende da permissão do HID, que hoje não existe (`.ai/docs/bench.md`).

---

## 7. O que muda no C++ e o que pode ser feito offline

**Precisa mudar na ferramenta C++, porque depende da câmera ou do SDK:**
- **Persistência:**
  - estrutura `dados/<condicao>/<captura_XX>/{left,right}/<pose_id>_<n>.png` sem apagar nada;
  - `detections.json` **por câmera, com todas as tags**;
  - `meta.json` com serial, resolução, firmware, versão do SDK, timestamp, temperatura
    `ONBOARD_*`, uptime, commit (fixado na compilação), controles de captura, parâmetros do
    detector, medidas do alvo e campos da porta e da água.
- **Modo roteiro:**
  - lê o YAML de poses e mostra a pose esperada;
  - nomeia os arquivos pelo id da pose;
  - **substitui o filtro de dissimilaridade** pela conferência da pose do roteiro.
- **Imobilidade:** cantos estáveis por ~1 s entre quadros consecutivos; nitidez na região do
  alvo.
- **Exportar F:** `calibration_parameters_raw` no mesmo YAML, com a autocalibração desligada.
- **Calibração do zero** como padrão no modo novo, com o prior de fábrica opcional e registrado.
- **Exportar as detecções no recálculo** (`--use_stored_values`), para trazer as capturas
  antigas. Pequeno.
- **Correções menores:** desligar a autocalibração no visualizador; implementar ou remover
  `--svo` na calibração.

**Pode ser feito offline, em Python, a partir dos dados salvos:**
- Cobertura R3 (grade 8×8 e inclinação por PnP).
- Recalibração com vários modelos (`calib_<modelo>.yml`):
  - por câmera, com todas as tags;
  - estéreo com as tags comuns;
  - com as versões Extended (erro por vista e desvios-padrão).
- M1 a M4, F em todas as métricas, estatística R6, relatórios.
- Reajuste futuro com modelos refrativos (R8).

**Não precisa mudar:** a captura bruta em PNG, a leitura raw da fábrica, o detector com refino
D11, o formato de saída e a autocalibração desligada na calibração e no checker.

---

## 8. Ajustes propostos ao backlog (para sua decisão)

1. **`persistencia-captura`:**
   - detecções por câmera com todas as tags, não só as comuns;
   - nada gravado no diretório corrente;
   - o recálculo também exporta as detecções.
2. **`roteiro-poses`:** incluir explicitamente a troca do filtro de dissimilaridade (risco 1).
3. **Nova feature pequena `saida-calibracao` (C++), antes do piloto:**
   - calibração do zero por padrão;
   - RMS por câmera calculado com os intrínsecos finais;
   - erro por vista;
   - metadados num arquivo ao lado do YAML, sem mudar o YAML lido pelo SDK.

   Alternativa: deixar tudo isso para a recalibração offline do `nucleo-offline` e só marcar o
   relatório C++ como "mono pré-estéreo".
4. **`nucleo-offline`:** proibir a detecção em Python; ler só os `detections.json` do C++.
5. **Correções menores, num item `higiene-ferramentas` ou dentro de `persistencia-captura`:**
   autocalibração no visualizador e `--svo` na calibração.
