# Spec — Roteiro fixo de poses (R2)

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R2; [`auditoria.md`](../../auditoria.md) §1.3 e §6 (risco 1). Depende de
> [`persistencia-captura`](../persistencia-captura/spec.md) (modo sessão, `pose_id`) e de
> [`exportar-fabrica`](../exportar-fabrica/spec.md) (F, só para orientar o operador).

## Problema

A coleta hoje é livre. O operador move o alvo até as métricas X, Y, Size e Skew completarem, e a
ferramenta recusa amostras "parecidas" com as anteriores (`calibration_checker.cpp:243-271`). Isso
tem três consequências:
- **as capturas não são comparáveis entre si** nem entre condições, porque cada uma tem um
  conjunto de poses diferente;
- **as poses que o brief pede são recusadas,** porque não mudam posição, tamanho nem skew: é o caso
  da rotação do alvo no plano (risco 1);
- **o "Skew" é um ângulo na imagem** e não diz quanto o alvo está inclinado em cada eixo
  (`calibration_checker.cpp:74-119`).

Os fundamentos:
- **Inclinação** determina a focal: na Parte 2 de `validation.md`, uma captura quase frontal
  deu fx 0,7 % alto. Peng & Sturm (2019) descrevem as poses boas como de inclinação grande, alvo
  cobrindo o campo e tocando a borda.
- **Rotação de 0/90/180/270°** é um dos quatro critérios de rede de Shortis (2015). No cilindro,
  quem gira é o alvo, não a câmera.
- **Bordas da imagem**, onde a distorção é maior, precisam de pontos. O AprilGrid aceita vista
  parcial (`aprilgrid-target` R4).

## Contexto

- **A montagem da bancada:** a câmera fica parada e o alvo se move ([`vision.md`](../../product/vision.md)).
- **O alvo:** AprilGrid de 660 mm de lado.
- **As distâncias por fração da largura** em HD720, com HFOV ~100° (estimativa pinhole em
  `vision.md`):

  | Faixa | Fração da largura | Distância |
  |---|---|---|
  | Perto | 70–80 % | 0,35–0,40 m |
  | Médio | 40–50 % | 0,55–0,69 m |
  | Longe | 20–25 % | 1,1–1,4 m |

- **Perto, o alvo não cabe na altura da imagem.** O alvo é quadrado e a imagem é 16:9: a 75 % da
  largura ele ocupa ~960 px de altura, contra 720 px. Por isso as posições "perto" são definidas
  pelo centro do alvo, com vista parcial.
- **A fração da largura vem da caixa do alvo inteiro, projetada.** Ela pode passar da imagem. A
  caixa só das tags detectadas (`outsideCorners`, `aprilgrid.cpp:353-381`) subestima a fração com
  vista parcial.

## Requisitos

### O roteiro
- **R1.** O roteiro é um arquivo YAML (`config/roteiro_v1.yaml`) com a lista **ordenada** das
  poses. Cada pose tem:
  - id e bloco;
  - faixa de distância (fração mínima e máxima da largura);
  - posição do centro do alvo na grade 3×3 da imagem;
  - inclinação em torno do eixo horizontal e do vertical da câmera (graus, com sinal);
  - rotação no plano (0/90/180/270°);
  - borda (para o bloco 4);
  - as tolerâncias de cada grandeza.
- **R2.** O roteiro v1 tem as 63 poses do brief, nesta ordem:

  | Bloco | Poses | Conteúdo |
  |---|---:|---|
  | 1, frontal | 27 | 3 distâncias × grade 3×3; alvo paralelo ao sensor |
  | 2, inclinações | 20 | distância média; centro + 4 cantos; ±35° em torno do eixo horizontal e ±35° em torno do vertical |
  | 3, rotação no plano | 12 | perto; centro + 2 cantos opostos; 0, 90, 180 e 270° |
  | 4, borda extrema | 4 | perto; metade do alvo fora da imagem, em cada uma das 4 bordas |

- **R3.** Formato dos ids: `B<bloco>-<distância>-<posição>[-<variação>]`.
  - Distância: `P`, `M` ou `L`.
  - Posição: `TL TC TR ML MC MR BL BC BR`.
  - Variação: `Hp35`, `Hm35`, `Vp35`, `Vm35`, `R000`, `R090`, `R180`, `R270`, ou `EL ER ET EB`
    para as bordas.

  Exemplos: `B1-P-TL`, `B2-M-MC-Hp35`, `B3-P-TL-R090`, `B4-P-EL`. O mesmo arquivo e os mesmos ids
  valem para todas as capturas e condições.
- **R4.** Outros roteiros, como o do teste métrico ([`validacao-metrica`](../validacao-metrica/spec.md)),
  usam o mesmo formato com outro arquivo.

### Na coleta
- **R5.** O arquivo de sessão aponta para o roteiro (`roteiro: <arquivo>`). Com roteiro:
  - a ferramenta mostra a **pose esperada**: id, descrição em texto e um esquema do retângulo
    esperado sobre a imagem;
  - ela segue a ordem do arquivo.
- **R6.** Para o quadro corrente, a ferramenta mede e mostra:
  - a fração da largura (caixa do alvo inteiro projetada);
  - a posição do centro do alvo;
  - a inclinação em torno de cada eixo da câmera e a rotação no plano, pela pose PnP;
  - a distância estimada em metros.

  Cada grandeza fica **verde dentro da tolerância** e vermelha fora.
- **R7.** **A pose PnP só orienta o operador e não entra na calibração.** Ela usa F (ou a
  calibração indicada na sessão). A distância estimada e as grandezas medidas no momento do
  salvamento vão para o `meta.json` do par.
- **R8.** Um par é salvo quando todas as condições abaixo valem ao mesmo tempo:
  - o operador aperta `s`;
  - todas as grandezas estão dentro da tolerância;
  - o mínimo de tags está atendido;
  - o quadro passou na imobilidade e na nitidez ([`captura-estatica`](../captura-estatica/spec.md)).
- **R9.** **No modo roteiro, o filtro de dissimilaridade fica desligado.** A conferência de R8 o
  substitui. As métricas X, Y, Size e Skew continuam no painel, só como informação.
- **R10.** Teclas extras no modo roteiro:
  - `o`: salva **fora da tolerância**, com `out_of_tolerance: true` no `meta.json`, para quando a
    pose é inviável;
  - `k`: pula a pose, registrada como `skipped`;
  - `b`: volta uma pose sem apagar nada; o par novo recebe o `n` seguinte.
- **R11.** **Fim do roteiro:** a coleta termina e a ferramenta mostra o resumo das poses salvas,
  das fora de tolerância e das puladas. A calibração C++ roda se houver o mínimo de pares. A
  verificação de cobertura é offline ([`cobertura`](../cobertura/spec.md)).
- **R12.** Sem roteiro na sessão, ou sem sessão, a coleta é a livre de hoje.

## Condições de implementação

- **Definição dos ângulos** no referencial óptico da câmera esquerda (x direita, y baixo, z
  frente). Com a normal do alvo **n**:
  - a inclinação "em torno do eixo horizontal" é o ângulo de **n** projetada no plano y–z com o
    eixo z;
  - a "em torno do vertical" é o mesmo no plano x–z;
  - a rotação no plano é o ângulo do eixo X do alvo projetado na imagem.

  A definição fica no arquivo de roteiro e no README, porque a cobertura usa a mesma.
- **A lógica fica em `common/pose_script/`, só com OpenCV e testável:** leitura do roteiro,
  geometria do alvo inteiro projetado e as grandezas de R6 a partir de uma pose. A GUI só consome
  essa lógica.
- **As tolerâncias padrão de v1 são parâmetros de projeto, não números de literatura.** Fração: as
  faixas do brief. Posição: ±0,08 da imagem. Ângulos: ±5°. Elas ficam no YAML e são revistas na
  primeira captura de bancada.

## Não-objetivos

- A imobilidade e a nitidez: spec próprio.
- Guiar a próxima pose pela incerteza, como o Calibration Wizard: o roteiro é fixo de propósito,
  para ser comparável entre capturas.
- Automatizar o movimento do alvo.

## Critérios de aceitação

- [ ] `config/roteiro_v1.yaml` tem as 63 poses nos 4 blocos, com ids únicos. Um teste confere a
      contagem por bloco e o formato dos ids.
- [ ] **Geometria, com poses sintéticas** (câmera F, alvo em pose dada), as funções devolvem:
  - a fração da largura com erro < 0,5 ponto percentual;
  - os ângulos com erro < 0,5°;
  - a rotação no plano 0/90/180/270 sem confusão de quadrante;
  - a distância com erro < 1 %.
- [ ] Na bancada, uma captura completa segue as 63 poses na ordem. Os arquivos saem com os ids do
      roteiro, e as poses do bloco 3 **não** são recusadas por similaridade.
- [ ] `o`, `k` e `b` produzem os registros esperados no `meta.json`.
- [ ] Sem roteiro, a regressão do xadrez continua idêntica e a coleta livre não muda.

## Perguntas em aberto

- NEEDS CLARIFICATION: **Suporte do alvo** para as inclinações de ±35° e as rotações: à mão, em
  suporte ou em tripé (pendência de [`vision.md`](../../product/vision.md)). Isso afeta as
  tolerâncias e a imobilidade.
