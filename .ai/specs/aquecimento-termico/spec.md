# Spec — Tempo até o regime térmico

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> "Plano de experimentos" (aquecimento até regime térmico, tempo `<X min>`). Depende de
> [`ambiente-sensores`](../ambiente-sensores/spec.md) (temperatura) e de
> [`persistencia-captura`](../persistencia-captura/spec.md) (formatos).

## Problema

O protocolo pede aquecer a câmera até o regime térmico antes de cada captura, mas o tempo `<X min>`
não é conhecido. A literatura lida não traz esse número: Shortis (2015) não trata de aquecimento
([`referencias.md`](../../product/referencias.md) §C.4).

Sem um tempo medido, as primeiras poses de cada captura podem sair com outra geometria, por
dilatação da lente e do suporte dos sensores. Isso aumenta a dispersão entre capturas e se confunde
com efeito de condição.

## Requisitos

- **R1. Registro em intervalo.** Um modo da ferramenta (`--timelapse <intervalo_s> <duracao_min>`,
  no modo sessão) grava um par a cada intervalo, com o alvo **fixo** diante da câmera **fixa**,
  desde a câmera fria.
  - Cada par vai para os formatos de `persistencia-captura`, com `pose_id = thermal`.
  - O par traz o timestamp, o uptime e as temperaturas `ONBOARD_LEFT/RIGHT/IMU`.
- **R2. Análise offline.** Com uma calibração fixa (F), para cada par:
  - **a pose do alvo na câmera esquerda,** por PnP: translação e ângulos;
  - **o erro de transferência estéreo:** pose da esquerda + R, T projetados na direita, comparados
    aos cantos da direita;
  - **o resíduo médio com sinal em x e em y,** por câmera.

  Tudo em função do tempo e da temperatura.
- **R3. Critério de regime.** O regime é atingido quando a variação de cada grandeza de R2 numa
  janela móvel (padrão 5 min) fica abaixo do ruído entre pares consecutivos. A janela e o fator
  são configuráveis. Saída: o instante de regime e a temperatura correspondente.
- **R4. Repetição.** O protocolo pede pelo menos 3 partidas a frio em dias diferentes. **X é o
  maior dos tempos medidos**, arredondado para cima ao minuto.
- **R5. Relatório.** Curvas das grandezas × tempo e × temperatura; o X proposto; o JSON com os
  números.

## Condições de implementação

- **A câmera fria** é a desligada há pelo menos o tempo configurado (padrão 2 h), com o USB
  desconectado. O protocolo de bancada registra o horário em que ela foi desligada.
- **A iluminação e a cena ficam fixas** durante o registro; os controles de captura, manuais
  (`--camera_settings`). Uma exposição automática mudaria a nitidez com o tempo.
- **Sem temperatura disponível,** a análise roda só em função do tempo, e o relatório avisa.

## Não-objetivos

- Modelar a deriva térmica para corrigi-la.
- Medir o efeito térmico debaixo d'água.

## Critérios de aceitação

> Validação: ver as etapas de [`plan.md`](plan.md), registro de 2026-11-04.

- [x] `--timelapse 30 60` grava 120 pares, com timestamps espaçados 30 ± 1 s e temperaturas no
      `meta.json` (2026-11-10: 29,94–30,07 s, temperatura em todos os pares).
- [x] Com dados sintéticos (pose com deriva exponencial de constante conhecida mais ruído), o
      critério de R3 encontra o instante de regime com erro < 1 janela — em 94,5 % das
      repetições (plan, etapa 3).
- [ ] Três registros reais a frio produzem as curvas e um X proposto.
- [ ] O X aprovado pelo usuário é copiado em [`experimentos`](../experimentos/spec.md).

## Perguntas em aberto

- NEEDS CLARIFICATION: **Duração de cada registro.** Proposta: 60 min, a ser encurtada se o regime
  vier antes.
