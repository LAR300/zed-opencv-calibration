# Spec — Verificação de cobertura antes de calibrar (R3)

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R3; [`auditoria.md`](../../auditoria.md) §1.3 (o Skew é um ângulo na imagem) e §6 (risco 4).
> Depende de [`nucleo-offline`](../nucleo-offline/spec.md). Usa a definição de ângulos de
> [`roteiro-poses`](../roteiro-poses/spec.md).

## Problema

Uma calibração só é confiável onde há pontos: a distorção nas bordas e nos cantos da imagem é
extrapolada se ninguém mediu ali. A focal depende da inclinação do alvo (Parte 2 de
`validation.md`; Peng & Sturm 2019).

As métricas atuais não verificam nenhuma das duas coisas:
- **X e Y** medem a faixa de posições do canto do alvo mais próximo da borda, não a densidade de
  pontos;
- **o Skew** é um ângulo na imagem e não separa os dois eixos de inclinação
  (`calibration_checker.cpp:74-119`).

O brief pede uma verificação **depois de cada captura e antes de calibrar**, e **uma captura
reprovada é refeita**.

## Requisitos

- **R1. Densidade de cantos.** Para cada câmera, uma grade de 8×8 células sobre a imagem
  (configurável; em HD720 são células de 160×90 px) com a contagem de cantos detectados. Contam
  todas as tags da câmera, não só as comuns.
- **R2. Critério de densidade:**
  - toda célula precisa ter **pelo menos N cantos**;
  - **as 4 células dos cantos da imagem são verificadas e reportadas à parte;**
  - N é configurável.
- **R3. Inclinação.** Para cada par, a inclinação do alvo em torno do eixo horizontal e do vertical
  da câmera esquerda, pela pose PnP com a definição de `roteiro-poses`. Saída: um histograma por
  eixo.
- **R4. Critério de inclinação:** pelo menos K poses com |inclinação| > 30° em **cada um** dos dois
  eixos. Opcionalmente, exige os dois sinais em cada eixo. K e a exigência de sinal são
  configuráveis; padrão K = 2, com os dois sinais.
- **R5. Relatório por captura:**
  - **aprovada / reprovada** e o motivo: células abaixo de N, cantos da imagem, inclinação
    insuficiente em um eixo;
  - um JSON e figuras: mapa de contagem por câmera e histogramas;
  - a distribuição das frações da largura e das distâncias estimadas, só como informação.
- **R6. Código de saída.** O comando devolve 0 se a captura foi aprovada e outro valor se foi
  reprovada, para uso em script.
- **R7. Calibração para a pose.** A inclinação usa a calibração indicada na config: padrão F. A
  própria calibração da captura também pode ser usada. A ordem da regra "verificar antes de
  calibrar" é garantida pelo uso de F.

## Condições de implementação

- **A contagem usa os cantos refinados de `detections.json`,** os mesmos da calibração.
- **A pose PnP usa o alvo inteiro visível,** com IPPE + LM, como no núcleo. As inclinações seguem a
  definição única de `roteiro-poses`. Um teste compara as duas implementações (C++ e Python) na
  mesma pose sintética.

## Não-objetivos

- Corrigir a cobertura durante a coleta: isso é o papel do roteiro.
- Avaliar a qualidade dos cantos (isso é o M1).

## Critérios de aceitação

- [ ] **Dados sintéticos:** uma captura com todas as células acima de N e inclinações > 30° nos dois
      eixos, com os dois sinais, é aprovada.
- [ ] Removendo os pares que cobrem o canto superior direito, a captura é reprovada com o motivo
      "célula de canto (0,7) abaixo de N".
- [ ] Removendo as poses inclinadas em torno do eixo vertical, a captura é reprovada com o motivo
      "inclinação vertical insuficiente".
- [ ] As inclinações calculadas em Python coincidem com as do `common/pose_script/` C++ até 0,1°
      numa pose sintética.
- [ ] Nas 3 capturas AprilGrid existentes, o comando gera os mapas e os histogramas. Esses
      números servem para propor N.

## Perguntas em aberto

- NEEDS CLARIFICATION: **Valor de N** (mínimo de cantos por célula). Proposta: o piloto mostra a
  distribuição das contagens nas capturas existentes, e o usuário escolhe N a partir dela. A
  literatura lida não fixa esse número.
