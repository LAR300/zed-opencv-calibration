# Decisões — Limites de amostras configuráveis e encerramento manual

> **Por que as escolhas técnicas foram feitas.** Estilo ADR leve. Durável: quando uma decisão
> mudar, registre a nova com o motivo.

### D1. O `main.cpp` decide sobre o máximo; o `CalibrationChecker` só avalia as métricas

- **Contexto:** `CalibrationChecker::evaluateSampleCollectionStatus` devolve `true` tanto quando as
  métricas completam quanto quando chega a `max_samples_`, sem dizer qual dos dois. Com
  `max_samples_ = 0` a condição `paramDb_.size() >= max_samples_` seria verdadeira desde a primeira
  amostra. A pausa (R4) e o "continuar sem novo máximo" (R5) precisam saber o motivo e mudar o
  máximo durante a coleta.
- **Decisão:** o `CalibrationChecker` é construído com o máximo **ilimitado**
  (`std::numeric_limits<size_t>::max()`), e então só devolve `true` quando as métricas completam com
  pelo menos `--min_samples`. O `main.cpp` compara a contagem com `--max_samples` e decide entre
  pausar, continuar ou calibrar.
- **Por quê:** não muda o `CalibrationChecker`, que foi refatorado e validado na spec
  `aprilgrid-target`. A regra "métricas completas com pelo menos o mínimo" continua num lugar só.
  - *Descartado:* um `setMaxSamples()` e um motivo de retorno no checker. Funciona, mas mexe numa
    classe que não precisa mudar.
- **Consequências:** a mensagem "Reached the maximum sample count" do checker deixa de aparecer. O
  `main.cpp` passa a imprimir a sua, com as métricas incompletas (R4) e o registro final (R9).

### D2. Regras puras num header testável

- **Contexto:** a lógica nova vive no laço de interface do `main.cpp`, que depende de câmera e janela
  e não tem teste. As regras em si são simples e erram fácil: os limites válidos, o que fazer em
  cada contagem e quando o `f` é aceito.
- **Decisão:** criar `stereo_calibration/include/sample_limits.hpp`, só com header e sem OpenCV nem
  SDK, com:
  - a validação de `--min_samples` e `--max_samples` (R2);
  - a decisão da coleta depois de cada amostra aceita: continuar, pausar no máximo ou terminar pelas
    métricas;
  - a regra do `f` (R7).

  O `main.cpp` chama essas funções, e os testes de `tests/` as cobrem.
- **Por quê:** TDD no que pode ser testado sem câmera, no mesmo padrão do `common/aprilgrid` (D10 da
  spec anterior). O laço de interface fica só com a parte visual.
  - *Descartado:* deixar tudo no `main.cpp`, o que só teria verificação manual.
- **Consequências:** `tests/CMakeLists.txt` passa a incluir `stereo_calibration/include`.

### D3. Durante a pausa, `s` retoma sem capturar

- **Contexto:** `s` e espaço são as teclas de captura. Na pausa, R5 diz que `s` continua a coleta.
- **Decisão:** na pausa, `s` ou espaço **só retoma** a coleta, sem salvar o quadro daquele momento. A
  próxima captura exige um novo `s`.
- **Por quê:** quem estava esperando a pausa raramente está com o alvo numa pose nova. Capturar
  junto com o retomar tenderia a gerar uma amostra recusada como "parecida demais", ou uma ruim.
- **Consequências:** a mensagem da pausa explica isso.

### D4. Números inválidos nas flags novas geram erro tratado

- **Contexto:** as flags numéricas atuais (`--h_edges`, `--min_tags`, …) usam `std::stoi` sem
  tratamento, e um valor não numérico encerra o programa com uma exceção não capturada. A spec pede
  mensagem clara para as flags novas.
- **Decisão:** `--min_samples` e `--max_samples` são lidos com tratamento de erro: valor não numérico
  ou com sobra (por exemplo `25x`) gera mensagem e código de saída 1. As flags existentes não
  mudam.
- **Por quê:** cumpre a spec sem ampliar o escopo para o parse inteiro.
- **Consequências:** o tratamento das flags fica inconsistente entre as antigas e as novas. Fica como
  melhoria possível, fora desta spec.
