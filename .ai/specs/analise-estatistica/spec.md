# Spec — R6: estatística por condição e comparação entre condições

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R6 e R5. Consome as saídas de [`metrica-ajuste`](../metrica-ajuste/spec.md) (M1),
> [`erro-mapeamento`](../erro-mapeamento/spec.md) (M2),
> [`validacao-cruzada`](../validacao-cruzada/spec.md) (M3),
> [`metricas-tarefa`](../metricas-tarefa/spec.md) (M4) e
> [`validacao-metrica`](../validacao-metrica/spec.md) (R7).

## Problema

Com 10 capturas por condição, as perguntas do protocolo precisam de respostas estatísticas
definidas **antes** dos dados:
- quanto varia uma calibração entre capturas;
- se o acrílico muda a calibração, ou só a sua dispersão;
- se a nossa calibração é equivalente ou melhor que a de fábrica.

## Requisitos

### Por condição
- **R1. Estatística das grandezas:** fx, fy, cx e cy de cada câmera, baseline, os três ângulos de R,
  fy/fx, os RMS do M1, BR, ε_bias, o EME empírico, os resumos do M3 e as métricas do M4. Para cada
  uma:
  - **média;**
  - **IC95 % pela t de Student** (n − 1 graus de liberdade; n = 10 dá t₀,₉₇₅ = 2,262);
  - **coeficiente de variação (CV),** desvio-padrão / |média|.

  **Não se reporta variância**, como pede o brief. O CV de grandezas perto de zero (ângulos, TY,
  TZ) é marcado como "não informativo", e para elas o relatório traz o desvio-padrão na unidade
  da grandeza.
- **R2. F na tabela:** o valor de F ao lado de cada grandeza, com a indicação de que ele está
  dentro ou fora do IC95 % das 10 capturas e o percentil dele na distribuição. É informativo: o
  brief proíbe a comparação parâmetro a parâmetro como critério principal (R5).

### Comparação entre duas condições A × B
- **R3. Permutação dos erros de mapeamento.** Com a matriz de erros de mapeamento entre as 20
  calibrações (M2):
  - **estatística:** média dos 100 pares A–B menos a média dos 45 pares A–A, como pede o brief. A
    variante com os 45 + 45 pares dentro das condições também é reportada;
  - **teste:** permutação dos **rótulos** das 20 calibrações, exata por enumeração.
    - A estatística principal distingue A de B, então são as C(20,10) = 184 756 atribuições.
    - A variante simétrica usa as 92 378 divisões distintas.
    - Com a enumeração completa, a menor p possível é 1/184 756.
  - **saídas:** valor p unilateral (A–B > A–A) e a distribuição nula.
- **R4. Aplicação cruzada:** as matrizes do M3 (R4 daquele spec) e o M4 com as calibrações de A nas
  imagens de B e vice-versa. Saída: a diferença entre "aplicado na própria condição" e "aplicado
  na outra", com IC95 %.
- **R5. Equivalência (TOST)** em fx, fy, baseline e erro métrico (M4 e R7):
  - dois testes t unilaterais de Welch sobre μ_B − μ_A, com margem ±Δ por grandeza;
  - equivalência quando o IC90 % da diferença fica dentro de ±Δ;
  - as margens vêm de um arquivo de configuração (ver Perguntas).
- **R6. Dispersões:** Levene na versão de Brown–Forsythe (centrado na mediana) e o teste F da razão
  de variâncias, unilateral (B mais disperso que A). Grandezas: as de R5, mais o EME empírico.
- **R7. Comparações múltiplas:** o relatório traz os valores p brutos e com o ajuste de Holm, este
  como informação. Os critérios dos portões usam o que [`experimentos`](../experimentos/spec.md)
  definir.

### Relatório
- **R8.** Um relatório por condição e um por comparação, em Markdown com figuras PNG, em
  `dados/relatorios/<nome>/`:
  - tabelas de R1 e R2;
  - para cada teste: estatística, valor p, IC e decisão;
  - figuras: dispersão dos parâmetros com IC e F, distribuição nula da permutação, matrizes M3,
    mapas M2 e M4;
  - a config e os commits usados.

## Condições de implementação

- **Por que permutar os rótulos, e não os 145 erros:** os erros de mapeamento entre pares não são
  independentes, porque cada calibração entra em vários pares. Permutar as calibrações preserva
  essa dependência; é o mesmo princípio dos testes de Mantel e PERMANOVA. Com 20 calibrações, a
  enumeração exata é barata.
- **Welch, não Student agrupado:** o protocolo espera que B possa ser mais disperso que A (R6).
- **SciPy 1.15:** `stats.t`, `stats.levene(center='median')`, `stats.f`. A permutação e o TOST são
  implementados no pacote, com testes contra casos de resultado conhecido.

## Margens do TOST (proposta para aprovação)

A sensibilidade da profundidade (brief R6) com f ≈ 534 px e B ≈ 120 mm (f·B ≈ 64 080 px·mm) dá o
viés de disparidade Δd que corresponde a um erro ΔZ/Z em cada distância:

| ΔZ/Z | Z = 0,5 m | 1 m | 2 m | 3 m |
|---|---:|---:|---:|---:|
| 0,5 % | 0,64 px | 0,32 px | 0,16 px | 0,11 px |
| 1 % | 1,28 px | 0,64 px | 0,32 px | 0,21 px |
| 2 % | 2,56 px | 1,28 px | 0,64 px | 0,43 px |

Como a faixa do SLAM vai até 3 m, a coluna de 3 m é a mais exigente.

**Proposta:**
- **orçamento de erro de profundidade** devido à calibração ΔZ/Z ≤ 1 % a 3 m, dividido igualmente
  entre focal e baseline (ΔZ/Z ≈ Δf/f + ΔB/B);
- **margens:**
  - Δf/f = ±0,5 % (fx: ±2,7 px; fy: ±2,7 px);
  - ΔB/B = ±0,5 % (±0,6 mm);
  - erro métrico: ±0,5 ponto percentual;
- **viés de disparidade entre calibrações** (M4 R5): ≤ 0,21 px.

Essas margens **não estão fixadas**. Elas só valem depois da aprovação do usuário, conforme pede o
brief.

## Não-objetivos

- Modelos hierárquicos ou bayesianos.
- Escolher o orçamento de erro do SLAM: isso é decisão do usuário.

## Critérios de aceitação

> Validação: ver as etapas de [`plan.md`](plan.md), registro de 2026-11-03.

- [x] **IC95 %:** num conjunto conhecido (n = 10), o intervalo bate com o calculado à mão.
- [x] **Permutação:**
  - dois grupos sintéticos da mesma distribuição dão p uniforme em 200 repetições (teste de
    Kolmogorov–Smirnov, p > 0,01);
  - grupos com deslocamento claro dão p < 0,01;
  - a enumeração visita exatamente 184 756 atribuições (92 378 na variante simétrica).
- [x] **TOST:** médias iguais com n = 10 e σ pequeno diante de Δ declaram equivalência; uma diferença
      maior que Δ não declara.
- [x] **Levene e F:** o grupo B com o dobro do desvio-padrão é detectado — pelo F com n = 10; pelo
      Brown–Forsythe só com n = 20 (poder simulado no plano, etapa 5).
- [x] Um relatório completo é gerado para duas "condições" sintéticas de 10 calibrações cada.

## Perguntas em aberto

- NEEDS CLARIFICATION: **Aprovar ou ajustar o orçamento de erro e as margens** da seção acima.
- NEEDS CLARIFICATION: **Nível de significância** dos portões. Proposta: α = 0,05, com o TOST pelo
  IC90 %.
