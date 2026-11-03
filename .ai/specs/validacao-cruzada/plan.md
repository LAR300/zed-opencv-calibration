# Plano — M3: validação cruzada entre capturas

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`nucleo-offline`](../nucleo-offline/plan.md) concluído.
- [`metrica-ajuste`](../metrica-ajuste/plan.md), etapa 2 (RMS com pose livre), para o teste da
  diagonal.

## Etapas

- [ ] 1. **Avaliação de C em J** (R1). PnP na esquerda, RMS da esquerda, transferência na direita e
  combinado, com a mediana e o p90 por vista.
  - Testes primeiro: os três casos sintéticos do critério.
  - **Pronto quando:** `pytest` verde.
- [ ] 2. **Matriz LOCO** (R2, R3). Montagem n × n; linhas de F e da média; resumo diagonal × fora da
  diagonal.
  - Testes primeiro: o critério da diagonal = treino; n = 3 sintético dá a matriz 3 × 3 com o
    resumo correto.
  - **Pronto quando:** `pytest` verde.
- [ ] 3. **Aplicação cruzada** (R4).
  - Testes primeiro: com dois conjuntos sintéticos, a forma e o resumo saem corretos.
  - **Pronto quando:** `pytest` verde.
- [ ] 4. **Por distância e por região** (R5). Agregação por faixa e por célula, com figuras.
  - **Pronto quando:** a figura de um caso sintético com erro só na borda mostra o erro nas células
    da borda.
- [ ] 5. **Comando e config** (R6). `python3 -m zedcal.m3 --config ...`.
  - **Pronto quando:** roda com as 3 capturas AprilGrid existentes, tratadas como uma "condição"
    de teste, e F.

## Riscos

- **O tempo de execução cresce com n².** Para n = 10 e ~63 pares por captura, são ~6 300 PnPs por
  matriz, o que é aceitável. Paralelizar por linha se for preciso.
