# Brief metodológico (usuário, 2026-11-03)

> Texto original do brief, redigido pelo usuário. É a fonte dos requisitos R1–R8 e das métricas
> M1–M4 citados na visão, no backlog e nos specs. Valores marcados com `< >` foram respondidos ou
> continuam abertos em [`vision.md`](vision.md) (Perguntas em aberto).

# Contexto

Estou desenvolvendo SLAM subaquático com uma câmera estéreo ZED 2i (lente: <2.1 mm ou 4 mm>; resolução de trabalho do SLAM: <ex.: HD1080>), processada numa ZED Box (Jetson Orin NX 16 GB). Tudo fica dentro de um cilindro de alumínio Blue Robotics, com a câmera olhando para a tampa plana de acrílico transparente. Erros de calibração (intrínsecos esquerda/direita e extrínsecos estéreo) viram erro de localização e de mapa, então a calibração é crítica.

Este repositório (branch develop) é um fork do stereolabs/zed-opencv-calibration adaptado para alvo AprilGrid. O que já sei:
- A autocalibração do ZED SDK (camera_disable_self_calib) já está desabilitada no código.
- Cada condição experimental terá 10 capturas independentes: em cada uma adquiro imagens novas e rodo a calibração do zero.
- A calibração de fábrica será usada como referência de comparação, NÃO como verdade absoluta.

Etapa atual: calibração no ar, sem e com o acrílico. A etapa na água vem depois; agora o código e os dados só precisam estar prontos para ela.

# Como quero que você trabalhe

1. Leia o repositório inteiro sem alterar nada.
2. Escreva a auditoria em .ai/auditoria.md e PARE para eu revisar.
3. Após minha aprovação, escreva os specs no meu formato: .ai/specs/<modulo>/spec.md (especificação e condições de implementação) e .ai/specs/<modulo>/plan.md (plano passo a passo com critério de pronto por passo). Escreva também .ai/specs/experimentos/spec.md e plan.md. PARE para revisão.
4. Só implemente depois que eu aprovar, um módulo por vez, com testes.
Valores marcados com < > ou ausentes: procure no código/configs; se não achar, PERGUNTE. Não invente parâmetros de hardware nem números de literatura.

# Fase 1 — Auditoria (cite arquivo e linha em cada resposta)

Captura e dados
- A ferramenta salva os pares de imagens brutas (não retificadas, sem compressão com perdas) e as detecções de cantos de cada quadro aceito? Onde e em que formato?
- Quais metadados são registrados? Preciso, por captura: serial, resolução, lente, condição, id da captura, id da pose, timestamp, temperatura do sensor do ZED, tempo desde que o sistema ligou, versão do SDK, commit do código.
- Como um quadro é aceito? Existe checagem de cobertura (X, Y, Size, Skew), de imobilidade/borrão (o ZED 2i tem rolling shutter) e de número mínimo de tags?
- Em que resolução/fps a câmera é aberta? Os parâmetros de fábrica são lidos raw (calibration_parameters_raw) ou retificados? Na mesma resolução da calibração?
- Confirme que camera_disable_self_calib=true em TODOS os caminhos (captura ao vivo, SVO, checker/avaliação).

Detecção AprilGrid
- Biblioteca, família de tags, tamanho de tag e espaçamento configurados. Há refinamento subpixel?
- Aceita detecção parcial do alvo (cortado na borda)? Como IDs viram pontos 3D?

Calibração
- Sequência e flags exatos de calibrateCamera/stereoCalibrate (ou fisheye). Qual modelo de distorção (5 coef., racional k1–k6, fisheye)? Intrínsecos são fixados no stereoCalibrate ou otimizados junto?
- Usa as versões Extended (erro por vista e desvio-padrão dos parâmetros)?
- Como os RMS (esquerda, direita, estéreo) são calculados e reportados? Há rejeição de outliers?
- O que vai para o arquivo de saída? Continua compatível com optional_opencv_calibration_file do ZED SDK?

Avaliação existente
- Há checker/visualizador de reprojeção? O que ele mede e sobre quais imagens?

Feche com: lacunas em relação aos requisitos abaixo; riscos que podem invalidar resultados; o que precisa mudar no fluxo de captura (C++) versus o que pode ser feito offline.

# Requisitos metodológicos (base dos specs)

R1. Persistência. Tudo precisa poder ser reprocessado offline com outros modelos, inclusive refrativos na fase da água. Estrutura sugerida:
dados/<condicao>/<captura_XX>/{left,right}/<pose_id>_<n>.png, detections.json, meta.json, calib_<modelo>.yml

R2. Roteiro de poses (mesma ordem e IDs em todas as capturas e condições). Distância definida pela fração da LARGURA da imagem ocupada pelo alvo (na água o FOV encolhe; assim o roteiro fica comparável), registrando também a distância em metros: perto = 70–80%, médio = 40–50%, longe = 20–25%.
- Bloco 1, frontal: 3 distâncias × grade 3×3 de posições (cantos, bordas, centro), alvo paralelo ao sensor = 27 pares.
- Bloco 2, inclinações: distância média, centro + 4 cantos, ±35° em torno do eixo horizontal e ±35° em torno do vertical = 20 pares.
- Bloco 3, rotação no plano (substitui girar a câmera em torno do eixo óptico, inviável no cilindro): perto, centro + 2 cantos opostos, alvo a 0°, 90°, 180°, 270° = 12 pares.
- Bloco 4, borda extrema: perto, metade do alvo fora da imagem nas 4 bordas = 4 pares (se a detecção parcial não for suportada, propor alternativa por câmera).
Captura estática: alvo e câmera parados ~1 s antes de gravar; descartar quadros borrados.

R3. Verificação de cobertura após cada captura, antes de calibrar: mapa de densidade de cantos em grade 8×8 por câmera (mínimo de <N> cantos por célula, inclusive cantos da imagem) e histograma de inclinação do alvo (exigir poses > 30° nos dois eixos). Captura reprovada é refeita.

R4. Métricas, em hierarquia:
- M1, ajuste (filtro, não validação): RMS mono e estéreo (filtro < 0,5 px), mapa de resíduos por região da imagem e Bias Ratio (Hagemann et al., IJCV 2022). Implementação da BR por câmera: com intrínsecos fixos, reestimar via solvePnP a pose de cada tag isolada (4 cantos) em cada imagem; σd² = 4·MSE_v (estimador robusto: MAD × 1,4826); s_d² = MSE_calib / (1 − N_P/N); ε_bias² = max(s_d² − σd², 0); BR = ε_bias²·(1 − N_P/N) / MSE_calib. BR ≈ 0: sem viés; BR → 1: dominado por viés.
- M2, incerteza (EME empírico): erro de mapeamento entre dois modelos A e B = grade de pixels a cada 20 px; desprojetar com A (undistortPoints com critério iterativo reforçado, a lente é grande-angular), projetar com B (projectPoints); reportar RMS, máximo e mapa de calor. Versão opcional otimizando uma rotação de 3 parâmetros para remover o que os extrínsecos compensariam. EME empírico = RMS dos erros de mapeamento de cada uma das 10 calibrações contra a calibração média.
- M3, generalização: validação cruzada leave-one-capture-out. Calibrar com a captura i e avaliar nas imagens da captura j ≠ i, estimando só a pose do alvo (solvePnP na esquerda, intrínsecos fixos), reprojetando na esquerda e, via R,T, na direita (isso testa os extrínsecos). Matriz 10×10; comparar diagonal (treino) com fora da diagonal (teste).
- M4, tarefa: (a) erro epipolar vertical pós-retificação, |y_esq − y_dir| dos cantos nas imagens retificadas (aceitável < 0,25 px; excelente < 0,1 px); (b) erro métrico de triangulação: distâncias entre cantos triangulados versus espaçamento real medido no alvo, em %; (c) resíduo de ajuste de plano dos cantos triangulados. Tudo em função da distância e da região da imagem.

R5. Comparação com a fábrica. Exportar os parâmetros de fábrica raw, na mesma resolução, no mesmo formato OpenCV, e tratá-los como mais uma calibração ("F") em TODAS as métricas: erro de mapeamento F × média (está dentro da dispersão das 10 capturas?) e F versus nossas calibrações aplicadas às mesmas imagens de validação (M3 e M4). Não comparar parâmetro a parâmetro como critério principal (fx/cx/distorção são correlacionados). Em estéreo, reportar também a diferença de cx entre esquerda e direita após retificação, que gera viés de disparidade.

R6. Estatística. Por condição: média, IC95% com t de Student (n = 10) e coeficiente de variação (não usar variância). Comparação A × B: (1) teste de permutação comparando erros de mapeamento A–A (45 pares) com A–B (100 pares); (2) aplicação cruzada (calibração de A nas imagens de B e vice-versa, M3/M4); (3) TOST em fx, fy, baseline e erro métrico, com margem <MARGEM>; (4) Levene/F para comparar dispersões (B pior que A indica problema de montagem). Para a margem, usar ΔZ/Z ≈ Δf/f + ΔB/B e, para viés de disparidade, ΔZ/Z ≈ Δd·Z/(f·B); proponha valores a partir da faixa de operação do SLAM (<faixa, ex.: 0,5–4 m>) e me pergunte antes de fixar.

R7. Teste métrico (sessão separada por condição): câmera em distâncias conhecidas <0,5 / 1 / 2 / 3 m, ajustar à faixa do SLAM>, medidas com trena ou laser; em cada uma, 5 pares com o alvo no centro e 4 com o alvo nas bordas. Medir com paquímetro o tamanho real das tags e o espaçamento antes do teste, e verificar planicidade do alvo.

R8. Preparação para a água (NÃO implementar agora, só garantir compatibilidade): os dados salvos devem permitir ajustar depois (a) pinhole absorvendo a refração e (b) modelos refrativos de porta plana (Pinax, axial/Agrawal, refrativo com n explícito). Prever no meta.json campos para a geometria da porta: distância lente–acrílico, espessura, inclinação e índice de refração. Na água, as hipóteses a testar serão fx_água/fx_ar ≈ 1,33 e baseline ≈ constante (~120 mm).

# Plano de experimentos (.ai/specs/experimentos/)

Condições, com portões de decisão:
- A: ar, sem acrílico. 10 capturas + teste métrico. Portão: nossa calibração empata ou supera F na validação? Saída: intrínsecos "secos" de referência.
- B: ar, com acrílico. Mesmo roteiro; desmontar e remontar a tampa entre capturas. Portão: B é equivalente a A (R6)?
- B': recalibração no ar com acrílico, SOMENTE se B falhar no portão.
- C1/C2 (água, futuro): pinhole absorvendo refração versus modelo refrativo; comparação por faixa de distância (calibrar numa faixa e validar em outra, incluindo distâncias maiores que as de calibração).
Para cada condição: preparação (aquecimento até regime térmico, tempo <X min>; limpeza do acrílico; checagem de reflexos internos com cena escura e iluminação ligada; medição do alvo), procedimento, checklist por captura, critérios de aceite e análise prevista.

# Módulos sugeridos (ajuste após a auditoria)

- captura: alterações mínimas na ferramenta C++ para salvar imagens brutas, detecções e metadados, e para seguir os IDs de pose do roteiro.
- cobertura: verificação R3 com relatório por captura.
- metricas: Python offline (OpenCV, NumPy, SciPy) lendo os dados salvos; M1 a M4 e erro de mapeamento.
- validacao-metrica: R7.
- analise: estatística R6 e relatório comparativo (tabelas e figuras) por condição.
Testes mínimos: erro de mapeamento entre calibrações idênticas = 0; perturbação conhecida em fx gera o deslocamento esperado em pixels; BR ≈ 0 em dados sintéticos sem viés e alta quando a distorção é omitida do modelo; validação cruzada reproduz o RMS de treino quando i = j.

# Restrições

- Não quebrar o fluxo atual nem a compatibilidade do arquivo de saída com o ZED SDK.
- Configuração em arquivo (YAML), nada hardcoded; execução reproduzível.
- Nos specs, justificar decisões metodológicas com as referências abaixo.

# Referências

Treibitz, Schechner, Kunz & Singh, Flat Refractive Geometry (TPAMI 2012); Agrawal et al., A Theory of Multi-Layer Flat Refractive Geometry (CVPR 2012); Sedlazeck & Koch, Perspective and non-perspective camera models in underwater imaging (2012); Jordt-Sedlazeck & Koch, Refractive Calibration of Underwater Cameras (ECCV 2012); Łuczyński, Pfingsthorn & Birk, The Pinax-Model (Ocean Engineering 2017); Shortis, Calibration Techniques for Accurate Measurements by Underwater Camera Systems (Sensors 2015); Hagemann et al., Inferring bias and uncertainty in camera calibration (IJCV 2022); Peng & Sturm, Calibration Wizard (ICCV 2019); Singh, Dharmadhikari & Alexis, An Online Self-calibrating Refractive Camera Model (2023); She et al., Refractive COLMAP (2024).
