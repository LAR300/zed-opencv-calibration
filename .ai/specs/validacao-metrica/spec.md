# Spec — R7: teste métrico em distâncias conhecidas

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R7. Usa [`roteiro-poses`](../roteiro-poses/spec.md) (outro roteiro),
> [`persistencia-captura`](../persistencia-captura/spec.md) (sessão e medidas do alvo) e
> [`metricas-tarefa`](../metricas-tarefa/spec.md) (triangulação). Faixa do SLAM: até 3 m
> ([`vision.md`](../../product/vision.md)).

## Problema

As métricas de reprojeção e mesmo o M4 medem consistência interna. O teste métrico liga a
calibração a uma **medida física independente**: comprimentos e distâncias conhecidos, na faixa em
que o SLAM opera.

Shortis (2015) recomenda validar com comprimentos conhecidos em várias distâncias e orientações,
**inclusive maiores que a distância média da calibração** ([`referencias.md`](../../product/referencias.md)
§C.4). O "longe" do roteiro fica em ~1,1–1,4 m, e o SLAM vai até 3 m.

## Requisitos

- **R1. Sessão separada por condição** (`metrico_A`, `metrico_B`), com:
  - a câmera em distâncias conhecidas, por padrão 0,5, 1, 2 e 3 m, ajustáveis na config;
  - em cada distância, **5 pares com o alvo no centro e 4 com o alvo nas bordas** (esquerda,
    direita, topo, base): 36 pares por condição.
- **R2. Roteiro métrico** (`config/roteiro_metrico_v1.yaml`), no formato de `roteiro-poses`, com os
  ids `MET-<distância em cm>-<posição>-<n>`, ex.: `MET-300-MC-1`, `MET-100-ER-1`. O alvo fica frontal;
  a tolerância de inclinação é registrada (padrão ±2°).
- **R3. A distância medida vai para o `meta.json` de cada par:**
  - o valor em mm;
  - o instrumento;
  - o ponto de referência na câmera;
  - a incerteza declarada.

  O operador a informa antes de cada distância. A ferramenta pede o valor ao trocar de distância.
- **R4. Medidas do alvo antes do teste,** na seção `target_measured` da sessão:
  - lado de pelo menos 6 tags (das bordas e do centro) e espaçamento entre pelo menos 6 pares de
    tags, com paquímetro;
  - o lado total do alvo;
  - a planicidade em 9 pontos de uma grade 3×3: o maior desvio de um plano de referência, com
    régua e lâmina calibrada, ou laser.
- **R5. Análise:**
  - **(a)** o erro métrico do M4 (R3 daquele spec) por distância e posição, contra as medidas de
    R4;
  - **(b)** a distância do alvo pela triangulação (o plano ajustado aos cantos) contra a medida
    de R3. O deslocamento d0 entre o ponto de referência e o centro óptico é desconhecido; ele é
    **estimado como um único parâmetro comum a todas as distâncias**. O relatório traz d0, o
    resíduo por distância e o erro relativo depois de d0;
  - **(c)** o erro epipolar do M4 por distância.

  Tudo para cada calibração: as da condição, a média e F.
- **R6. Relatório** por condição em `dados/<metrico_X>/relatorio/`, alimentando
  [`analise-estatistica`](../analise-estatistica/spec.md) (TOST do erro métrico).

## Condições de implementação

- **Por que estimar d0** (R5 b): o centro de projeção fica dentro da câmera, e na água o centro de
  projeção efetivo se desloca (Treibitz 2012, `referencias.md` §C.1). Medir até ele não é possível.
  - Um d0 comum a todas as distâncias absorve só o deslocamento constante.
  - Um erro de escala da calibração aparece como resíduo proporcional à distância, que é o que se
    quer ver.
- **A 3 m, o alvo ocupa ~9 % da largura** (~118 px), com tags de ~16 px de lado. Isso fica acima do
  mínimo de 10 px do detector (`aprilgrid.hpp:67`), mas com menos precisão de canto. O relatório
  separa o erro por distância por esse motivo.

## Não-objetivos

- Validar o SLAM completo.
- Medir com instrumentos que a equipe não tem.

## Critérios de aceitação

> Validação: ver as etapas de [`plan.md`](plan.md), registro de 2026-11-03.

- [x] `config/roteiro_metrico_v1.yaml` tem 36 poses, e um teste confere ids e contagens.
- [x] **Dados sintéticos** com distâncias conhecidas e d0 = 25 mm: a análise recupera d0 com erro
      < 1 mm. Um baseline 1 % maior gera resíduo proporcional à distância.
- [ ] Na bancada, uma sessão métrica completa registra as distâncias medidas e as medidas do alvo
      no `meta.json` e na sessão.
- [ ] O relatório traz (a), (b) e (c) por distância para as calibrações da condição, a média e F.

## Perguntas em aberto

- NEEDS CLARIFICATION: **Instrumento** (trena ou laser) e **ponto de referência** na câmera ou no
  cilindro, e a incerteza de cada um.
- NEEDS CLARIFICATION: **Distância mínima.** Proposta: 0,5 m, como no brief. A faixa do SLAM tem
  limite inferior definido?
- NEEDS CLARIFICATION: **Instrumentos para a planicidade:** régua com lâmina calibrada, laser ou
  outro.
