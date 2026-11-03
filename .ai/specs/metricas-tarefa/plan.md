# Plano — M4: métricas de tarefa

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`nucleo-offline`](../nucleo-offline/plan.md) concluído.
- A sessão com `target_measured` (`persistencia-captura`). Para as capturas antigas, as medidas vêm
  de `validation.md` (tag de 88 mm, espaço de 26,4 mm) numa config.

## Etapas

- [ ] 1. **Retificação nos pontos** (R1). `stereoRectify` sem zero disparity; cantos comuns
  retificados com o critério reforçado.
  - Testes primeiro: num par sintético ideal (câmeras paralelas), os pontos retificados coincidem
    com a projeção pinhole retificada.
  - **Pronto quando:** `pytest` verde.
- [ ] 2. **Erro epipolar** (R2). Estatísticas, frações abaixo dos limiares e mapas.
  - Testes primeiro: o caso "sem ruído" e o caso da rotação de 0,05°.
  - **Pronto quando:** `pytest` verde.
- [ ] 3. **Triangulação e erro métrico** (R3). Os conjuntos (a), (b) e (c) com as distâncias
  medidas.
  - Testes primeiro: o caso sem ruído e o baseline 1 % errado; uma vista com só 2 tags gera os
    conjuntos possíveis sem erro.
  - **Pronto quando:** `pytest` verde.
- [ ] 4. **Resíduo de plano** (R4).
  - Testes primeiro: o caso plano dá ≈ 0; uma flecha sintética de 1 mm dá um resíduo compatível com
    ela.
  - **Pronto quando:** `pytest` verde.
- [ ] 5. **cx retificado e Δd** (R5).
  - Testes primeiro: o caso de Δd = 0,5 px.
  - **Pronto quando:** `pytest` verde.
- [ ] 6. **Comando, agregação e figuras** (R6, R7). `python3 -m zedcal.m4 --config ...`.
  - **Pronto quando:** roda nas capturas existentes com F e as calibrações existentes.

## Riscos

- **A triangulação a 1,4 m com pouco paralaxe é ruidosa.** O erro métrico dos lados das tags (a)
  fica dominado por ruído a grande distância. Por isso os três conjuntos são separados e as
  métricas são reportadas por faixa de distância.
