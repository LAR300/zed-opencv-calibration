# Fluxo de calibração — visão geral

Como calibrar a ZED 2i de forma rastreável e comparável, do preparo da bancada à decisão. O
fluxo tem duas partes, cada uma num documento:

1. **Coleta** ([`fluxo-coleta.md`](fluxo-coleta.md)): com a câmera. A ferramenta
   `zed_stereo_calibration` guia as poses, grava tudo e, no fim, **calibra**, como sempre fez.
2. **Análise offline** ([`fluxo-analise-offline.md`](fluxo-analise-offline.md)): sem a câmera,
   sobre os arquivos gravados. Mostra se o erro é ruído ou viés, se a captura cobre a imagem, se a
   calibração generaliza, quanto erra em milímetros e se é melhor que a de fábrica.

O que cada funcionalidade e cada métrica faz, e de onde veio:
[`funcionalidades.md`](funcionalidades.md).

## Antes × agora

**Sem `--session`, a ferramenta funciona exatamente como antes:** você captura, ela calibra e
mostra o RMS e os parâmetros. O fluxo novo acrescenta o que vem antes e depois:

| | Antes | Agora |
|---|---|---|
| Ferramenta | `zed_stereo_calibration` | A mesma, com `--session sessao.yaml` |
| Poses | Livres | Roteiro fixo de 63 poses, pedidas pela tela |
| Quando salva | Na tecla | Só com o alvo parado, nítido e na pose pedida |
| Resultado | YAML + RMS no terminal | O mesmo, mais o relatório e todos os dados brutos |
| Onde grava | `zed-images/`, apagada a cada coleta | Uma pasta por captura, que nunca é apagada |
| Depois | Nada | Cobertura, métricas M1–M4 e estatística, offline |
| Quantas | Uma | 10 por condição, para a estatística |

**Por quê:** um RMS baixo numa calibração isolada não prova que ela está certa. Nas capturas
AprilGrid do laboratório o RMS era ~0,31 px, mas ~70 % do erro era viés do alvo impresso. Para
comparar com a calibração de fábrica, ou medir o efeito do acrílico, é preciso capturas
repetíveis, dados guardados e várias capturas por condição.

## O fluxo completo

```
uma vez por câmera      uma vez por condição          cada captura (×10)                   no fim da condição
──────────────────      ────────────────────          ──────────────────                   ──────────────────
acesso ao SDK/HID   →   medir o alvo            →     sessao.yaml (capture: i)        →    M1 e M4 por captura
exportar F              limiares de imobilidade       coleta com o roteiro                 M2, M3 do conjunto
                        aquecer a câmera (todo dia)   → a ferramenta calibra               relatório e portões
                                                      cobertura: aceita ou refaz
└──────────────────────────── fluxo-coleta.md ────────────────────────────┘   └── fluxo-analise-offline.md ──┘
```

## Os experimentos

O protocolo que usa este fluxo está em
[`../specs/experimentos/spec.md`](../specs/experimentos/spec.md):

| Condição | O que é | Portão |
|---|---|---|
| A | No ar, sem acrílico: 10 capturas + teste métrico | Nossa calibração empata ou supera F na validação? |
| B | No ar, com a tampa de acrílico montada uma vez: 10 capturas + teste métrico | B equivale a A? |
| B′ | Só se B reprovar: a calibração com acrílico vira a operacional | — |
| Robô | O mesmo protocolo na câmera do robô (outro serial, outra F) | — |
| Água | Futuro: C1 (pinhole absorvendo a refração) e C2 (modelo refrativo) | — |

As capturas já feitas indicam que B vai reprovar: a porta muda a escala em ~0,9 %.

## Antes da condição A

- **Decidir o alvo:** um alvo de precisão em base rígida, estimar a geometria do alvo junto com a
  calibração, ou manter o atual aceitando o viés. Em qualquer caso, medir largura × altura.
- **Medir na bancada** o tempo de aquecimento e os limiares de imobilidade com o alvo do
  laboratório.
- **Decidir** o N mínimo da cobertura, as margens do TOST e a faixa "perto" do roteiro.

Lista completa: [`../product/pendencias-bancada.md`](../product/pendencias-bancada.md).
