# Plano — Teste métrico em distâncias conhecidas

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`roteiro-poses`](../roteiro-poses/plan.md) concluído: ele suporta outro roteiro.
- [`metricas-tarefa`](../metricas-tarefa/plan.md), etapas 1–4.
- Instrumento e ponto de referência definidos (Perguntas do spec).

## Etapas

- [ ] 1. **Roteiro métrico** (R2).
  - Testes primeiro: 36 poses, 9 por distância, ids no formato.
  - **Pronto quando:** `ctest` verde.
- [ ] 2. **Distância medida na coleta** (R3). Ao trocar de distância, a ferramenta pede o valor, o
  instrumento e a incerteza, e grava no `meta.json`.
  - **Pronto quando:** numa sessão de teste, os 9 pares de uma distância trazem o valor informado.
- [ ] 3. **Medidas do alvo na sessão** (R4). Campos de `target_measured` para as tags, os
  espaçamentos e a planicidade, validados.
  - Testes primeiro: valores ausentes ou negativos são recusados.
  - **Pronto quando:** `ctest` verde.
- [ ] 4. **Análise** (R5). `python3 -m zedcal.metric_test --config ...`: (a) erro métrico,
  (b) distância com o ajuste de d0, (c) erro epipolar, por distância.
  - Testes primeiro: o caso sintético de d0 = 25 mm e o do baseline 1 % maior.
  - **Pronto quando:** `pytest` verde.
- [ ] 5. **Relatório** (R6).
  - **Pronto quando:** numa sessão de bancada real, o relatório é gerado para F e para uma
    calibração existente.

## Riscos

- **Alinhar o alvo frontal a 3 m** é difícil à mão. A tolerância de ±2° é verificada pela pose PnP
  na coleta. A distância medida pelo laser é a do ponto de referência ao alvo, na direção
  perpendicular a ele.
