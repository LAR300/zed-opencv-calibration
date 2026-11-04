# Spec — M2: erro de mapeamento entre calibrações e EME empírico

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R4, M2; R5 (F × média). Base: [`referencias.md`](../../product/referencias.md) §A.4
> (Hagemann 2022: K(θ̂, θ̄) e EME). Depende de [`nucleo-offline`](../nucleo-offline/spec.md).

## Problema

Os parâmetros de uma calibração (fx, cx, distorção) são correlacionados. Duas calibrações com
números diferentes podem projetar quase igual, então comparar parâmetro a parâmetro engana. O
brief proíbe isso como critério principal (R5).

O que importa é **quanto a imagem muda**:
- o pixel previsto por A para um raio, contra o previsto por B;
- a dispersão desse erro entre as 10 capturas de uma condição, que é a incerteza empírica.

## Contexto

- **Erro de mapeamento de Hagemann:** K(θ̂, θ̄) = min_R 1/(2N_G) Σ‖u_g − p(R·p⁻¹(u_g, θ̄), θ̂)‖².
  - Ele desprojeta com uma calibração e projeta com a outra.
  - Há uma versão sem R e outra otimizando uma rotação de 3 parâmetros, que remove o que os
    extrínsecos compensariam.
  - O artigo não fixa a densidade da grade. O brief define 20 px.
- **A validação de Hagemann usa como referência "a média de dez calibrações"** (`referencias.md`
  §A.4). É o "EME empírico" do brief.
- **A lente é grande-angular** (~100° em HD720). A desprojeção precisa de um critério iterativo
  reforçado: `cv2.undistortPointsIter` existe no OpenCV 4.5.4 do container.

## Requisitos

- **R1. Erro de mapeamento A → B, por câmera:**
  1. grade de pixels a cada 20 px (configurável), cobrindo a imagem inteira, inclusive a borda;
  2. desprojeção com A por `undistortPointsIter`, com critério reforçado (configurável; padrão:
     100 iterações e 1e-12);
  3. projeção dos raios com B (`projectPoints`, sem rotação nem translação).
- **R1b. Região coberta** (acrescentado na implementação, 2026-11-03). Além da imagem inteira, o
  erro é dado só nos pontos da grade com um canto detectado a até 40 px (configurável).
  - Fora dessa região, a calibração só extrapola, e o erro mede a extrapolação.
  - **Medido em dados sintéticos com faixas laterais sem cantos:** na imagem inteira, a
    referência deu 1,52 px contra 0,83–2,12 px de cada calibração (pior que a melhor); na região
    coberta, deu 0,144 px contra 0,222–0,726 px (melhor que todas).
- **R2. Controle da desprojeção.** Cada ponto da grade é reprojetado com A. Os pontos com erro de
  ida e volta acima de 1e-3 px (configurável) são **excluídos e contados**. Isso evita que uma
  desprojeção divergente na borda da lente seja lida como diferença entre A e B.
- **R3. Saídas por par (A, B) e por câmera:**
  - RMS e máximo, nas convenções `per_point_2d` e `per_coord` (a do K de Hagemann);
  - mapa de calor do erro por ponto da grade;
  - número de pontos excluídos.
- **R4. Variante com rotação.** A mesma métrica, minimizando sobre uma rotação de 3 parâmetros
  aplicada aos raios desprojetados (`scipy.optimize.least_squares`), sem translação, como em
  Hagemann. A saída traz a rotação encontrada (graus).
- **R5. Calibração de referência ("média") de uma condição,** calculada no espaço da imagem:
  - para uma grade de raios, projeta-se com cada uma das calibrações e tira-se a média dos pixels;
  - ajusta-se um modelo `rational8` a essa projeção média, por mínimos quadrados.

  O resultado é uma calibração completa, gravável em YAML, e serve de "média" nas outras
  métricas. Também são reportadas, para comparar:
  - a média simples dos parâmetros;
  - a calibração com as imagens de todas as capturas juntas.
- **R6. EME empírico de uma condição:**
  - o erro de mapeamento de cada calibração contra a referência de R5 (10 valores, com e sem
    rotação);
  - **o EME empírico = o RMS desses 10 erros**;
  - também a matriz dos erros entre todos os pares (45 para n = 10), que alimenta o teste de
    permutação do R6 do brief ([`analise-estatistica`](../analise-estatistica/spec.md)).
- **R7. F × referência** (R5 do brief): o erro de mapeamento de F contra a referência, comparado à
  distribuição dos 10 erros da condição. Saída: o percentil de F nessa distribuição.
- **R8. Configuração** em YAML: as calibrações de entrada, a grade, os critérios e a variante. A
  saída vai para `<condicao>/metricas/m2/`.

## Condições de implementação

- **Por que a "média" é feita no espaço da imagem** (R5): K e D são correlacionados. A média
  simples dos coeficientes de distorção pode não representar nenhuma lente.
  - A média dos pixels projetados é o que a métrica compara, então a referência fica coerente com
    ela.
  - Ajustar um modelo a essa média dá uma calibração usável em todas as métricas.
  - As duas alternativas ficam no relatório para o piloto comparar.
- **O EME de Hagemann por covariância** (`trace(Σ·H)`, com Σ por bootstrap) é opcional neste spec.
  Ele pode ser calculado com o bootstrap do núcleo, mas o critério do brief é o empírico.

## Não-objetivos

- O erro de mapeamento dos extrínsecos: M3 e M4 testam R e T.
- Modelos refrativos.

## Critérios de aceitação

> Validação: ver as etapas de [`plan.md`](plan.md), registro de 2026-11-03.

- [x] Calibrações idênticas dão erro 0 (< 1e-9 px) com e sem rotação.
- [x] **fx perturbado:** com distorção zero e δ = 1e-3, o erro em cada ponto é |x − cx|·δ
      (|Δ| < 1e-6 px), e o RMS bate com o valor calculado à mão.
- [x] **cx deslocado em 1 px:** sem rotação o erro é 1 px em toda a grade. Com rotação, o RMS cai,
      e a rotação é em torno do eixo vertical, na ordem de atan(1/fx).
- [x] A desprojeção de um modelo com distorção extrema exclui e conta os pontos da borda que
      divergem.
- [x] Em 10 calibrações sintéticas com ruído, **na região coberta** (R1b), a referência fica mais
      perto do modelo verdadeiro que cada uma das 10. Na imagem inteira isso não vale quando há
      bordas sem dados; daí R1b.
- [x] Nas calibrações reais existentes (`v2`, acrílico, controles fixos) e em F, o módulo produz os
      mapas e a matriz de pares. Resultado no plano, etapa 6.

## Perguntas em aberto

Nenhuma.
