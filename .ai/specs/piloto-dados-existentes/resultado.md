# Resultado do piloto — métricas nas capturas existentes

> Spec: [`spec.md`](spec.md). Rodado em 2026-11-03 com `analysis/piloto/run_piloto.py` sobre
> `calib_runs/piloto/` (detecções exportadas das quatro capturas e F da S/N 35180697). As saídas
> completas, com calibrações, relatórios, mapas de resíduos e `summary.json`, estão em
> `calib_runs/piloto/resultados/`. Todas as calibrações são `rational8` em HD720. Nenhuma pasta
> original foi alterada (md5 conferido).

## Base

| Captura | Alvo | Pares (usados) |
|---|---|---:|
| `xadrez` (2026-10-16) | xadrez 9×6, 23,8 mm | 35 (35) |
| `v2` (2026-10-20) | AprilGrid | 29 (29) |
| `acrilico` (2026-10-23) | AprilGrid, através do acrílico | 35 (34) |
| `controles` (2026-11-03) | AprilGrid, controles fixos | 25 (25) |

**A calibração offline no modo de compatibilidade reproduz os YAML do C++ nas quatro capturas.**
- K: até 6e-8 relativo;
- T: até 2e-5 relativo (o xadrez; nas AprilGrid, ≤ 7e-7);
- D: até 1e-5.

Isso confere o critério de R1 e R2.

## As seis perguntas (R4)

### 1. O ruído do detector (σ̂_d)

| Alvo | σ̂_d esq. / dir. (px) |
|---|---|
| Xadrez | 0,144 / — (BR 0,31 / 0,24) |
| AprilGrid (`v2`, `acrilico`, `controles`) | 0,118–0,134 |

Os dois alvos têm ruído de detecção parecido, ~4–5× os 0,03 px de Hagemann (que usou outra câmera
e outro detector, então serve só de contexto). **O AprilGrid não é mais ruidoso que o xadrez.**

### 2. O BR do AprilGrid comparado ao do xadrez

Calibração própria de cada captura, variante compatível com o C++:

| Captura | BR esq. / dir. | ε_bias esq. (px) | RMS estéreo (px) |
|---|---|---:|---:|
| `xadrez` | 0,31 / 0,24 | 0,096 | 0,254 |
| `v2` | 0,70 / 0,69 | 0,190 | 0,316 |
| `acrilico` | 0,73 / 0,74 | 0,219 | 0,369 |
| `controles` | 0,71 / 0,66 | 0,185 | 0,311 |

**Sim, o RMS de ~0,31 px do AprilGrid é dominado por viés.** O excesso em relação ao xadrez
(0,25 px) vem do viés (ε_bias de 0,19–0,22 px contra 0,10 px), não do ruído, que é o mesmo.

F avaliada nas mesmas imagens tem BR maior ainda: 0,34 no xadrez e 0,75–0,79 nas AprilGrid.

### 3. O mapa de resíduos e o fy/fx

- **Bordas.** No AprilGrid, o RMS nas células de borda é maior que no centro: 0,35–0,44 px contra
  0,28–0,32 px. No xadrez, borda e centro são iguais (0,236 / 0,238 px).
- **Gradiente vertical com F.** Avaliando F (fy/fx ≈ 1) nos dados AprilGrid, o resíduo vertical
  médio por linha da imagem vai de positivo no alto a negativo embaixo:
  - `controles`: +0,10 → −0,27 px;
  - `acrilico`: +0,18 → −0,39 px;
  - `v2`: +0,12 → −0,12 px.

  No xadrez o gradiente é pequeno (+0,02 → −0,06 px).
- **Regressão dos resíduos com F,** com pose livre:
  - os dados AprilGrid pedem uma escala vertical relativa menor que a horizontal, em 0,03–0,05 %;
  - no xadrez, 0,00–0,01 %.

  A pose livre absorve a maior parte do efeito; o sinal é pequeno, mas consistente nas três
  capturas AprilGrid e ausente no xadrez.

**Causas testadas offline:**

| Hipótese | Teste | Resultado |
|---|---|---|
| Alvo empenado | Dados sintéticos com flecha de 0, 1, 2 e 4 mm | Seriam precisos ~4 mm para BR ≈ 0,6, e o efeito vai para o fx (−0,86 %), não para o fy/fx (−0,02 %). **Não explica.** |
| Refino dos cantos anisotrópico | Deslocamento do refino (refinado − não refinado) por tipo de canto e por faixa da imagem | ~1,4 px para fora, em diagonal, com variações iguais na vertical e na horizontal. O gradiente vertical das médias por faixa era efeito de seleção dos cantos. **Não explica.** |
| Movimento durante a leitura (rolling shutter) | Por vista, pose livre com os intrínsecos fixos e a parte do resíduo linear na linha da imagem, d = v·(y − ȳ), que nenhuma pose rígida absorve (`analysis/piloto/rolling_shutter.py`, 2026-11-04) | Existe em todas as capturas, inclusive no xadrez: a inclinação por vista muda de sinal entre vistas e é a mesma nas duas câmeras (correlação E–D 0,56–0,92). Mas explica só ~1–2 % do MSE do resíduo por vista (p90 5–11 %), com amplitude parecida no xadrez e no AprilGrid. **Não explica** o BR de 0,7; reforça a captura estática. |
| Seleção das tags (só as comuns) | Variantes `all` × `common` | Mesmo fy/fx e mesmo BR (pergunta 4). **Não explica.** |

**Ficam em aberto:**
- **a distribuição de poses.** O AprilGrid de 66 cm foi capturado quase sempre apoiado no chão,
  inclinado em torno do eixo horizontal;
- **um efeito do alvo ou da detecção que só aparece nas bordas** da imagem (distorção forte com
  tags grandes).

**O teste que decide** é o do roteiro: o alvo girado 90° no próprio plano (bloco 3), e capturas com
inclinações nos dois eixos. Se o fy/fx acompanhar o giro, a causa é o alvo ou a detecção; se sumir
com poses equilibradas, é a distribuição de poses. Precisa da bancada.

### 4. Todas as tags por câmera × só as comuns

- **Nas bordas, todas as tags dão 13 % a 43 % mais cantos por câmera.** Por exemplo, `acrilico`
  esquerda: 709 contra 496.
- **Com o estéreo `joint`, o resultado é idêntico ao das tags comuns** (0,000 px de erro de
  mapeamento entre as duas; 0,026 px no `acrilico`). O ajuste estéreo reotimiza os intrínsecos só
  com as tags comuns e apaga o ganho.
- **Com o estéreo `fixed`,** os intrínsecos usam todas as tags. A mudança vai de 0,5 a 0,8 px no
  mapeamento (AprilGrid) e não muda nem o BR nem o fy/fx.

### 5. `joint` × `fixed` e com × sem rejeição

- **`joint` × `fixed`:** de 0,51 a 0,76 px de erro de mapeamento nas AprilGrid, e 1,64 px no xadrez.
  O baseline muda até 0,3 mm (`acrilico`: 120,26 → 120,54 mm). Com as métricas de ajuste (M1) não
  dá para dizer qual é melhor: o BR é igual. Isso é generalização, então é o M3 (validação
  cruzada) que decide.
- **Rejeição de outliers:** não removeu nenhuma vista em nenhuma captura. Aqui, o efeito é nulo.

### 6. As três referências "média" do M2

Nas 3 calibrações AprilGrid, variante compatível:
- **média no espaço da imagem × média simples dos parâmetros:** 0,006–0,010 px, equivalentes
  aqui;
- **calibração conjunta** (as três capturas juntas) × referência: 0,38 px.

O ajuste da referência fechou com 0,0002–0,0004 px.

## Recomendações (para aprovação)

1. **Não começar a condição A antes de entender o viés do AprilGrid.**
   - Hoje as calibrações AprilGrid têm BR ≈ 0,7, e as 10 capturas de A herdariam isso.
   - Proposta: uma feature curta, `diagnostico-vies-aprilgrid`, na primeira sessão de bancada,
     com quatro itens:
     - a medida da planicidade do alvo (régua e lâmina), que já está no R7 do brief;
     - uma captura com o roteiro v1, que traz as inclinações nos dois eixos e o alvo girado;
     - uma captura de xadrez com as mesmas poses, para comparar;
     - a comparação de BR e fy/fx entre elas.

   Os módulos de captura (`roteiro-poses`, `captura-estatica`) precisam estar prontos antes dela.
2. **Variante de calibração dos experimentos:** fixar depois do M3 (`validacao-cruzada`),
   rodando-o sobre estas mesmas capturas.
   - **`joint`** é o da ferramenta C++;
   - **`fixed`** aproveita as tags de borda de cada câmera.

   Até lá, a referência é a variante compatível (`common`, `joint`, `cpp`).
3. **Rejeição de outliers nas análises offline: `none`.**
   - Aqui ela não removeu nada.
   - Na captura com um par corrompido (`saida-calibracao`, etapa 5), ela removeu o par mas refez
     as calibrações a partir de intrínsecos já distorcidos.
   - Entre capturas, remover vistas diferentes prejudica a comparação.
   - No lugar dela, revisar as vistas com erro alto pelo relatório e pelo M1.
4. **Limiar de BR:** não fixar agora. Como referência, o xadrez dá 0,24–0,31; uma calibração
   AprilGrid "boa" deveria chegar perto disso depois do diagnóstico.
5. **Referência "média" do M2:** manter a do espaço da imagem (spec `erro-mapeamento` R5). Ela é
   equivalente à média simples nestes dados e mais robusta quando as distorções divergem.
6. **N mínimo por célula (cobertura):** fica para quando o módulo `cobertura` existir.
   - As distribuições de contagem por célula nestas capturas estão em
     [`cobertura/plan.md`](../cobertura/plan.md), etapa 5.

## Diagnóstico offline do viés AprilGrid (2026-11-04)

Feito com os dados existentes, sem o alvo, como parte do `diagnostico-vies-aprilgrid`. Scripts
exploratórios em `analysis/piloto/`; variante `compat`, rejeição `none`.

1. **O refino não é a causa.** Calibrando com os cantos antes do refino (`refinement.py`), o
   ε_bias sobe de 0,19 para 1,3–2,0 px: o refino tira ~90 % do viés. O excesso de fy/fx continua
   (+0,20–0,25 %), então ele também não vem do refino.
2. **O excesso de fy/fx é o alvo esticado em Y.** Com os intrínsecos de F fixos
   (`target_aspect.py`), as três capturas AprilGrid preferem o modelo do alvo esticado
   **+0,20–0,28 % no eixo Y do alvo**, nas duas câmeras; o xadrez, +0,04–0,09 %. Calibrando com o
   alvo esticado +0,23 % (`target_aspect_br.py`), fy/fx cai para −0,01…+0,08 %, como o xadrez e
   F. Como as vistas mantêm o alvo quase em pé (rotação no plano entre −22° e +24°), a calibração
   põe esse erro do alvo em fy/fx. Em 660 mm, 0,23 % são ~1,5 mm: dá para medir largura × altura
   do alvo na bancada.
3. **O BR não vem das bordas** (`br_by_region.py`): nos tags centrais, o BR fica em 0,56–0,67.
4. **O resto do viés é um padrão fixo do alvo impresso** (`tag_coherence.py`): ~60 % da energia
   do resíduo é o deslocamento do tag inteiro (o esperado com cantos independentes é 25 %), e o
   deslocamento de cada tag no plano do alvo (~0,3 mm RMS) se repete entre as três capturas
   (correlação 0,92–0,94).
5. **Corrigindo o alvo com um mapa medido nos dados, com validação cruzada** (o mapa de cada
   captura vem só das outras duas):

   | Mapa | RMS esquerda (px) | BR esquerda | ε_bias esquerda (px) |
   |---|---:|---:|---:|
   | nenhum (grade nominal) | 0,305–0,367 | 0,70–0,73 | 0,185–0,219 |
   | por tag (36 deslocamentos, `target_map_cv.py`) | 0,260–0,306 | 0,58–0,60 | 0,145–0,165 |
   | por canto (144 deslocamentos, ~0,41 mm RMS, `corner_map_cv.py`) | 0,229–0,268 | 0,53–0,54 | 0,122–0,136 |

   O mapa por canto tira ~25 % do RMS e ~35 % do ε_bias. O que sobra (ε_bias ~0,12 px) inclui
   o ruído de um mapa estimado com só duas capturas.

**Conclusão:** o viés das calibrações AprilGrid é, na maior parte, **erro de geometria do alvo
impresso**: cantos ~0,4 mm fora da grade ideal, de forma repetível, mais um esticamento de ~0,23 %
em Y que vira fy/fx. Não é a detecção, nem as bordas da imagem, nem o movimento.

**Decisões para o usuário** (antes da condição A):
- (a) **um alvo melhor:** impressão de precisão em base rígida e plana (ex.: alumínio composto ou
  vidro), com o certificado do fabricante; ou
- (b) **estimar a geometria do alvo junto com a calibração** (ajuste de feixes com os pontos do
  alvo livres, como o `calibrateCameraRO` do OpenCV, que não aceita vistas parciais; seria
  implementado no `zedcal`), usando as capturas de A; ou
- (c) **manter o alvo atual** e aceitar o viés, sabendo que ele entra em fy/fx e no BR.
- Em qualquer caso, **medir largura e altura do alvo** na bancada, para confirmar o esticamento.

## Perguntas ao usuário

- Aprovar ou ajustar as recomendações 1 a 5.
- A recomendação 1 muda a ordem do backlog: `roteiro-poses` e `captura-estatica` passam à frente,
  junto com a nova feature de diagnóstico. O `validacao-cruzada` (M3) segue em paralelo, offline.
