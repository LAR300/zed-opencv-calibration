# Visão de produto — Metodologia de calibração estéreo da ZED 2i para SLAM subaquático

> **O quê e para quem**, no nível de produto. Construída a partir do brief metodológico do usuário
> (2026-11-03) e da pesquisa em [`referencias.md`](referencias.md). Aprovada antes de detalhar os
> specs.

## Problema

O SLAM do miniROV (cuVSLAM + nvblox) usa uma ZED 2i dentro de um cilindro Blue Robotics, atrás de
uma tampa plana de acrílico. Erro nos intrínsecos ou nos extrínsecos estéreo vira erro de
localização e de mapa. Hoje não é possível dizer, com evidência, se uma calibração é melhor que
outra. Quatro motivos:

- **Cada calibração é uma rodada única.** Faltam repetições para medir a dispersão, e não há
  roteiro de poses que torne as rodadas comparáveis.
- **O critério é só o RMS de reprojeção.** Ele não denuncia viés: na Parte 1, §3, de
  [`validation.md`](../specs/aprilgrid-target/validation.md), o RMS foi idêntico com a escala do
  alvo errada. Nossas calibrações AprilGrid têm ~0,31 px, contra 0,03–0,1 px típicos no ar segundo
  Shortis (2015). Elas também carregam um fy/fx +0,2–0,3 % sem explicação.
- **Os dados não ficam para reprocessar.** Detecções, metadados e condições não são salvos. A
  etapa na água exigirá reajustar os mesmos dados com modelos refrativos.
- **As calibrações em uso pelo SLAM vieram de outra ferramenta, com outro detector.**
  `zed_opencv_calibration_cesar_002.yaml` (RMS 0,94 px) e `macae_001` saíram de
  `calibrate_intrinsics.py`. Esse detector tem o viés de cantos corrigido em D11 de
  [`decisions.md`](../specs/aprilgrid-target/decisions.md).

## Usuários / público

- **A equipe de percepção do miniROV**, que calibra na bancada e entrega o arquivo ao SLAM via
  `optional_opencv_calibration_file`.
- **Quem revisa os resultados** e decide se uma montagem (com acrílico, depois na água) está
  validada. Essa pessoa precisa de números com intervalo de confiança, não de um RMS isolado.

## Proposta de valor

- **Calibração justificada.** Toda calibração entregue ao SLAM vem com evidência de que não tem
  viés (Bias Ratio), com a incerteza medida em 10 capturas independentes e com o desempenho na
  tarefa (erro epipolar e erro métrico de triangulação).
- **Comparações com teste estatístico.** Fábrica × nossa e sem × com acrílico são decididas por
  um teste estatístico e por portões definidos antes, não por inspeção de parâmetros.
- **Dados prontos para a etapa na água.** Os dados de cada captura ficam guardados e podem ser
  reajustados offline com qualquer modelo, inclusive refrativo, sem nova coleta.

## Escopo (capacidades principais)

1. **Captura rastreável (C++).** A ferramenta salva os pares brutos (PNG), as detecções e os
   metadados de cada quadro aceito, numa estrutura por condição, captura e pose. Ela segue um
   roteiro de poses fixo e só aceita quadros estáticos.
2. **Verificação de cobertura** antes de calibrar: densidade de cantos numa grade 8×8 e
   histograma de inclinação. Uma captura reprovada é refeita.
3. **Métricas offline (Python).**
   - M1, ajuste: RMS, mapa de resíduos e Bias Ratio (Hagemann 2022).
   - M2, incerteza: erro de mapeamento e EME empírico.
   - M3, generalização: validação cruzada leave-one-capture-out.
   - M4, tarefa: erro epipolar após retificação, erro métrico de triangulação e resíduo de plano.
4. **Calibração de fábrica como mais uma calibração ("F")** em todas as métricas, exportada raw na
   mesma resolução.
5. **Análise estatística** por condição: média, IC95 %, CV, permutação, TOST e Levene. Gera um
   relatório comparativo.
6. **Protocolo de experimentos**, com preparação, checklists e portões de decisão. A: ar sem
   acrílico; B: ar com acrílico; B′ condicional. Inclui o teste métrico em distâncias conhecidas.

## Fora de escopo (por agora)

- **Executar as condições na água (C1/C2) e implementar modelos refrativos** (Pinax, axial e
  similares). Agora, só garantir que os dados e metadados permitam isso depois (R8).
- **Mexer na calibração interna da câmera ou reescrever a calibração na EEPROM.** O arquivo
  OpenCV continua sendo carregado em tempo de execução.
- **Seleção automática da próxima pose**, no estilo Calibration Wizard. O roteiro é fixo, para
  ser comparável entre capturas.
- **Mudanças no `zed-ros2-wrapper` ou no grafo do SLAM**, além de apontar para o arquivo validado.

## Restrições e premissas

- **Compatibilidade.**
  - Não quebrar o fluxo atual das ferramentas: sem as flags novas, o comportamento é o de hoje.
  - O arquivo de saída continua compatível com o `optional_opencv_calibration_file` do ZED SDK
    (`Size`, `K_*`, `D_*`, `R` em Rodrigues, `T` em mm).
- **Configuração e reprodutibilidade.**
  - Configuração em YAML, nada fixo no código.
  - Execução reproduzível: commit do código e versão do SDK ficam no `meta.json`.
- **Autocalibração do SDK desligada** (`camera_disable_self_calib = true`) em todos os caminhos de
  captura e avaliação.
  - Hoje está ligado só em `stereo_calibration/src/main.cpp:397` e `stereo_checker/src/main.cpp:327`.
- **Mesmo detector em todas as métricas.** As métricas offline consomem as detecções produzidas
  pelo detector C++. Assim a calibração e o Bias Ratio usam os mesmos cantos (Hagemann: armadilha
  4 em `referencias.md`).
- **Dez capturas independentes por condição**, cada uma com imagens novas e calibração do zero.
- **Fábrica é referência, não verdade.**
- **Ambiente.**
  - Ferramentas no container `isaac-ros-dev`: ZED SDK 5.1, OpenCV 4.5.4, NumPy 1.26.4, SciPy 1.15.3.
  - Dados das rodadas fora do repositório, em `ros2_ws/dev/calib_runs/`.
- **Valores de hardware e de literatura não são inventados.** O que não está no código ou nas
  configs é perguntado.

## Contexto técnico já levantado

| Item | Valor | Fonte |
|---|---|---|
| Câmera de bancada | ZED 2i S/N 35180697, lente de 2,1 mm. **Não é a câmera do robô**: mesmo modelo, outro número de série | `validation.md`; usuário, 2026-11-03 |
| Plataforma de captura | notebook da bancada, com o container `isaac-ros-dev` | usuário, 2026-11-03 |
| Montagem da coleta | câmera parada, alvo movido | usuário, 2026-11-03 |
| Faixa de operação do SLAM | até 3 m. Na água, a visibilidade fica muito ruim depois disso | usuário, 2026-11-03 |
| Resolução e fps do SLAM, e da calibração | HD720 @ 15 fps | `petro_localization/config/zed/zed_localization.yaml:24-25`; confirmado pelo usuário |
| Resolução da ferramenta | `RESOLUTION::AUTO` (= HD720 na ZED 2i), 15 fps | `stereo_calibration/src/main.cpp:392-394` |
| Imagens | `*_UNRECTIFIED_BGR`, PNG, em `zed-images/` (apagado a cada captura) | `stereo_calibration/src/main.cpp:564-574, 618, 981` |
| Fábrica | lida de `calibration_parameters_raw`; fx L 533,82, baseline 119,982 mm em HD720 | `main.cpp:532`; `SN35180697.conf` |
| HFOV estimado em HD720 | ~100° (2·atan(640/533,8)) | cálculo |
| Alvo | AprilGrid 6x6 tag36h11, tag de 88 mm, espaço de 26,4 mm, 660 mm de lado, placa rígida | `validation.md` Parte 2 |
| Distâncias do roteiro (R2), estimadas por pinhole com HFOV 100° | perto 70–80 % → 0,35–0,40 m; médio 40–50 % → 0,55–0,69 m; longe 20–25 % → 1,1–1,4 m | cálculo, a confirmar na bancada |
| Sensores de temperatura | `ONBOARD_LEFT/RIGHT`, `IMU`, `BAROMETER` no SDK 5.1; dependem do acesso ao HID (hoje sem permissão) | `Camera.hpp:4407`; `.ai/docs/bench.md` |
| Resultados anteriores | xadrez 0,25 px; AprilGrid ~0,31 px com fy/fx +0,2–0,3 %; o acrílico no ar muda o alinhamento estéreo, não os intrínsecos | `validation.md` Partes 1–4 |

## O que a literatura impõe ao projeto

Detalhes e citações em [`referencias.md`](referencias.md).

- **Bias Ratio (Hagemann).** A implementação proposta confere com o artigo, mas pede três cuidados:
  - contar tudo por coordenada (o RMS do OpenCV é por canto);
  - estimador robusto dos dois lados;
  - `SOLVEPNP_IPPE_SQUARE` + LM nos alvos virtuais de 4 cantos.

  O artigo não publicou código, então o BR é implementado por nós.
- **Rotação do alvo no plano (0/90/180/270°).** É recomendada por Shortis e é o teste pendente do
  fy/fx.
- **Água: o pinhole que absorve a refração tem limites.** A razão fx_água/fx_ar ≈ 1,33 só vale no
  centro da imagem. Com a lente de 2,1 mm, o meio-ângulo na água (~38°) passa do limite de ~30°
  de Treibitz. O pinhole só vale perto da distância de calibração. Por isso o protocolo valida em
  distâncias fora da faixa de calibração.
- **Geometria da porta, a registrar desde já:**
  - distância lente–acrílico (o Pinax supõe d0 ≈ 0,15 × espessura);
  - espessura;
  - inclinação (2° já causa erros grandes);
  - índice do acrílico (PMMA 1,4905 a 589 nm);
  - temperatura e salinidade da água (Quan & Fry 1995).

  A ligação rígida entre lente e porta é o fator mais importante (Shortis).

## Perguntas em aberto

### Respondidas (2026-11-03)

- **Lente:** 2,1 mm.
- **Resolução:** HD720 @ 15 fps, a mesma do SLAM.
- **Câmera:** a de bancada (S/N 35180697) é do mesmo modelo da câmera do robô, mas não é ela.
  Consequências:
  - os experimentos A e B validam a **metodologia**;
  - a calibração entregue ao SLAM terá de ser refeita na câmera do robô, com o mesmo protocolo;
  - o serial no `meta.json` é obrigatório.
- **Plataforma:** notebook da bancada.
- **Montagem:** a câmera fica parada e o alvo se move.
- **Faixa do SLAM:** até 3 m. Consequências:
  - o teste métrico (R7) vai até 3 m;
  - as distâncias acima do "longe" do roteiro (~1,1–1,4 m) servem de validação fora da faixa de
    calibração, como recomenda Shortis.
- **Local:** código Python em `analysis/` neste repositório; dados em
  `ros2_ws/dev/calib_runs/dados/<condicao>/<captura_XX>/`.

### Ainda em aberto (não bloqueiam os specs; voltar quando a feature indicada chegar)

- NEEDS CLARIFICATION: **Calibrações em uso pelo SLAM.** O que são `cesar_002` (fx 1293) e
  `macae_001` (fx 1372)? São 2,4–2,6× a fábrica, longe do ~1,33× esperado na água.
  *(piloto-dados-existentes)*
- NEEDS CLARIFICATION: **Limite inferior da faixa do SLAM**, se houver. Ele entra na margem do TOST.
  *(analise-estatistica)*
- NEEDS CLARIFICATION: **Como inclinar o alvo** a ±35° nos dois eixos e girá-lo no plano: à mão,
  em suporte ou em tripé? *(roteiro-poses)*
- NEEDS CLARIFICATION: **Geometria do cilindro.** *(experimentos, preparacao-agua)*
  - Espessura do acrílico e distância lente–acrílico.
  - A tampa pode ser remontada 10×?
  - Como medir a inclinação dela?
- NEEDS CLARIFICATION: **Teste métrico (R7):** instrumento (trena ou laser) e ponto de referência
  na câmera. *(validacao-metrica)*
- NEEDS CLARIFICATION: **Mínimo de cantos por célula da grade 8×8.** Proposta: derivar das
  capturas existentes. *(cobertura)*
- NEEDS CLARIFICATION: **Aquecimento.** Medir o tempo até o regime térmico? Exige a regra udev do
  HID no notebook. *(aquecimento-termico)*
- NEEDS CLARIFICATION: **Disponibilidade da câmera do robô** para a calibração final.
  *(execucao-robo)*
