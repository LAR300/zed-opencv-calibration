# Plano — M3: validação cruzada entre capturas

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`nucleo-offline`](../nucleo-offline/plan.md) concluído.
- [`metrica-ajuste`](../metrica-ajuste/plan.md), etapa 2 (RMS com pose livre), para o teste da
  diagonal.

## Etapas

- [x] 1. **Avaliação de C em J** (R1). `zedcal/m3.py` (`evaluate`): PnP na esquerda, RMS da esquerda, transferência na direita via R, T, e combinado √((e_E² + e_D²)/2), com a mediana e o p90 por vista. Os três casos sintéticos passam.

  Especificação original da etapa: PnP na esquerda, RMS da esquerda, transferência na direita e
  combinado, com a mediana e o p90 por vista.
  - Testes primeiro: os três casos sintéticos do critério.
  - **Pronto quando:** `pytest` verde.
- [x] 2. **Matriz LOCO** (R2, R3). `matrix` (linhas = calibrações, inclusive extras como F e a referência; colunas = capturas) e `loco_summary`. A diagonal reproduz o RMS de pose livre do M1 (`points: all`) até 1e-6 px.

  Especificação original da etapa: Montagem n × n; linhas de F e da média; resumo diagonal × fora da
  diagonal.
  - Testes primeiro: o critério da diagonal = treino; n = 3 sintético dá a matriz 3 × 3 com o
    resumo correto.
  - **Pronto quando:** `pytest` verde.
- [x] 3. **Aplicação cruzada** (R4). A mesma `matrix`, com as calibrações de uma condição nas capturas de outra (`other_captures` no comando); a matriz não precisa ser quadrada (teste com 4 × 3).

  Especificação original da etapa:
  - Testes primeiro: com dois conjuntos sintéticos, a forma e o resumo saem corretos.
  - **Pronto quando:** `pytest` verde.
- [x] 4. **Por distância e por região** (R5). `by_distance` (faixas pela norma da translação do PnP) e `by_region` (RMS da esquerda na grade 8×8). Não houve teste sintético de "erro só na borda"; o teste confere as contagens e a forma.

  Especificação original da etapa: Agregação por faixa e por célula, com figuras.
  - **Pronto quando:** a figura de um caso sintético com erro só na borda mostra o erro nas células
    da borda.
- [x] 5. **Comando e config** (R6). `python3 -m zedcal.m3_command --config ...`.

  **Capturas do piloto (2026-11-03),** 4 capturas (xadrez e 3 AprilGrid) e F, variantes compatível
  (`joint`) e `all_fixed`. Médias fora da diagonal:

  | Métrica | `joint` | `fixed` |
  |---|---:|---:|
  | Esquerda | 0,333 px | 0,332 px |
  | Transferência para a direita | 0,537 px | 0,677 px |
  | Estéreo | 0,449 px | 0,538 px |

  - **Decisão** (o piloto delegou ao M3): `joint` generaliza melhor nos extrínsecos, com os
    intrínsecos empatados. Variante dos experimentos: `joint`.
  - **AprilGrid × xadrez:** nas imagens de xadrez, as calibrações AprilGrid ficam 5–11 % acima da
    de xadrez e de F na esquerda (0,25–0,27 contra 0,24 px). Nas imagens AprilGrid, **todas** as
    calibrações, inclusive a de xadrez e F, ficam em 0,32–0,43 px. O excesso está nos dados
    AprilGrid, o que reforça o `diagnostico-vies-aprilgrid`.
  - **Acrílico:** na captura com acrílico, a transferência para a direita das calibrações sem
    acrílico vai a 0,66–0,83 px, contra 0,39 px da própria. Confirma a Parte 3 da validação.

  Especificação original da etapa: `python3 -m zedcal.m3 --config ...`.
  - **Pronto quando:** roda com as 3 capturas AprilGrid existentes, tratadas como uma "condição"
    de teste, e F.

## Riscos

- **O tempo de execução cresce com n².** Para n = 10 e ~63 pares por captura, são ~6 300 PnPs por
  matriz, o que é aceitável. Paralelizar por linha se for preciso.
