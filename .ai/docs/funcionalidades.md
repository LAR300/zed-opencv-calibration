# Funcionalidades e métricas implementadas

O que foi acrescentado ao fork da Stereolabs (`zed-opencv-calibration`) para a metodologia de
calibração da ZED 2i: cada funcionalidade da ferramenta de captura e cada métrica da análise
offline, para que serve e de onde veio. Como usar: [`fluxo-coleta.md`](fluxo-coleta.md) e
[`fluxo-analise-offline.md`](fluxo-analise-offline.md). O registro de cada decisão está no spec
indicado (`.ai/specs/<spec>/`).

**Fontes.** `[n]` = artigo da lista de [Referências](#referências). "Brief" = o brief metodológico
(`.ai/product/brief.md`, requisitos R1–R8 e métricas M1–M4). "Auditoria" = `.ai/auditoria.md`.
"Nossos dados" = medido nas capturas da bancada. "Decisão própria" = escolha deste trabalho, sem
fonte externa, registrada no spec.

## 1. Ferramenta de captura e calibração (C++)

### 1.1 Alvo e detecção

| Funcionalidade | O que faz / como usar | Por que | Fonte | Spec |
|---|---|---|---|---|
| **AprilGrid do Kalibr** | `--target_yaml target.yaml` em vez do xadrez, na calibração e no checker. Cada tag tem um ID, então vale a **vista parcial**: um par serve com 12 tags nas duas imagens, em 2 linhas e 2 colunas (`--min_tags/rows/cols`) | O xadrez precisa aparecer inteiro: não leva pontos às bordas, onde a distorção é maior, e não serve debaixo d'água, onde a turbidez esconde parte do alvo | Formato e numeração do Kalibr; detector ArUco do OpenCV | `aprilgrid-target` |
| **Refino no cruzamento em X** | O ArUco só localiza e decodifica as tags; cada canto é refinado com `cornerSubPix` na imagem original, numa janela de 0,15 × o lado da tag; limiar adaptativo 25 | Os quadradinhos do Kalibr tocam as quinas das tags; o refino padrão deixava cantos a 5–7 px. Com o refino: vistas válidas de 18/38 para 34/38, RMS por vista de 0,54 para 0,44 px. O refino tira ~90 % do viés dos cantos crus | Princípio do Kalibr (detectar aproximado, refinar no cruzamento); nossos dados | `aprilgrid-target` (D11) |
| **Parâmetros do detector** | `--detector_params arquivo.yml` (campos do `cv::aruco::DetectorParameters`), sem recompilar | Ajustar a detecção a outra luz ou outro alvo | Decisão própria | `aprilgrid-target` (D9) |

### 1.2 Controle da coleta

| Funcionalidade | O que faz / como usar | Por que | Fonte | Spec |
|---|---|---|---|---|
| **Controles de câmera fixos** | `--camera_settings config/camera_settings_zed2i.yaml`: exposição, ganho, white balance, nitidez e gamma fixos durante a coleta, restaurados ao sair, registrados no log e no `meta.json` | O automático muda a imagem entre quadros (borrão, ruído) e nada registrava os valores. Medido: com exposição automática, o ruído de um alvo parado foi ~60 % maior (0,52 contra 0,32 px) | Nossos dados | `camera-settings` |
| **Limites de amostras e fim manual** | `--min_samples` (25), `--max_samples` (35; 0 = sem máximo): no máximo a coleta **pausa** e pergunta; `f` encerra e calibra | A coleta terminava sozinha no máximo, com métricas incompletas: a primeira captura AprilGrid saiu com fx 0,7 % errado e RMS 0,32 px | Nossos dados | `sample-limits` |

### 1.3 Captura rastreável

| Funcionalidade | O que faz / como usar | Por que | Fonte | Spec |
|---|---|---|---|---|
| **Modo sessão** | `--session sessao.yaml`: cada captura em `<output_root>/<condição>/captura_XX/`, nunca sobrescrita | Antes, uma coleta nova apagava `zed-images/` | Brief R1; auditoria (risco 8) | `persistencia-captura` |
| **Pares brutos e `detections.json`** | Pares sem retificação, sem perda; **todos** os tags de cada câmera, com os cantos antes e depois do refino | Refazer qualquer análise ou modelo depois, sem a câmera; a calibração antiga usava só os tags comuns às duas câmeras | Brief R1, R8; [4], [5]: o pinhole na água só vale na distância de calibração, então os dados brutos são guardados para um modelo refrativo | `persistencia-captura` |
| **`meta.json`** | Câmera, SDK, firmware, commit do código, controles no início e no fim, medidas do alvo, porta e meio; por par: timestamp, tempo desde a abertura, temperaturas dos sensores, pose medida, imobilidade; eventos do roteiro | Rastreabilidade; temperatura para o efeito térmico; porta e meio para os modelos refrativos | Brief R1, R8; [2], [5] (parâmetros de um modelo de porta plana) | `persistencia-captura` |
| **Commit no binário** | Cada build grava o commit do git (e se havia mudanças locais) | Saber com que código cada captura foi feita | Decisão própria | `persistencia-captura` |

### 1.4 Calibração e relatório

| Funcionalidade | O que faz / como usar | Por que | Fonte | Spec |
|---|---|---|---|---|
| **Calibração do zero** | Na sessão, `calibration.prior: none` (padrão) parte do zero; `factory` parte de F | Antes a coleta ao vivo partia da fábrica: a calibração não era independente para ser comparada a F | Brief R5; auditoria (risco 2) | `saida-calibracao` |
| **Relatório fiel** | `calib_cpp_<modelo>.report.json`: RMS estéreo, RMS de cada câmera com os parâmetros **salvos** e com pose livre, erro por vista, vistas removidas, desvios-padrão, convenção do RMS escrita | O "Left/Right" impresso era da etapa mono, não dos parâmetros salvos | Auditoria (risco 3) | `saida-calibracao` |
| **Recalcular sem câmera** | `--recalibrate captura_XX`; `--use_stored_values --detections_out detections.json` para capturas antigas | Reprocessar e trazer as capturas antigas para a análise com o mesmo detector | Brief R1 | `saida-calibracao`, `persistencia-captura` |
| **Calibrar de um SVO** | `--svo arquivo.svo2` | Calibrar a partir de gravações | Decisão própria | `higiene-ferramentas` |

### 1.5 Coleta guiada

| Funcionalidade | O que faz / como usar | Por que | Fonte | Spec |
|---|---|---|---|---|
| **Roteiro de poses** | `roteiro:` na sessão (`config/roteiro_v1.yaml`, 63 poses com IDs): a tela desenha a pose pedida e mede a do alvo (fração da largura, centro, inclinações, rotação, distância); teclas `s`/`o`/`k`/`b` registradas no `meta.json` | Capturas repetíveis e comparáveis entre si e entre condições; garantir poses informativas: inclinação grande, alvo nas bordas, alvo girado | Brief R2; [8] (inclinação grande, alvo tocando as bordas); [6] (alvo girado 0/90/180/270°, enchendo a imagem) | `roteiro-poses` |
| **Captura estática** | No modo roteiro, `s` só salva com o alvo parado por `window_s` (deslocamento < `max_displacement_px`) e nítido (> `min_target_sharpness`) | A ZED 2i tem rolling shutter: alvo em movimento deforma a imagem | [6] (quadros parados com rolling shutter); nossos dados (o efeito existe nas capturas, ~1–2 % do resíduo) | `captura-estatica` |
| **Medição do ruído estático** | `--measure_static_noise 30`: com o alvo parado, mede o deslocamento entre quadros e a nitidez e propõe os limiares (p95 do deslocamento; metade do p05 da nitidez) | Limiares medidos, não chutados | Decisão própria | `captura-estatica` |
| **Registro de aquecimento** | `--timelapse 30 60`: um par a cada 30 s por 60 min, com as temperaturas, sem calibrar | Medir quanto tempo a câmera leva para estabilizar antes de capturar | A literatura lida não trata disso ([6]); por isso é medido | `aquecimento-termico` |

### 1.6 Ferramentas auxiliares

| Funcionalidade | O que faz / como usar | Por que | Fonte | Spec |
|---|---|---|---|---|
| **`zed_export_factory`** | Exporta a calibração de fábrica F (`calibration_parameters_raw`, autocalibração desligada) para o mesmo YAML da ferramenta; `--verify` confere que o SDK a lê de volta igual | F vira mais uma calibração, avaliada com as mesmas métricas: é a referência de comparação, não a verdade | Brief R5; documentação do ZED SDK | `exportar-fabrica` |
| **Acesso ao SDK e ao HID** | `zed_access_check` (lê as temperaturas), `scripts/bench/check_zed_access.sh --check` (no host), regra udev e scripts de correção | Sem o HID não há temperatura nem IMU | Problema de bancada | `ambiente-sensores` |
| **Visualizador sem autocalibração** | `zed_reprojection_viewer` abre com `camera_disable_self_calib = true` | A autocalibração do SDK alterava os parâmetros mostrados | Documentação do ZED SDK; auditoria | `higiene-ferramentas` |
| **Utilitários de bancada** | `scripts/dev/` (`build.sh`): ler os controles da câmera, apertar teclas num Xvfb, conferir detecções, erros do checker sem janela | Testes de bancada sem tocar no teclado | Decisão própria | [`bench.md`](bench.md) |

## 2. Análise offline (`analysis/zedcal`)

### 2.1 Núcleo

| Funcionalidade | O que faz | Por que | Fonte | Spec |
|---|---|---|---|---|
| **Leitura e escrita** | Lê `detections.json`, `meta.json` e os YAML do SDK; escreve YAML no formato do SDK | Toda métrica parte dos arquivos gravados | Brief | `nucleo-offline` |
| **Calibração offline com variantes** | `calibrate`: os mesmos passos do C++ (paridade verificada: RMS idêntico) e variantes (todos os tags × só os comuns, estéreo `joint` × `fixed`, com ou sem rejeição, `rational8`/`radtan5`, prior) | Testar escolhas de calibração sem mexer no C++; o M3 escolheu `joint`, a do C++ | Brief | `nucleo-offline`, `validacao-cruzada` |
| **Sem detecção em Python** | O Python usa sempre os cantos do detector C++ | O Bias Ratio precisa do ruído do mesmo detector que a calibração usou | [1]; auditoria (risco 6) | `nucleo-offline` |
| **Bootstrap** | `bootstrap: {n: 200}`: desvio-padrão dos parâmetros reamostrando as vistas | A incerteza que o ajuste informa é otimista | [1] (bootstrap calibra bem a incerteza) | `nucleo-offline` |
| **Dados sintéticos** | Câmeras e alvos sintéticos com ruído e viés controlados | Testar cada métrica com a resposta conhecida | Decisão própria | `nucleo-offline` |

### 2.2 Métricas

| Métrica | O que mede | Como ajuda na calibração | Como ler (referência) | Fonte | Spec |
|---|---|---|---|---|---|
| **M1 · RMS de reprojeção** | Distância média entre canto detectado e previsto, por canto (norma 2D); o MSE por coordenada é RMS²/2 | Filtro de qualidade (< 0,5 px) | Típico no ar: 0,03–0,1 px [6]; nossas capturas: 0,25 (xadrez) a 0,37 px | Brief M1; [6]; auditoria (convenção do OpenCV) | `metrica-ajuste` |
| **M1 · Bias Ratio, σ_d, ε_bias** | Separa o erro em ruído do detector (σ_d, medido em tags isolados com os intrínsecos fixos) e viés (ε_bias); BR = fração do erro que é viés | Um RMS baixo pode esconder viés, e viés não diminui com mais imagens. Foi como achamos o defeito do alvo | 0 = só ruído; xadrez 0,24–0,31; AprilGrid do laboratório ~0,70 | [1] | `metrica-ajuste` |
| **M1 · Mapa de resíduos** | RMS e resíduo médio com sinal por célula 8 × 8 da imagem | Mostra onde e em que direção o modelo erra | Setas pequenas e aleatórias = ruído; grandes e alinhadas = viés | [1] | `metrica-ajuste` |
| **M2 · Erro de mapeamento e EME** | Diferença em px entre duas calibrações projetando os mesmos raios (com uma rotação livre, como os extrínsecos compensam); referência média da condição no espaço da imagem; EME = dispersão das calibrações em torno dela | Compara calibrações entre si e com F sem depender de imagens; mostra se a condição é repetível | Quanto menor o EME, mais repetível; F no percentil 100 = mais longe que todas | [1] | `erro-mapeamento` |
| **M3 · Validação cruzada** | Cada calibração avaliada nas capturas que não usou: esquerda, transferência para a direita (extrínsecos) e estéreo; aplicação cruzada entre condições | Mede generalização, não só ajuste; decidiu a variante de calibração | Razão teste/treino perto de 1 = generaliza | Brief M3 | `validacao-cruzada` |
| **M4 · Erro epipolar** | Desalinhamento vertical após a retificação, em px, e frações abaixo de 0,25 e 0,1 px | É o que o matching estéreo do SLAM sente | Nossas calibrações 0,16–0,19 px; F 0,20–0,21 px | Brief M4 | `metricas-tarefa` |
| **M4 · Erro métrico** | Lados dos tags, passo e vão do alvo triangulados contra o alvo medido, em % | Erro de escala em milímetros | F +0,35 % no vão; acrílico muda a escala ~0,9 % | Brief M4; [6] (validar com comprimentos conhecidos) | `metricas-tarefa` |
| **M4 · Resíduo de plano** | Dispersão dos pontos triangulados em torno do plano do alvo | Ruído de profundidade | Cresce com Z²; igual entre calibrações | Brief M4 | `metricas-tarefa` |
| **M4 · Viés de disparidade Δd** | Diferença de disparidade efetiva entre duas calibrações, pela profundidade triangulada: Δd = f·B·(1/Z_b − 1/Z_a) | Vira erro de profundidade que cresce com a distância (dZ/Z ≈ Δd/d) | 0,2 px ≈ 1 % a 3 m | Decisão própria: a diferença de cx após a retificação, pedida no brief, não mede o viés | `metricas-tarefa` |
| **Cobertura** | Cantos por célula 8 × 8 da imagem; vistas inclinadas além de 30° em cada eixo, com os dois sinais | Reprova a captura antes de confiar nela: bordas vazias deixam a distorção extrapolada; sem inclinação, focal e distância se confundem | Critério: N cantos por célula (provisório: 1), 2 vistas inclinadas por eixo | Brief R3; [8]; [6] | `cobertura` |
| **Estatística da condição** | Média, IC 95 % (t), DP, CV por parâmetro; onde F cai | Repetibilidade da calibração; posição da fábrica | CV "não informativo" para grandezas perto de zero | Brief R6 | `analise-estatistica` |
| **Comparação A × B** | Permutação exata dos rótulos sobre a matriz de mapeamento; aplicação cruzada; TOST de Welch com margens; Brown–Forsythe e razão F das dispersões; Holm | Responde aos portões: B equivale a A? | Margens e α provisórios; com 10 capturas o Brown–Forsythe detecta um DP dobrado em 31 % das vezes, a razão F em 64 % | Brief R6; métodos estatísticos clássicos | `analise-estatistica` |
| **Teste métrico** | Distância do plano do alvo triangulada contra a medida, com um deslocamento d0 comum a todas as distâncias; erro métrico por distância | Validação da escala independente do alvo da calibração | Erro proporcional à distância = erro de escala | Brief R7; [6]; [3] (o centro de projeção efetivo não é medível, daí d0) | `validacao-metrica` |
| **Regime térmico** | Pose do alvo e transferência estéreo no tempo, com F fixa; instante em que cada série fica dentro de 2σ do valor final | Define o tempo X de aquecimento antes de capturar | ~95 % das simulações acertam a menos de uma janela (5 min) | Decisão própria (a literatura lida não trata) | `aquecimento-termico` |

## 3. Achados que vieram dos dados, não da literatura

- **O alvo AprilGrid impresso tem defeito:** cantos ~0,4 mm fora da grade, com o mesmo padrão nas
  três capturas (correlação 0,92–0,94), e ~0,23 % de esticamento em Y, que vira o fy/fx 0,2 % acima
  de 1. Causa da maior parte do Bias Ratio de ~0,70. Descartados: o refino, as bordas, o rolling
  shutter, o empenamento, a seleção de tags (`piloto-dados-existentes/resultado.md`).
- **O acrílico muda a calibração:** escala ~0,9 % e transferência estéreo de 0,39 para 0,66–0,83 px.
- **Os limiares de imobilidade** dependem dos controles da câmera (0,50 px fixos contra 0,78 px
  automáticos).

## Referências

Verificadas no Crossref e no arXiv; notas de leitura em
[`../product/referencias.md`](../product/referencias.md).

1. Hagemann, Knorr, Janssen, Stiller. *Inferring Bias and Uncertainty in Camera Calibration.*
   IJCV 130(1):17–32, 2022. DOI 10.1007/s11263-021-01528-x. [arXiv 2107.13484](https://arxiv.org/abs/2107.13484).
   **Usado em:** Bias Ratio, mapa de resíduos, erro de mapeamento e EME, bootstrap.
2. Agrawal, Ramalingam, Taguchi, Chari. *A Theory of Multi-Layer Flat Refractive Geometry.*
   CVPR 2012. DOI 10.1109/CVPR.2012.6248073. [MERL TR2012-047](https://shadow.merl.com/publications/docs/TR2012-047.pdf).
   **Usado em:** que parâmetros da porta e do meio registrar no `meta.json`.
3. Treibitz, Schechner, Kunz, Singh. *Flat Refractive Geometry.* IEEE TPAMI 34(1):51–65, 2012.
   DOI 10.1109/TPAMI.2011.105. [PDF](https://webee.technion.ac.il/~yoav/publications/jFlat_published.pdf).
   **Usado em:** limites do pinhole atrás de porta plana; cuidado com a montagem da porta; o
   deslocamento d0 do teste métrico.
4. Jordt-Sedlazeck, Koch. *Refractive Calibration of Underwater Cameras.* ECCV 2012.
   DOI 10.1007/978-3-642-33715-4_61. Com a tese de Jordt (Kiel, 2014).
   **Usado em:** calibrar no ar primeiro (condição A) e guardar os dados brutos.
5. Łuczyński, Pfingsthorn, Birk. *The Pinax-model for accurate and efficient refraction
   correction of underwater cameras in flat-pane housings.* Ocean Engineering 133:9–22, 2017.
   DOI 10.1016/j.oceaneng.2017.01.029. [Código](https://github.com/tomluc/Pinax-camera-model).
   **Usado em:** campos da porta (distância, espessura, índices) e salinidade; candidato à etapa na água.
6. Shortis. *Calibration Techniques for Accurate Measurements by Underwater Camera Systems.*
   Sensors 15(12):30810–30826, 2015. DOI 10.3390/s151229831.
   **Usado em:** roteiro (alvo girado, enchendo a imagem), captura estática, valores típicos de
   RMS, validação com comprimentos conhecidos, ligação rígida lente–porta.
7. She, Seegräber, Nakath, Köser. *Refractive COLMAP: Refractive Structure-from-Motion Revisited.*
   IROS 2024. [arXiv 2403.08640](https://arxiv.org/abs/2403.08640).
   **Usado em:** quando o pinhole absorve a refração (porta ortogonal), para a etapa na água.
8. Peng, Sturm. *Calibration Wizard.* ICCV 2019. DOI 10.1109/ICCV.2019.00158.
   [arXiv 1811.03264](https://arxiv.org/abs/1811.03264).
   **Usado em:** roteiro (inclinação grande, alvo nas bordas) e critérios de cobertura.
9. Singh, Dharmadhikari, Alexis. *An Online Self-calibrating Refractive Camera Model with
   Application to Underwater Odometry.* ICRA 2024. [arXiv 2310.16658](https://arxiv.org/abs/2310.16658).
   **Usado em:** contexto da etapa na água.

Outras fontes: documentação do ZED SDK 5.1 (autocalibração, formato do arquivo OpenCV,
`calibration_parameters_raw`); formato AprilGrid do Kalibr; índices de refração (PMMA, água
doce e salgada) em `referencias.md` §D. Os métodos estatísticos (Welch, TOST, Brown–Forsythe,
razão F, Holm, permutação) são clássicos e foram escolhidos a partir do brief.
