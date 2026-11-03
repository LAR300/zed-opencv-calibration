# Plano — Roteiro fixo de poses

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`persistencia-captura`](../persistencia-captura/plan.md) concluído (sessão, `pose_id`,
  `meta.json`).
- F exportada ([`exportar-fabrica`](../exportar-fabrica/plan.md)), para a pose PnP de orientação.

## Etapas

### Fase A — lógica pura `common/pose_script/` (TDD)

- [ ] 1. **Leitura do roteiro** (R1, R3). Parse e validação: ids únicos e no formato, tolerâncias
  positivas, ângulos e frações nas faixas.
  - Testes primeiro: o arquivo v1 é aceito; um id repetido, um formato inválido ou uma tolerância
    negativa são recusados com mensagem.
  - **Pronto quando:** `ctest` verde.
- [ ] 2. **Roteiro v1** (R2). Escrever `config/roteiro_v1.yaml` com as 63 poses.
  - Testes primeiro: 27/20/12/4 por bloco; as poses do bloco 2 têm distância `M`; as do 3 e do 4
    têm `P`.
  - **Pronto quando:** `ctest` verde.
- [ ] 3. **Geometria da pose** (R6, condições de implementação).
  - Funções: caixa do alvo inteiro projetada, fração da largura, centro, os dois ângulos de
    inclinação, rotação no plano e distância.
  - Também um verificador "dentro da tolerância", grandeza por grandeza.
  - Testes primeiro: os casos sintéticos do critério de geometria, inclusive o alvo parcialmente
    fora da imagem (fração > visível) e os quatro quadrantes de rotação.
  - **Pronto quando:** `ctest` verde.

### Fase B — GUI e integração

- [ ] 4. **Modo roteiro na coleta** (R5, R7, R8, R9, R12). A sessão aponta o roteiro; a pose
  esperada e as grandezas aparecem na tela; o salvamento exige R8; o filtro de dissimilaridade é
  desligado; as grandezas vão para o `meta.json`.
  - **Pronto quando:**
    - na bancada, as 9 poses do bloco 1 "perto" são salvas com os ids certos;
    - uma pose do bloco 3 a 90° é salva logo depois da de 0° na mesma posição;
    - a regressão do xadrez é idêntica.
- [ ] 5. **Teclas `o`, `k`, `b` e o fim do roteiro** (R10, R11).
  - **Pronto quando:** cada tecla gera o registro esperado no `meta.json`, e o resumo final lista as
    poses salvas, fora de tolerância e puladas.
- [ ] 6. **Captura completa de bancada.** As 63 poses.
  - **Pronto quando:** todas foram salvas ou justificadas (`o` ou `k`); o tempo total foi
    registrado; as tolerâncias foram revistas com o operador, e o v1 foi atualizado se preciso.
- [ ] 7. **Documentação.** README (roteiro, ids, definição dos ângulos, teclas) e um guia de bancada
  com fotos ou esquemas de cada bloco.
  - **Pronto quando:** o guia permite fazer o roteiro sem consultar o código.

## Riscos

- **A pose PnP com F pode ficar ruidosa** em vista parcial muito grande (bloco 4). Mostrar a
  grandeza como "indisponível" quando o PnP for mal condicionado, em vez de um valor errado.
- **As inclinações de ±35° com o alvo de 66 cm podem perder tags** por ângulo rasante. Se o mínimo
  de tags não for atendido, o operador usa `o` e o caso fica registrado. Rever a tolerância na
  etapa 6.
