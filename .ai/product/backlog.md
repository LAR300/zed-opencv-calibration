# Backlog — Metodologia de calibração estéreo da ZED 2i para SLAM subaquático

> Features candidatas, **priorizadas**. Cada item tem um `<slug>` que vira `.ai/specs/<slug>/`
> quando você roda `/ai_dev_kit:plan <slug>`. Marque `[x]` quando a feature for entregue.
>
> **Valor:** alto / médio / baixo · **Esforço:** P / M / G · **MVP:** faz parte do primeiro corte?
>
> Contexto: [`vision.md`](vision.md) (perguntas em aberto ao final) e
> [`referencias.md`](referencias.md). R1–R8 e M1–M4 são os requisitos do brief metodológico
> ([`brief.md`](brief.md)), resumidos em [Requisitos de origem](#requisitos-de-origem).
>
> **Specs (2026-11-03):** todos os itens de E1–E5 têm `spec.md` e `plan.md` em `../specs/<slug>/`,
> exceto as execuções (`execucao-*`), que seguem o spec `experimentos`. Os itens de E6 ficam para
> depois.
>
> **Rascunho.** O backlog foi gerado junto com a visão, a pedido do usuário. Revisto em 2026-11-03
> com os achados de [`../auditoria.md`](../auditoria.md) (§8); os riscos citados como "risco N"
> são os da §6 da auditoria.

## Sequência e portões

```
Fase 0  auditoria ─────────────────────────────────────────────── PARE: revisão do usuário
Fase 1  specs de todos os módulos + experimentos ──────────────── PARE: revisão do usuário
Fase 2  implementação, um módulo por vez, com testes
        ├─ base de dados:  ambiente-sensores · persistencia-captura · exportar-fabrica · saida-calibracao
        ├─ métricas:       nucleo-offline → metrica-ajuste → erro-mapeamento
        ├─ piloto:         piloto-dados-existentes ─────────────── confere métricas e fy/fx
        ├─ captura:        roteiro-poses · captura-estatica · cobertura · aquecimento-termico
        ├─ validação:      validacao-cruzada · metricas-tarefa · analise-estatistica · validacao-metrica
        └─ (paralelo)      higiene-ferramentas
Fase 3  execucao-a ─────────────── portão A: nossa calibração empata ou supera F na validação?
        execucao-b ─────────────── portão B: B equivale a A (R6)?
        execucao-b-linha ───────── só se B falhar: adota a referência de B
        execucao-robo ──────────── mesmo protocolo na câmera do robô (outro serial)
Fase 4  (futuro) preparacao-agua → condições C1/C2
```

Por que esta ordem:
- **A base de dados vem primeiro.** Todas as métricas dependem dos dados salvos (R1) e do arquivo F.
- **As métricas vêm antes do roteiro.** Assim elas são validadas em dados que já existem (o
  piloto) antes de investir em 10 capturas por condição.
- **O piloto pode mudar o plano.** Se o Bias Ratio der alto nas calibrações AprilGrid atuais, o
  problema de detecção (fy/fx) precisa ser resolvido antes das capturas A, senão as 10 capturas
  herdam o viés.
- **O roteiro e a cobertura vêm antes de qualquer execução.** Sem eles, as capturas não são
  comparáveis entre si.

## Épicos

| Épico | Onde | Features |
|---|---|---|
| **E0. Diagnóstico** | `.ai/` | auditoria |
| **E1. Captura rastreável** | C++, `stereo_calibration`, `stereo_checker`, `common/` | ambiente-sensores, persistencia-captura, exportar-fabrica, saida-calibracao, roteiro-poses, captura-estatica, aquecimento-termico, higiene-ferramentas |
| **E2. Verificação de cobertura** | Python offline | cobertura |
| **E3. Métricas** | Python offline (`analysis/`) | nucleo-offline, metrica-ajuste, erro-mapeamento, validacao-cruzada, metricas-tarefa, piloto-dados-existentes |
| **E4. Validação e análise** | Python offline + bancada | validacao-metrica, analise-estatistica |
| **E5. Experimentos** | `.ai/specs/experimentos/` + `calib_runs/dados/` | experimentos, execucao-a, execucao-b, execucao-b-linha, execucao-robo |
| **E6. Água (futuro)** | — | preparacao-agua, condicoes-agua |

## Itens (ordenados por prioridade)

- [x] **auditoria** — Auditoria do repositório contra o brief (Fase 1)
  - Valor: alto · Esforço: P · MVP: sim
  - Depende de: —
  - Entregue em 2026-11-03: [`../auditoria.md`](../auditoria.md), revisada pelo usuário. Cobre a
    captura e os dados, a detecção AprilGrid, a calibração, o formato de saída e a avaliação
    existente. Fecha com as lacunas (§5), os riscos (§6) e a divisão entre C++ e offline (§7).
  - Resolvido nela: o RMS do OpenCV 4.5.4 é **por canto** (norma 2D), então MSE por coordenada =
    RMS²/2. A autocalibração fica ativa só no `stereo_reprojection_viewer`. As ferramentas mono são
    para ZED X One.

- [ ] **ambiente-sensores** — Acesso permanente ao ZED SDK e ao HID da câmera
  - Valor: alto · Esforço: P · MVP: sim
  - Depende de: —
  - O `admin` precisa entrar no grupo `zed` pelo `entrypoint_additions/`, e o host (o notebook da bancada) precisa de uma
    regra udev para `idVendor 2b03`. Sem o HID não há temperatura do sensor, que o R1 exige no
    `meta.json`, nem IMU.
  - **A mudança é no repositório `isaac_ros`, não neste.** Spec curta, registrada aqui por ser
    pré-requisito.

- [x] **persistencia-captura** — Pares brutos, detecções e metadados por captura (R1, R8)
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: auditoria, ambiente-sensores (para a temperatura)
  - **Estrutura:** `dados/<condicao>/<captura_XX>/{left,right}/<pose_id>_<n>.png`, mais
    `detections.json` e `meta.json`. Nada é apagado; nada é gravado no diretório corrente
    (risco 8).
  - **`detections.json` é por câmera, com todas as tags detectadas,** e não só as comuns às duas.
    Hoje a calibração de cada câmera usa só as tags comuns (`opencv_calibration.cpp:98`), o que
    empobrece as bordas da imagem (risco 4). Cada tag leva os cantos com e sem o refino.
  - **Campos do `meta.json`:**
    - câmera: serial, resolução, lente, versão do SDK e do firmware, temperatura `ONBOARD_L/R`;
    - captura: condição, id da captura, ids das poses, timestamp, tempo desde que o sistema
      ligou;
    - código: commit;
    - reproduzibilidade: controles de captura (`--camera_settings`) e parâmetros do detector;
    - medidas do alvo;
    - porta: distância lente–acrílico, espessura, inclinação, índice do acrílico;
    - água: temperatura e salinidade.
  - **Recálculo exporta as detecções.** O `--use_stored_values` também grava `detections.json`.
    Assim as capturas antigas de `calib_runs/` entram nas métricas com o mesmo detector C++.
  - **Sem a flag nova, o fluxo e o `zed-images/` continuam como hoje.**

- [x] **exportar-fabrica** — Calibração de fábrica "F" no formato OpenCV (R5)
  - Valor: alto · Esforço: P · MVP: sim
  - Depende de: auditoria
  - Exporta `calibration_parameters_raw` do SDK, com a autocalibração desligada e na mesma
    resolução, para o mesmo YAML que a ferramenta escreve. A partir daí, F entra em todas as
    métricas como mais uma calibração.
  - **Teste:** o checker com F exportado deve reproduzir o checker com a calibração interna.

- [x] **saida-calibracao** — Calibração do zero e relatório fiel na ferramenta C++
  - Valor: alto · Esforço: P · MVP: sim
  - Depende de: auditoria
  - **Calibração do zero por padrão no modo novo.** Hoje a captura ao vivo usa a fábrica como
    chute inicial (`CALIB_USE_INTRINSIC_GUESS`, `opencv_calibration.cpp:159`), e o recálculo parte
    do zero. Comparar com F exige independência (risco 2). O prior fica opcional e registrado.
  - **RMS por câmera calculado com os intrínsecos finais.** Hoje "Left/Right" é o da etapa mono,
    mas K e D salvos vêm do `stereoCalibrate` (risco 3).
  - **Erro por vista e desvios-padrão** (versões Extended), num arquivo ao lado do YAML.
  - **Metadados num arquivo ao lado do YAML.** O YAML lido pelo SDK não muda.
  - **Regressão:** sem as flags novas, o recálculo de `calib_runs/regress_chess/` dá o mesmo
    resultado (`.ai/docs/bench.md`).

- [x] **nucleo-offline** — Pacote Python: leitura dos dados, calibração offline e dados sintéticos
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: persistencia-captura
  - Faz quatro coisas:
    - lê `detections.json`, `meta.json` e os YAML do ZED, e escreve YAML no formato do SDK;
    - recalibra a partir das detecções com vários modelos (`calib_<modelo>.yml`: radtan 5,
      racional 8, fisheye);
    - gera alvos e câmeras sintéticos, com ruído e viés controlados, para os testes;
    - lê a configuração de cada análise de um YAML.
  - **Não detecta cantos em Python.** Só lê os `detections.json` produzidos pelo detector C++. Os
    scripts de `calib_runs/tools/` detectam em Python, sem o refino D11, e não devem ser reusados
    (risco 6).
  - **Calibração por câmera com todas as tags daquela câmera;** estéreo com as tags comuns. Usa as
    versões Extended (erro por vista e desvios-padrão).
  - **Convenção do RMS:** o RMS do OpenCV é por canto. O pacote expõe o MSE por coordenada
    explicitamente, para o Bias Ratio (risco 5).
  - **Teste:** a recalibração offline reproduz o resultado C++ com as mesmas detecções.

- [x] **metrica-ajuste** — M1: RMS, mapa de resíduos e Bias Ratio
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: nucleo-offline
  - **Bias Ratio por câmera, como Hagemann (2022):**
    - alvo virtual = cada tag isolada (4 cantos), pose por `IPPE_SQUARE` + LM, intrínsecos fixos;
    - σd² = 4·(1,4826·MAD)², com o MAD sobre as componentes com sinal;
    - s_d² com 1 − N_P/N contado em coordenadas, e MSE_calib robusto.
  - **Saída:** BR e ε_bias.
  - **Testes:** BR ≈ 0 em dados sintéticos sem viés; BR alto quando a distorção é omitida do
    modelo.

- [ ] **erro-mapeamento** — M2: erro de mapeamento entre calibrações e EME empírico
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: nucleo-offline
  - **Mapeamento entre duas calibrações:** grade a cada 20 px; desprojeta com A
    (`undistortPoints` com critério iterativo reforçado) e projeta com B.
  - **Variante com rotação:** otimiza uma rotação de 3 parâmetros, como o K(θ̂, θ̄) de Hagemann.
  - **Saídas:** RMS, máximo e mapa de calor.
  - **EME empírico:** cada uma das 10 calibrações contra a calibração média.
  - **Testes:** calibrações idênticas dão 0; uma perturbação conhecida em fx dá o deslocamento
    esperado.
  - Em aberto para o spec: a "média" de parâmetros de distorção é bem definida? Alternativa:
    calibrar com as imagens das 10 capturas juntas.

- [ ] **piloto-dados-existentes** — Rodar M1 e M2 nas capturas que já existem
  - Valor: alto · Esforço: P · MVP: sim
  - Depende de: persistencia-captura (export no recálculo), saida-calibracao, metrica-ajuste,
    erro-mapeamento
  - Usa `2026-10-16_ar_9x6`, `2026-10-20_ar_aprilgrid_v2`, `2026-10-23_acrilico_ar_aprilgrid` e
    `2026-11-03_camera_settings`, mais F.
  - Responde se o RMS de ~0,31 px do AprilGrid é viés, comparado ao xadrez. Também diz se o
    fy/fx aparece como viés.
  - **Pode criar uma feature de correção de detecção antes de execucao-a.**

- [ ] **roteiro-poses** — Roteiro fixo de 63 poses com IDs (R2)
  - Valor: alto · Esforço: G · MVP: sim
  - Depende de: persistencia-captura
  - **O roteiro é um YAML com quatro blocos:**
    - frontal: 3 distâncias × grade 3×3 = 27;
    - inclinações de ±35° nos dois eixos: 20;
    - rotação no plano 0/90/180/270°: 12;
    - borda extrema, com metade do alvo fora da imagem: 4.
  - **A distância é a fração da largura da imagem ocupada pelo alvo**, e a distância em metros
    também é registrada.
  - **A GUI indica a pose esperada** e confere, pela pose estimada, se a fração, a posição e a
    inclinação batem. O ID da pose vai no nome do arquivo.
  - **No modo roteiro, o filtro de dissimilaridade sai** (`calibration_checker.cpp:243-271`). Ele
    recusa amostras a menos de 0,15 de todas as anteriores. A rotação no plano (bloco 3) não muda
    posição, tamanho nem skew, então seria recusada (risco 1). Ele é substituído pela conferência
    da pose do roteiro.
  - **A fração da largura é medida pela caixa do alvo inteiro** projetada, e não só pela região
    das tags detectadas (`outsideCorners`, `aprilgrid.cpp:353-381`).
  - O AprilGrid já aceita vista parcial, então o bloco 4 não precisa de alternativa. Mas os
    pontos do bloco 4 só valem para a calibração de cada câmera se as detecções forem por câmera
    (ver persistencia-captura).

- [ ] **captura-estatica** — Aceitar só quadros parados e nítidos (R2, rolling shutter)
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: roteiro-poses
  - O alvo deve ficar parado ~1 s, medido pelo deslocamento dos cantos entre quadros
    consecutivos, antes de gravar. Quadros borrados são descartados. Os limiares vão para o YAML.
  - Hoje a nitidez é a variância do Laplaciano da **imagem inteira** (`main.cpp:579-586`). Um fundo
    nítido passa no teste com o alvo borrado (risco 7). A nitidez passa a ser medida só na região do
    alvo.

- [ ] **cobertura** — Verificação de cobertura antes de calibrar (R3)
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: nucleo-offline
  - **Grade 8×8 por câmera**, com um mínimo de N cantos por célula, inclusive nos cantos da
    imagem.
  - **Histograma de inclinação**, que exige poses acima de 30° nos dois eixos. A inclinação vem
    da pose do alvo por PnP. O "Skew" atual é um ângulo medido na imagem
    (`calibration_checker.cpp:74-119`) e não serve para isso.
  - **Relatório por captura, aprovada ou reprovada.** Uma captura reprovada é refeita.
  - N é derivado das capturas existentes e confirmado com o usuário.

- [ ] **aquecimento-termico** — Medir o tempo até o regime térmico
  - Valor: médio · Esforço: P · MVP: sim
  - Depende de: ambiente-sensores, persistencia-captura
  - Registra a temperatura `ONBOARD_*` e um parâmetro sensível ao longo do tempo, desde a câmera
    fria. Candidatos: fx por PnP num alvo fixo, ou a disparidade de um ponto fixo.
  - Define o `<X min>` do protocolo. A literatura lida não traz esse número (Shortis não trata
    disso).

- [ ] **validacao-cruzada** — M3: leave-one-capture-out, matriz 10×10
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: nucleo-offline
  - Calibra com a captura i e avalia nas imagens da captura j.
    - Pose do alvo por `solvePnP` na esquerda, com intrínsecos fixos.
    - Reprojeta na esquerda e, via R e T, na direita: é assim que os extrínsecos são testados.
  - Compara a diagonal da matriz com o resto. F entra como mais uma linha.
  - **Teste:** com i = j, reproduz o RMS de treino.

- [ ] **metricas-tarefa** — M4: erro epipolar, erro métrico e resíduo de plano
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: nucleo-offline
  - Três medidas:
    - erro epipolar |y_esq − y_dir| após a retificação (aceitável < 0,25 px; excelente < 0,1 px);
    - distâncias entre cantos triangulados contra o espaçamento medido no alvo, em %;
    - resíduo de ajuste de plano.

    Tudo em função da distância e da região da imagem.
  - Inclui a diferença de cx entre esquerda e direita após a retificação, que gera viés de
    disparidade (R5).

- [ ] **analise-estatistica** — R6: estatística por condição e relatório comparativo
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: erro-mapeamento, validacao-cruzada, metricas-tarefa
  - **Por condição:** média, IC95 % (t de Student, n = 10) e CV.
  - **Comparação A × B:**
    - teste de permutação, erros A–A (45 pares) contra A–B (100 pares);
    - aplicação cruzada;
    - TOST em fx, fy, baseline e erro métrico;
    - Levene/F nas dispersões.
  - **Saída:** relatório com tabelas e figuras.
  - **A margem do TOST** sai de ΔZ/Z ≈ Δf/f + ΔB/B e da faixa do SLAM (até 3 m). Ela é proposta e só fixada
    depois de perguntar ao usuário.

- [ ] **validacao-metrica** — R7: teste métrico em distâncias conhecidas
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: metricas-tarefa
  - Em cada distância, ajustada à faixa do SLAM, captura 5 pares com o alvo no centro e 4 com ele
    nas bordas.
  - Antes do teste: medir tags e espaçamento com paquímetro e verificar a planicidade do alvo.
  - Analisa o erro de distância e de comprimento por distância e região.
  - Seguindo Shortis, inclui distâncias maiores que a média da calibração. A faixa do SLAM vai até
    3 m, então as distâncias candidatas são 0,5 / 1 / 2 / 3 m, a confirmar no spec.

- [ ] **experimentos** — Protocolo das condições A, B e B′ (spec + plan)
  - Valor: alto · Esforço: M · MVP: sim
  - Depende de: todas as features de E1–E4 especificadas
  - **Por condição:**
    - preparação: aquecimento, limpeza do acrílico, checagem de reflexos com cena escura e luz
      ligada, medição do alvo;
    - procedimento, checklist por captura e critérios de aceite;
    - portão de decisão e análise prevista.
  - **Em B,** desmontar e remontar a tampa entre capturas e registrar a inclinação da porta.
  - Fica em `.ai/specs/experimentos/`.

- [ ] **execucao-a** — Condição A: ar, sem acrílico (10 capturas + teste métrico)
  - Valor: alto · Esforço: G · MVP: sim
  - Depende de: experimentos, piloto-dados-existentes
  - **Portão:** nossa calibração empata ou supera F na validação (M3, M4)?
  - **Saída:** os intrínsecos "secos" de referência.

- [ ] **execucao-b** — Condição B: ar, com acrílico, tampa montada uma vez
  - Valor: alto · Esforço: G · MVP: sim
  - Depende de: execucao-a
  - **Portão:** B é equivalente a A pelos critérios do R6?
  - Ordem em blocos e tampa não remontada (usuário, 2026-11-03). A repetibilidade da remontagem é
    uma premissa do encaixe da tampa, não é medida.

- [ ] **execucao-b-linha** — Condição B′: adotar a calibração com acrílico
  - Valor: médio · Esforço: P · MVP: não (condicional)
  - Depende de: execucao-b
  - Só se B falhar no portão: a referência de B vira a calibração operacional com acrílico, sem
    captura nova.

- [ ] **higiene-ferramentas** — Autocalibração no visualizador e `--svo` na calibração
  - Valor: baixo · Esforço: P · MVP: não
  - Depende de: —
  - `stereo_reprojection_viewer` abre a ZED 2i com a autocalibração ativa
    (`stereo_reprojection_viewer/src/main.cpp:308-338`). Desligar, para que nada visto nele venha
    de uma calibração ajustada pelo SDK (risco 9).
  - `zed_stereo_calibration --svo` é lido mas não aplicado (`main.cpp:102`, `:123-124`): a
    ferramenta abre a câmera conectada sem aviso. Implementar com a autocalibração desligada, ou
    remover a flag (risco 10).

- [ ] **execucao-robo** — Calibração da câmera do robô com o protocolo validado
  - Valor: alto · Esforço: M · MVP: não (depende da disponibilidade da câmera)
  - Depende de: execucao-b (ou execucao-b-linha)
  - A câmera de bancada é do mesmo modelo, mas tem outro serial. Por isso a calibração entregue ao
    SLAM precisa vir da câmera do robô, montada no cilindro.
  - Entrega o `optional_opencv_calibration_file` validado no lugar de `cesar_002`.

- [ ] **preparacao-agua** — Plano da etapa na água (R8)
  - Valor: médio · Esforço: M · MVP: não
  - Depende de: execucao-b
  - **Hipóteses:** fx_água/fx_ar ≈ 1,33 (só no centro, ver `referencias.md` C.1) e baseline
    constante.
  - **Escolha do modelo refrativo:** Pinax, se d0 ≈ 0,15·d1 se confirmar; senão uma porta plana
    com d e normal, como Jordt ou She.
  - **Avaliar a ferramenta aberta** de Seegräber et al. (2024).
  - **Validação** por faixa de distância.

- [ ] **condicoes-agua** — Execução de C1 (pinhole absorvendo a refração) e C2 (refrativo)
  - Valor: alto · Esforço: G · MVP: não (fora de escopo por agora)
  - Depende de: preparacao-agua

## Requisitos de origem

Resumo do brief do usuário (2026-11-03), para rastrear cada feature:

| Id | Requisito | Features |
|---|---|---|
| R1 | Persistência para reprocessar offline, inclusive com modelos refrativos | persistencia-captura, saida-calibracao, nucleo-offline |
| R2 | Roteiro de poses fixo, por fração da largura; captura estática | roteiro-poses, captura-estatica |
| R3 | Cobertura 8×8 e histograma de inclinação antes de calibrar | cobertura |
| R4 | M1 ajuste/BR, M2 mapeamento/EME, M3 leave-one-capture-out, M4 tarefa | saida-calibracao, metrica-ajuste, erro-mapeamento, validacao-cruzada, metricas-tarefa |
| R5 | Fábrica como calibração "F" em todas as métricas, mais o cx retificado | exportar-fabrica, saida-calibracao (independência de F), metricas-tarefa |
| R6 | Estatística: IC95, CV, permutação, aplicação cruzada, TOST, Levene | analise-estatistica |
| R7 | Teste métrico em distâncias conhecidas | validacao-metrica |
| R8 | Compatibilidade com a água: geometria da porta no `meta.json` | persistencia-captura, preparacao-agua |
