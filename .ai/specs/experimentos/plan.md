# Plano — Execução das condições A, B e B′

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir. Cada etapa tem o seu critério de
> pronto; os dados vão para `ros2_ws/dev/calib_runs/dados/`.

## Pré-requisitos

- Concluídos e testados: `ambiente-sensores`, `persistencia-captura`, `saida-calibracao`,
  `exportar-fabrica`, `roteiro-poses`, `captura-estatica`, `cobertura`, `nucleo-offline`, M1–M4,
  `analise-estatistica` e `validacao-metrica`.
- Piloto concluído, com as recomendações aprovadas.
- `aquecimento-termico` concluído, com o X aprovado.

## Etapas

### Fase 0 — decisões e ensaio

- [ ] 1. **Preencher a tabela "Decisões a registrar antes de A"** (R11).
  - **Pronto quando:** todas as linhas têm valor e a aprovação do usuário fica registrada no spec,
    com a data.
- [ ] 2. **Exportar F** da S/N 35180697 (`zed_export_factory --verify`).
  - **Pronto quando:** `dados/fabrica/SN35180697/` existe e a verificação passou.
- [ ] 3. **Captura de ensaio** (`ensaio_A`, fora das 10): o roteiro completo, a cobertura, a
  calibração e o M1, para cronometrar e ajustar o procedimento.
  - **Pronto quando:** o ensaio passa nos critérios de R3, o tempo por captura é registrado e os
    ajustes de procedimento estão anotados neste plano.

### Fase 1 — condição A

- [ ] 4. **Medição do alvo** (R1, item 6): paquímetro e planicidade.
  - **Pronto quando:** os valores estão na sessão de A e no `metrico_A`.
- [ ] 5. **Capturas A1 a A10** (R1–R4), na ordem decidida.
  - **Pronto quando:** 10 capturas aceitas; cada uma com a cobertura aprovada, o M1 dentro do
    filtro e o checklist de preparação registrado. As reprovadas estão marcadas e preservadas.
- [ ] 6. **Sessão `metrico_A`** (`validacao-metrica`).
  - **Pronto quando:** 36 pares, com as distâncias medidas no `meta.json`.
- [ ] 7. **Análise de A** (R6, R7) e **portão A** (R9).
  - **Pronto quando:** o relatório de A está gerado, com o diagnóstico do fy/fx e a resposta do
    portão. A referência "seca" está publicada em `dados/A_ar_sem_acrilico/referencia/`. O usuário
    revisou.

### Fase 2 — condição B

- [ ] 8. **Montar a tampa uma vez** e registrar a montagem. Depois, **capturas B1 a B10** (R1–R4),
  sem mexer na tampa.
  - **Pronto quando:** 10 capturas aceitas, com a checagem de reflexos guardada em cada uma.
- [ ] 9. **Sessão `metrico_B`.**
  - **Pronto quando:** 36 pares registrados.
- [ ] 10. **Análise A × B** (R8) e **portão B** (R10).
  - **Pronto quando:** o relatório A × B responde ao portão B com os três testes, e o usuário
    decidiu sobre B′.

### Fase 3 — condicional

- [ ] 11. **B′** (somente se B falhar e o usuário decidir): publicar a referência de B como a
  calibração operacional com acrílico, em `dados/B_ar_com_acrilico/referencia/`.
  - **Pronto quando:** a referência está publicada e o relatório A × B explica por que ela substitui
    a de A com o acrílico montado.

### Encerramento

- [ ] 12. **Registro final** em `.ai/specs/experimentos/resultado.md`:
  - as decisões e os resultados dos portões;
  - a calibração recomendada para o ar com acrílico;
  - as lições para a câmera do robô e para a etapa na água.
  - **Pronto quando:** o usuário aprovou o resultado.

## Riscos

- **Tempo de bancada:** 63 poses × 20 capturas, mais o teste métrico. O ensaio (etapa 3) mede o
  tempo real. Se ele for proibitivo, reduzir o roteiro é uma decisão do usuário, registrada antes
  de A, e vale para todas as capturas.
- **Uma deriva ao longo dos dias** (temperatura da sala, montagem) se confunde com o efeito da
  condição, porque a ordem é em blocos (R5). Mitigação: a temperatura e a data no `meta.json`, e
  a procura de tendência dentro de cada bloco.
- **Uma captura reprovada várias vezes** indica um problema de procedimento ou de roteiro, e não de
  calibração. Parar e revisar antes de insistir.
