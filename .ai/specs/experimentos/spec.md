# Spec — Protocolo experimental: condições A, B e B′ (no ar)

> **O quê, por quê e condições de execução.** Origem: [`brief.md`](../../product/brief.md)
> "Plano de experimentos", R1–R8. Usa todos os módulos de [`backlog.md`](../../product/backlog.md).
> As condições C1/C2 (água) ficam fora deste spec ([`vision.md`](../../product/vision.md), fora de
> escopo).

## Objetivo

1. **Condição A** (ar, sem acrílico): obter os intrínsecos e extrínsecos "secos" de referência, com
   a incerteza medida, e decidir se a nossa calibração **empata ou supera a de fábrica (F)** na
   validação.
2. **Condição B** (ar, com acrílico): decidir se a calibração com o acrílico montado é
   **equivalente** à de A (R6). Se não for, B′: a calibração com acrílico passa a ser a
   operacional.
3. **Validar a metodologia na câmera de bancada.** A câmera do robô tem outro serial e será
   calibrada depois com o mesmo protocolo (`execucao-robo` no backlog).

## Contexto e evidências que motivam o desenho

- **No ar, o acrílico não mudou os intrínsecos de cada câmera, mas mudou o alinhamento estéreo**
  (~0,2 px de disparidade, ~1 % de profundidade a 3 m). Fonte: `aprilgrid-target/validation.md`,
  Parte 3. Uma única rodada por condição não diz se isso é efeito ou variação entre capturas; por
  isso 10.
- **A ligação rígida entre lente e porta é o fator mais importante** (Shortis 2015). Uma inclinação
  de 2° entre o sensor e a porta já causa erros grandes e assimétricos (Treibitz 2012).
  - O brief previa remontar a tampa entre as capturas de B.
  - **Decisão do usuário (2026-11-03): a tampa é montada uma vez e não é remontada.** No robô ela é
    aberta entre missões, mas o encaixe foi projetado para que a remontagem não mude a posição.
    Remontar 10 vezes também é trabalhoso.
  - Assim, **a repetibilidade da remontagem é uma premissa, não é medida** neste protocolo. A
    dispersão de B contra A continua testada (R6, Levene); ela agora reflete o acrílico, e não a
    montagem.
- **O fy/fx +0,2–0,3 % das calibrações AprilGrid está em aberto** (`validation.md`, Parte 4). O
  bloco 3 do roteiro (alvo girado) permite testá-lo dentro de A.

## Variáveis controladas (iguais em todas as capturas)

| Variável | Valor | Fonte |
|---|---|---|
| Câmera | ZED 2i S/N 35180697, lente de 2,1 mm | `vision.md` |
| Resolução e fps | HD720 @ 15 fps | `vision.md` |
| Autocalibração do SDK | desligada | auditoria §1.5 |
| Controles de captura | `config/camera_settings_zed2i.yaml`, o mesmo arquivo em tudo | spec `camera-settings` |
| Alvo | AprilGrid 6×6 de 660 mm; medidas de paquímetro na sessão | `persistencia-captura` R2 |
| Roteiro | `config/roteiro_v1.yaml`, 63 poses | `roteiro-poses` |
| Captura estática | limiares de `noise.json` | `captura-estatica` |
| Aquecimento | X min, de `aquecimento-termico` | — |
| Iluminação | artificial, fixa; sem luz do dia variando | — |
| Variante de calibração | a decidida pelo piloto, do zero | `piloto-dados-existentes` |
| Montagem | câmera parada, alvo movido | `vision.md` |

## Condições

| Id | Descrição | Capturas | Particularidade |
|---|---|---:|---|
| `A_ar_sem_acrilico` | ar, sem acrílico na frente da câmera | 10 | — |
| `metrico_A` | teste métrico sem acrílico | 1 sessão (36 pares) | spec `validacao-metrica` |
| `B_ar_com_acrilico` | ar, com a tampa de acrílico | 10 | tampa **montada uma vez**, antes de B1, e não mexida até B10 |
| `metrico_B` | teste métrico com acrílico | 1 sessão | — |
| B′ | decisão, sem captura nova | 0 | **só se B falhar no portão:** a referência de B vira a calibração operacional com acrílico |

## Requisitos

### Preparação de cada sessão de bancada
- **R1. Checklist de preparação,** registrado no `capture.log` ou numa nota da sessão:
  1. ambiente: `check_zed_access.sh --check` aprovado (spec `ambiente-sensores`);
  2. câmera ligada há pelo menos X min, com o horário de ligar registrado;
  3. controles de captura aplicados pelo arquivo, e os valores impressos no início e no fim
     iguais;
  4. **em B:** acrílico limpo (procedimento e material registrados);
  5. **em B:** checagem de reflexos internos. Com a cena escura e a iluminação de bancada ligada,
     uma captura de referência das duas câmeras é inspecionada em busca de reflexos ou imagens
     fantasmas na região onde o alvo vai passar. Ela é guardada na pasta da captura;
  6. **medição do alvo** com paquímetro e da planicidade, uma vez por condição, antes da primeira
     captura e no teste métrico (spec `validacao-metrica` R4). Os valores vão para a sessão.

### Cada captura
- **R2. Procedimento da captura i** (i = 1…10):
  1. criar o arquivo de sessão (condição, captura i, lente, medidas do alvo, porta, meio, roteiro,
     `calibration`, `static_capture`);
  2. **em B1:** registrar a montagem da tampa (data, quem montou, observações). Nas capturas
     seguintes, conferir que ela não foi mexida;
  3. executar o roteiro v1 completo;
  4. rodar a cobertura (`python3 -m zedcal.coverage`). Se for **reprovada**:
     - marcar a pasta com o arquivo `REPROVADA` e o motivo;
     - refazer como uma nova captura, com o número seguinte;
     - **nada é apagado**;
  5. calibrar com a variante definida (offline). A calibração C++ (`calib_cpp_*`) também é gerada,
     para conferir a paridade e para o SDK.
- **R3. Critérios de aceite de uma captura:**
  - a cobertura foi aprovada (R3 do brief, N do spec `cobertura`);
  - o filtro M1 passa: RMS mono E/D e RMS estéreo < 0,5 px;
  - no máximo k poses puladas ou fora de tolerância (ver Perguntas);
  - os metadados estão completos (`meta.json` sem campos obrigatórios nulos, exceto os de porta e
    meio, quando não se aplicam).

  Uma captura que não passa é marcada `REPROVADA` e refeita.
- **R4. Independência.** Entre duas capturas: o alvo é retirado da cena e recolocado; a calibração é
  feita do zero; nenhuma captura usa a calibração de outra. A câmera e a tampa não são mexidas.
- **R5. Ordem: em blocos** (decisão do usuário, 2026-11-03). A1…A10, depois a tampa é montada, e
  depois B1…B10.
  - A condição fica confundida com o tempo: dia, temperatura da sala, deriva lenta.
  - Mitigação: as temperaturas no `meta.json`; capturas de A e de B em condições de sala
    parecidas; e o registro da data de cada uma, para que a análise possa procurar uma tendência
    dentro de cada bloco.

### Análise prevista
- **R6. Por condição:**
  - M1, M2 (EME empírico e referência "média"), M3 (matriz LOCO, com F e a média) e M4;
  - a estatística descritiva de [`analise-estatistica`](../analise-estatistica/spec.md) R1–R2;
  - o teste métrico da condição.
- **R7. Diagnóstico do fy/fx em A:** comparar fy/fx de cada captura calibrada com e sem as poses do
  bloco 3 (alvo girado).
  - Se o excesso de fy/fx **acompanhar a orientação do alvo**, a causa está no alvo ou na detecção.
  - Se não, está na distribuição de poses (`validation.md`, Parte 4).
- **R8. Comparação A × B:** os testes de R3–R6 de `analise-estatistica`: permutação, aplicação
  cruzada, TOST e Levene/F.

### Portões de decisão
- **R9. Portão A — "a nossa calibração empata ou supera F na validação?"**
  - **Validação:** M3 fora da diagonal (as nossas nas capturas que não as geraram, contra F nas
    mesmas capturas) e M4 nas capturas de validação e no `metrico_A`.
  - **Critério:** em cada métrica, a diferença pareada por captura de validação (F − nossa) com
    IC95 %:
    - **"supera":** o IC fica todo acima de zero;
    - **"empata":** o IC fica dentro de ±margem;
    - **"perde":** o IC fica todo abaixo de −margem.
  - **Saída:** a referência "média" de A é publicada como os intrínsecos e extrínsecos "secos".
    Se "perde", investigar antes de B.
- **R10. Portão B — "B é equivalente a A?"** B é equivalente quando as três condições valem:
  - o teste de permutação (A–B contra A–A) não é significativo;
  - o TOST declara equivalência em fx, fy, baseline e erro métrico;
  - o Levene/F não aponta B mais disperso.

  Se alguma falhar, o relatório indica qual, e a decisão sobre B′ é do usuário. **B′:** adotar a
  referência "média" de B como a calibração operacional com o acrílico montado. Não há captura
  nova, porque as capturas de B já são calibrações com acrílico e com a tampa montada uma vez.
- **R11. Registro das decisões.** As decisões do piloto e da bancada são registradas neste spec
  (seção abaixo) antes da primeira captura de A: variante de calibração, N, X, margens, α e k.

## Notas das análises offline (2026-11-03, para a revisão do usuário)

- **O portão B provavelmente falha no erro métrico.** No piloto (M4, `metricas-tarefa` etapa 6),
  as calibrações sem acrílico medem o passo do alvo +0,8 a +1,0 % maior na captura com acrílico, e a
  calibração com acrílico mede −0,8 % nas capturas sem ele. Com a margem proposta de ±0,5 ponto
  percentual, o TOST não declararia equivalência. É uma captura de cada lado, sem roteiro; as 10
  capturas de A e B dizem o resto.
- **"O Levene/F não aponta B mais disperso" é uma condição fraca com n = 10.** Um DP dobrado é
  detectado 64 % das vezes pelo F e 31 % pelo Brown–Forsythe (`analise-estatistica`, plano,
  etapa 5). Não rejeitar não mostra que as dispersões são iguais. O relatório deve mostrar a razão
  dos DPs com o seu IC, e não só o p.

## Decisões a registrar antes de A

| Decisão | Origem | Valor |
|---|---|---|
| Variante de calibração (`joint` ou `fixed`; com ou sem rejeição) | piloto e M3 | **`joint`**, intrínsecos refinados com R, T, como o C++: no M3 sobre as capturas do piloto ele generaliza melhor nos extrínsecos (transferência para a direita 0,54 contra 0,68 px), com os intrínsecos empatados. Rejeição `none` nas análises offline (usuário, 2026-11-03) |
| N mínimo de cantos por célula | piloto e cobertura | — |
| X min de aquecimento | aquecimento-termico | — |
| Margens do TOST e orçamento de erro | analise-estatistica | — |
| Limiar de BR | piloto | nenhum por ora; referência: xadrez 0,24–0,31 (usuário, 2026-11-03) |
| Referência "média" do M2 | piloto | no espaço da imagem (usuário, 2026-11-03) |
| Pré-condição de A | piloto | o viés do AprilGrid (BR ≈ 0,7) diagnosticado antes da primeira captura (`diagnostico-vies-aprilgrid`; usuário, 2026-11-03) |
| α dos portões | analise-estatistica | — |
| Margem de "empata" do portão A, por métrica | este spec | — |
| k poses puladas permitidas | este spec | — |
| Ordem A/B | este spec | **em blocos** (usuário, 2026-11-03) |
| Remontagem da tampa em B | este spec | **não remontar**; montada uma vez (usuário, 2026-11-03) |

## Não-objetivos

- Condições na água (C1, C2) e modelos refrativos.
- A calibração da câmera do robô.
- Mudanças no SLAM.

## Critérios de aceitação

- [ ] Todas as decisões da tabela acima estão preenchidas e aprovadas antes da primeira captura
      de A.
- [ ] A tem 10 capturas aceitas e 1 sessão métrica; o relatório de A responde ao portão A.
- [ ] B tem 10 capturas aceitas e 1 sessão métrica, com a montagem da tampa registrada; o
      relatório A × B responde ao portão B.
- [ ] O diagnóstico do fy/fx (R7) está no relatório de A.
- [ ] Toda captura reprovada está preservada e marcada, com o motivo.

## Perguntas em aberto

- **Montagem em A (respondida em 2026-11-03):** a câmera fica no mesmo suporte do cilindro, só sem a
  tampa. Entre A e B, só o acrílico muda.
- **Ordem, remontagem e B′ (respondidas em 2026-11-03):** em blocos; a tampa é montada uma vez; B′
  é a adoção da referência de B, sem captura nova (R5, R10).
- NEEDS CLARIFICATION: **Margem de "empata"** do portão A, por métrica (M3 em px, M4 em px e %), e
  **k** (poses puladas). Proposta de k: 3 das 63.
