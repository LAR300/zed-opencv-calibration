# Plano — Captura estática

> Requisitos em [`spec.md`](spec.md) (Rn). Marque `[x]` ao concluir.

## Pré-requisitos

- [`persistencia-captura`](../persistencia-captura/plan.md) concluído.

## Etapas

- [x] 1. **Acompanhamento de cantos** (R1). `common/capture_io/static_capture.{hpp,cpp}` (`StabilityTracker`). Os três casos sintéticos passam, mais o de cantos comuns insuficientes ("indeterminado").

  Especificação original da etapa: `StabilityTracker`: recebe as detecções E/D por quadro,
  com o timestamp; devolve o deslocamento RMS das tags comuns ao quadro anterior e o tempo estável
  acumulado.
  - Testes primeiro: os três casos sintéticos do critério de imobilidade.
  - **Pronto quando:** `ctest` verde.
- [x] 2. **Nitidez por polígono** (R2). `targetSharpness`. Testes: com o alvo borrado sobre um fundo nítido, a medida da imagem inteira muda < 10 % e a medida nas tags cai > 5×; com o alvo nítido sobre um fundo liso, a medida nas tags fica > 5× a da imagem inteira.

  Especificação original da etapa: Variância do Laplaciano na máscara dos polígonos das tags.
  - Testes primeiro: os dois casos sintéticos do critério de nitidez.
  - **Pronto quando:** `ctest` verde.
- [x] 3. **Seção `static_capture` da sessão** (R3). `StaticCaptureConfig`, opcional, copiado no `meta.json` e no exemplo. Os padrões são **provisórios**: janela de 1,0 s, 0,5 px e nitidez mínima 0 (não conferida), até a medida de bancada (etapa 4).

  Especificação original da etapa:
  - Testes primeiro: valores ausentes usam os padrões; valores ≤ 0 são recusados.
  - **Pronto quando:** `ctest` verde.
- [ ] 4. **Modo `--measure_static_noise <s>`** (R7). **Implementado em 2026-11-04; falta a medida
  com o alvo.** Só com `--session` (sem `roteiro`, `--timelapse` ou `--svo`); grava
  `noise.json` na pasta da captura com as distribuições (n, p05, p50, p90, p95, p99, máx.) do
  deslocamento entre quadros e da nitidez E/D, e os limiares propostos: `max_displacement_px` =
  p95 do deslocamento (condição de implementação do spec); `min_target_sharpness` = 0,5 × o
  menor p05 das duas câmeras (proposta minha, a revisar com os dados). Nada é salvo nem
  calibrado; `end_reason = static noise measured`. `common/capture_io/static_noise.{hpp,cpp}`,
  testes em `tests/test_static_noise.cpp` (percentis iguais aos do NumPy). Verificado ao vivo
  sem o alvo: 91 quadros em 6 s, nenhuma proposta, arquivo com nulos.

  Especificação original da etapa: Com o alvo parado, grava N segundos de deslocamento
  e de nitidez em `noise.json`.
  - **Pronto quando:** na bancada, o arquivo existe com percentis, e os limiares padrão de
    `config/session_example.yaml` são atualizados a partir dele.
- [ ] 5. **Integração na coleta** (R4–R6, R8). **Feita no modo roteiro** (2026-11-04): painel
  ("still for x s" / "moving"), nitidez do alvo E/D, `s` e `o` recusados sem imobilidade ou
  nitidez, o quadro salvo é o corrente da janela estável, e `still` no `meta.json` com os
  limiares. **Decisão:** a captura de sessão livre continua sem esse bloqueio até os limiares
  serem medidos (etapa 4); com os limiares provisórios, ela poderia recusar todo par.

  Especificação original da etapa: Painel, bloqueio do `s`, quadro salvo do fim da
  janela, campos no `meta.json`.
  - **Pronto quando:** o teste de bancada do critério de aceitação passa e a regressão do xadrez é
    idêntica.

## Riscos

- **Com as inclinações fortes do bloco 2, poucas tags comuns** deixam o deslocamento RMS ruidoso.
  Exigir um mínimo de cantos comuns no acompanhamento e mostrar "indeterminado" abaixo dele.
