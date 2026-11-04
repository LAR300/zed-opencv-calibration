# Plano — Roteiro fixo de poses

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`persistencia-captura`](../persistencia-captura/plan.md) concluído (sessão, `pose_id`,
  `meta.json`).
- F exportada ([`exportar-fabrica`](../exportar-fabrica/plan.md)), para a pose PnP de orientação.

## Etapas

### Fase A — lógica pura `common/pose_script/` (TDD)

- [x] 1. **Leitura do roteiro** (R1, R3). `common/pose_script/pose_script.{hpp,cpp}`
  (`loadPoseScript`). O formato dos ids ficou mais geral: partes de letras e dígitos separadas por
  `-`, como `B1-P-TL` ou `MET-300-MC-1` do roteiro métrico. Quando o id começa por `B<n>-`, `n` tem
  de bater com o `block`.

  Especificação original da etapa: Parse e validação: ids únicos e no formato, tolerâncias
  positivas, ângulos e frações nas faixas.
  - Testes primeiro: o arquivo v1 é aceito; um id repetido, um formato inválido ou uma tolerância
    negativa são recusados com mensagem.
  - **Pronto quando:** `ctest` verde.
- [x] 2. **Roteiro v1** (R2). `config/roteiro_v1.yaml`, gerado por `config/make_roteiro_v1.py`
  (edita-se o gerador, não o arquivo). O gerador também escreve `config/roteiro_metrico_v1.yaml`
  (spec `validacao-metrica`, R2: 36 poses).
  - Posições na grade 3×3 em 1/6, 1/2 e 5/6 da imagem.
  - Bordas do bloco 4 com o centro do alvo na borda, ou seja, metade dele fora.

  Especificação original da etapa: Escrever `config/roteiro_v1.yaml` com as 63 poses.
  - Testes primeiro: 27/20/12/4 por bloco; as poses do bloco 2 têm distância `M`; as do 3 e do 4
    têm `P`.
  - **Pronto quando:** `ctest` verde.
- [x] 3. **Geometria da pose** (R6, condições de implementação). `viewGeometry` (fração da largura
  pela caixa do alvo inteiro, centro, inclinações, rotação no plano, distância) e `checkPose`.
  - As convenções ficam no cabeçalho de `pose_script.hpp`.
  - Girar o alvo em torno da própria normal por g põe o eixo X dele em −g na imagem; por isso o
    roteiro usa ângulos na imagem.
  - Testes em `tests/test_pose_script.cpp` (154/154 no total): erros de 1e-9 em poses construídas,
    abaixo do limite de 0,5° do plano.

  Especificação original da etapa:
  - Funções: caixa do alvo inteiro projetada, fração da largura, centro, os dois ângulos de
    inclinação, rotação no plano e distância.
  - Também um verificador "dentro da tolerância", grandeza por grandeza.
  - Testes primeiro: os casos sintéticos do critério de geometria, inclusive o alvo parcialmente
    fora da imagem (fração > visível) e os quatro quadrantes de rotação.
  - **Pronto quando:** `ctest` verde.

### Fase B — GUI e integração

- [ ] 4. **Modo roteiro na coleta** (R5, R7, R8, R9, R12). **Implementado em 2026-11-04; falta a
  verificação com o alvo.**
  - Sessão: chaves opcionais `roteiro` e `pose_calibration` (padrão: a F exportada em
    `<output_root>/fabrica/SN<serial>/`); caminhos relativos à pasta da sessão.
  - Lógica testável em `common/pose_script/`: `ScriptRunner` (ordem, `o`/`k`/`b`, resumo),
    `decideScriptSave` (regra de R8) e `estimateTargetPose` (o mesmo PnP robusto do Python; teste
    com a distorção de F e o alvo a ±35° na borda). 172/172 testes C++.
  - Na coleta: retângulo da pose esperada na imagem esquerda; painel na direita com a pose,
    as grandezas (verde/vermelho), a imobilidade e a nitidez do alvo; o filtro de
    dissimilaridade e o de nitidez da imagem inteira saem; as métricas X/Y/Size/Skew **não** são
    atualizadas no modo roteiro (desvio de R9: a cobertura é verificada offline pelo
    `cobertura`); o `meta.json` ganha `pose`, `out_of_tolerance`, `still` por par e `script`
    com os eventos.
  - Verificado ao vivo sem o alvo (Xvfb, teclas simuladas): `k` ×3, `b`, `s` e `o` recusados
    com os motivos certos, `k` até o fim; resumo com as 63 puladas; "too few pairs" e saída sem
    calibrar; 65 eventos no `meta.json`. Regressão do xadrez idêntica.

  Especificação original da etapa: A sessão aponta o roteiro; a pose esperada e as grandezas
  aparecem na tela; o salvamento exige R8; o filtro de dissimilaridade é desligado; as grandezas
  vão para o `meta.json`.
  - **Pronto quando:**
    - na bancada, as 9 poses do bloco 1 "perto" são salvas com os ids certos;
    - uma pose do bloco 3 a 90° é salva logo depois da de 0° na mesma posição;
    - a regressão do xadrez é idêntica.
- [ ] 5. **Teclas `o`, `k`, `b` e o fim do roteiro** (R10, R11). Implementado com a etapa 4;
  `k`, `b` e o fim verificados ao vivo; `o` salvando de fato precisa do alvo.
  - **Pronto quando:** cada tecla gera o registro esperado no `meta.json`, e o resumo final lista as
    poses salvas, fora de tolerância e puladas.
- [ ] 6. **Captura completa de bancada.** As 63 poses.
  - **Pronto quando:** todas foram salvas ou justificadas (`o` ou `k`); o tempo total foi
    registrado; as tolerâncias foram revistas com o operador, e o v1 foi atualizado se preciso.
- [ ] 7. **Documentação.** A parte do README está feita (2026-11-04: "Pose Script", com ids,
  ângulos, teclas e registros). **Falta** o guia de bancada com fotos ou esquemas de cada bloco,
  que depende da captura completa (etapa 6).

  Especificação original da etapa: README (roteiro, ids, definição dos ângulos, teclas) e um
  guia de bancada com fotos ou esquemas de cada bloco.
  - **Pronto quando:** o guia permite fazer o roteiro sem consultar o código.

## Riscos

- **A faixa "perto" do brief (70–80 % da largura) não cabe na altura da imagem.** O alvo é
  quadrado e a imagem é 16:9: com 75 % da largura, ele tem 133 % da altura. Nas posições de canto
  do bloco 1, mais da metade do alvo fica fora da imagem. O AprilGrid aceita vista parcial, mas o
  "centro do alvo" em (1/6, 1/6) fica difícil de enquadrar. A maior fração com o alvo inteiro
  visível é 0,56. Pergunta ao usuário (2026-11-04).

- **A pose PnP com F pode ficar ruidosa** em vista parcial muito grande (bloco 4). Mostrar a
  grandeza como "indisponível" quando o PnP for mal condicionado, em vez de um valor errado.
- **As inclinações de ±35° com o alvo de 66 cm podem perder tags** por ângulo rasante. Se o mínimo
  de tags não for atendido, o operador usa `o` e o caso fica registrado. Rever a tolerância na
  etapa 6.
