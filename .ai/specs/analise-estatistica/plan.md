# Plano — Estatística por condição e comparação

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- As saídas de M1–M4 em formato estável (os specs respectivos).
- Margens aprovadas pelo usuário, para a etapa 4. Até lá, os testes usam margens de exemplo.

## Etapas

- [ ] 1. **Estatística descritiva** (R1, R2). Média, IC95 % t, CV, a marca "não informativo", e a
  posição e o percentil de F.
  - Testes primeiro: o caso do IC calculado à mão; uma grandeza com média ≈ 0 é marcada.
  - **Pronto quando:** `pytest` verde.
- [ ] 2. **Permutação exata** (R3). Enumeração das 184 756 atribuições (e das 92 378 divisões, na
  variante simétrica) e as duas estatísticas.
  - Testes primeiro: o critério da uniformidade de p e o do deslocamento; a contagem de divisões.
  - **Pronto quando:** `pytest` verde, e o tempo para uma matriz 20 × 20 é registrado.
- [ ] 3. **Aplicação cruzada** (R4). Consome as matrizes do M3 e do M4.
  - **Pronto quando:** com dois conjuntos sintéticos, a diferença e o IC saem corretos.
- [ ] 4. **TOST** (R5), com as margens do arquivo de configuração.
  - Testes primeiro: os casos do critério de aceitação.
  - **Pronto quando:** `pytest` verde.
- [ ] 5. **Levene e F** (R6), com o ajuste de Holm (R7).
  - Testes primeiro: o caso do desvio-padrão dobrado; Holm num conjunto conhecido.
  - **Pronto quando:** `pytest` verde.
- [ ] 6. **Relatórios** (R8). Por condição e por comparação, em Markdown com figuras.
  - **Pronto quando:** o relatório das duas condições sintéticas é gerado e revisado pelo usuário
    quanto ao formato.

## Riscos

- **Com n = 10, o TOST tem pouco poder:** a equivalência pode não ser declarada mesmo sem
  diferença real. O relatório mostra o IC90 % junto da margem, para que se veja se faltou dado ou
  se a diferença existe.
