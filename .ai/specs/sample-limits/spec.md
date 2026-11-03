# Spec — Limites de amostras configuráveis e encerramento manual da coleta

> **O quê e por quê.** Fonte da verdade da intenção. **Sem decisão técnica nem passos de
> implementação** (esses vão em `decisions.md` e `plan.md`).

## Problema

A coleta de `zed_stereo_calibration` usa limites fixos no código, de 25 e 35 amostras, e termina
**sozinha e sem aviso** quando chega a 35, mesmo com métricas abaixo de 100 %. Na primeira
calibração AprilGrid (`calib_runs/2026-10-20_ar_aprilgrid/`) isso aconteceu com o Skew incompleto,
e a focal saiu 0,7 % errada com um RMS de 0,32 px, que parecia bom. Só a recaptura, terminada pelas
métricas, passou nos critérios. Ver
[`aprilgrid-target/validation.md`](../aprilgrid-target/validation.md).

Também não há como **encerrar a coleta quando o usuário decide**: antes do máximo, ela só termina
quando as quatro métricas completam.

## Contexto

- `stereo_calibration/src/main.cpp` define `min_samples = 25` e `max_samples = 35` como constantes.
  `CalibrationChecker::evaluateSampleCollectionStatus` encerra a coleta quando há pelo menos
  `min_samples` amostras e X, Y, Size e Skew estão todos em 100 %, **ou** quando chega a
  `max_samples`.
- `calibrate()` recusa calcular com menos de `MIN_IMAGE = 20` amostras válidas.
- Teclas já usadas na janela: `s` ou espaço para capturar, `q` ou ESC para abortar sem calibrar.
- O painel mostra X, Y, Size e Skew com MIN_VAL, MAX_VAL, COVERAGE, REQUIRED e SCORE, e a contagem
  "Sample saved: N [min. 25, max. 35]".
- Vale igual para xadrez e AprilGrid (`--target_yaml`).
- Algumas métricas podem ser difíceis de completar: o Skew exige inclinações em que o AprilGrid
  perde tags, e o Size depende do tamanho do alvo em relação ao campo de visão. Por isso um limite
  continua útil.

## Requisitos

### Limites
- **R1.** `--min_samples <n>` e `--max_samples <n>` definem os limites da coleta. Os padrões
  continuam 25 e 35. `--max_samples 0` significa **sem máximo**.
- **R2.** Valores inválidos geram erro claro e encerram a ferramenta antes de abrir a câmera, com
  código de saída diferente de zero: `--min_samples` menor que 20 (o mínimo que a calibração
  aceita) ou `--max_samples` diferente de 0 e menor que `--min_samples`.
- **R3.** Os limites em uso aparecem no início da execução e no painel ("Sample saved: N [min. …,
  max. …]"; "sem máximo" quando for 0), e estão no `--help`.

### Máximo atingido com métricas incompletas
- **R4.** Ao chegar a `--max_samples` com alguma métrica abaixo de 100 %, a coleta **pausa**, sem
  calibrar, e mostra na janela e no terminal **quais métricas estão incompletas e com quanto**. A
  imagem ao vivo continua aparecendo durante a pausa, com a mensagem sobreposta, para o usuário já
  posicionar o alvo antes de continuar.
- **R5.** Durante a pausa o usuário escolhe:
  - **`f`:** calibrar com as amostras coletadas;
  - **`s` ou espaço:** continuar coletando. A partir daí não há novo máximo; a coleta segue até as
    métricas completarem ou até o usuário encerrar com `f`;
  - **`q` ou ESC:** abortar, como hoje.
- **R6.** Com todas as métricas em 100 % e pelo menos `--min_samples` amostras, a coleta termina
  sozinha e calibra, como hoje.

### Encerramento manual
- **R7.** A tecla **`f`** encerra a coleta e calibra a qualquer momento, desde que haja **pelo menos
  20 amostras**:
  - com menos de 20, mostra uma mensagem dizendo quantas faltam e a coleta continua;
  - com 20 ou mais, mas abaixo de `--min_samples` ou com métricas incompletas, encerra **com aviso**
    listando o que ficou abaixo do pedido.
- **R8.** A instrução na tela menciona a tecla `f` junto das de captura.

### Rastreabilidade
- **R9.** Ao fim da coleta, o terminal registra **como ela terminou** (métricas completas, máximo
  atingido seguido da escolha do usuário, ou encerramento manual), o número de amostras e o score
  final de cada métrica. Assim o `calib.log` (`| tee`) documenta a coleta.

### Funcionalidades que se mantêm
- **R10.** Sem as novas opções, o comportamento é o de hoje, exceto pelo aviso e pela pausa de R4,
  que substituem o encerramento silencioso.
- **R11.** O `--use_stored_values` continua usando todas as imagens salvas. As novas opções não têm
  efeito nesse modo, e isso aparece numa mensagem se elas forem passadas.
- **R12.** O código segue o estilo do repositório (C++17, parse de argumentos no padrão atual,
  mensagens em inglês), e o `README.md` documenta as opções, a tecla `f` e a pausa.

## Não-objetivos

- Mudar os limites ideais das métricas (REQUIRED) ou a forma de calculá-las.
- Mudar o `MIN_IMAGE = 20` da calibração.
- Ferramentas mono e o checker.
- Clique do mouse ou controle remoto (passador de slides) para capturar. Foi discutido e fica para
  outra spec.

## Critérios de aceitação

- [ ] **Sem opções,** uma coleta que completa as métricas antes de 35 amostras termina sozinha e
      calibra, como hoje.
- [ ] **Com `--max_samples 26`** e métricas incompletas, a coleta pausa na 26ª amostra e mostra as
      métricas que faltam. `s` continua a coleta e `f` calibra.
- [ ] **Com `--max_samples 0`,** a coleta passa de 35 amostras e termina só pelas métricas ou por `f`.
- [ ] **A tecla `f`:** com menos de 20 amostras mostra a mensagem e continua; com 20 ou mais calibra,
      com o aviso de métricas incompletas quando for o caso.
- [ ] **Erros:** `--min_samples 10`, `--max_samples 15 --min_samples 25` e valores não numéricos
      encerram com mensagem clara e código diferente de zero, sem abrir a câmera.
- [ ] **O `calib.log`** registra como a coleta terminou, o número de amostras e os scores finais.
- [ ] **Funciona com xadrez e com AprilGrid.**
- [ ] **Sem regressão:** o recálculo `--use_stored_values` das imagens de xadrez de 16/10 continua
      idêntico ao `baseline.yml`.
- [ ] **O README** documenta as opções, a tecla `f` e a pausa.

## Perguntas em aberto

Nenhuma. A imagem ao vivo durante a pausa ficou decidida em R4.
