# Spec — M4: métricas de tarefa (epipolar, erro métrico e resíduo de plano)

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R4, M4; R5 (a diferença de cx após a retificação). Depende de
> [`nucleo-offline`](../nucleo-offline/spec.md).

## Problema

O SLAM usa a calibração de duas formas, e as métricas de reprojeção não medem nenhuma delas
diretamente:
- **retificar o par** para a correspondência estéreo, em que um erro vertical residual atrapalha a
  busca na linha;
- **triangular,** em que um erro de focal, baseline ou viés de disparidade vira erro de escala e de
  profundidade.

## Contexto

- **Limiares do brief para o erro epipolar vertical:** aceitável < 0,25 px; excelente < 0,1 px.
- **Sensibilidade da profundidade:**
  - ΔZ/Z ≈ Δf/f + ΔB/B;
  - para um viés de disparidade, ΔZ/Z ≈ Δd·Z/(f·B) (brief, R6).

  Com f ≈ 534 px e B ≈ 120 mm, f·B ≈ 64 080 px·mm. Um Δd de 0,1 px dá 0,47 % a 3 m.
- **As medidas reais do alvo** (tag e espaçamento, com paquímetro) vêm da sessão
  (`target_measured`, [`persistencia-captura`](../persistencia-captura/spec.md) R2).

## Requisitos

- **R1. Retificação de cada calibração:** `stereoRectify` com `alpha` configurável (padrão 0) e
  **sem** `CALIB_ZERO_DISPARITY`, para que a diferença de cx apareça (R5).
  - Os cantos das tags comuns são levados ao espaço retificado por `undistortPoints` com R1/P1 e
    R2/P2, com o critério iterativo reforçado.
  - Não há remapeamento de imagem.
- **R2. Erro epipolar vertical:** |y_E − y_D| por canto comum retificado.
  - **Saídas:** RMS, mediana, p90 e máximo.
  - **Contagens:** a fração abaixo de 0,25 px e a abaixo de 0,1 px.
  - **Mapas:** por região da imagem (grade 8×8) e por faixa de distância.
- **R3. Erro métrico de triangulação.** Os cantos comuns são triangulados com P1 e P2 (mm). Em
  cada vista são comparados três conjuntos de distâncias com as distâncias reais (medidas, não
  nominais):
  - **(a)** os 4 lados de cada tag;
  - **(b)** os cantos homólogos de tags vizinhas, na linha e na coluna, que dão o passo do alvo;
  - **(c)** as maiores distâncias disponíveis na vista, entre os cantos visíveis mais afastados.

  **Saídas:** o erro relativo (%) por conjunto, por faixa de distância e por região.
- **R4. Resíduo de plano.** Plano ajustado aos cantos triangulados de cada vista (SVD). Saídas: o
  RMS e o máximo da distância ortogonal (mm), também relativos à distância do alvo.
- **R5. Diferença de cx retificado:**
  - por calibração, cx_E − cx_D das matrizes P1 e P2;
  - entre duas calibrações, a diferença desse valor (Δd, em px) e o ΔZ/Z correspondente a 0,5,
    1, 2 e 3 m, pela fórmula do Contexto.
- **R6. Avaliação:** cada calibração (as da condição, a média e F) avaliada nas mesmas capturas de
  validação: capturas de outra repetição e o teste métrico
  ([`validacao-metrica`](../validacao-metrica/spec.md)). As capturas de validação ficam na config.
- **R7. Configuração** em YAML; a saída vai para `<condicao>/metricas/m4/`.

## Condições de implementação

- **A retificação é feita nos pontos, não nas imagens.** As métricas só precisam dos cantos. É
  mais rápido e não introduz o erro de interpolação de `remap`.
- **Sem `CALIB_ZERO_DISPARITY`** (R1): com essa flag, cx_E = cx_D por construção, e o viés de
  disparidade entre calibrações (R5) ficaria escondido.
- **As distâncias reais usam `target_measured`,** não as nominais do `target.yaml`. A Parte 1
  (§3) de `validation.md` mostrou que um erro de 6,8 % na escala do alvo só aparece no baseline e
  no checker estéreo.

## Não-objetivos

- Medir a precisão do SLAM ou do matcher.
- Retificar imagens.

## Critérios de aceitação

- [ ] **Dados sintéticos com a calibração verdadeira** e sem ruído:
  - erro epipolar < 1e-6 px;
  - erro métrico < 1e-6 %;
  - resíduo de plano < 1e-6 mm.
- [ ] **Uma rotação de 0,05° em torno do eixo óptico** imposta entre as câmeras na calibração
      avaliada produz um erro epipolar que cresce com a distância ao centro da imagem, com o
      sinal certo.
- [ ] **O baseline 1 % errado** na calibração avaliada dá erro métrico ≈ 1 % em (b) e (c).
- [ ] **Δd conhecido:** duas calibrações que diferem só em cx_D por 0,5 px dão Δd = 0,5 px em R5,
      e o ΔZ/Z bate com a fórmula.
- [ ] Nas capturas AprilGrid existentes, com F e com as calibrações existentes, os mapas e as
      tabelas são gerados.

## Perguntas em aberto

Nenhuma.
