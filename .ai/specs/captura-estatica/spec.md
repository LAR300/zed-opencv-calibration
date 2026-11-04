# Spec — Captura estática: imobilidade e nitidez na região do alvo

> **O quê, por quê e condições de implementação.** Origem: [`brief.md`](../../product/brief.md)
> R2 ("Captura estática: alvo e câmera parados ~1 s antes de gravar; descartar quadros
> borrados"); [`auditoria.md`](../../auditoria.md) §1.3 e §6 (risco 7). Depende de
> [`persistencia-captura`](../persistencia-captura/spec.md). É usado por
> [`roteiro-poses`](../roteiro-poses/spec.md).

## Problema

A ZED 2i tem **rolling shutter**. As linhas da imagem são lidas em instantes diferentes, então um
alvo em movimento sai **deformado**, e não só borrado (datasheet em
[`referencias.md`](../../product/referencias.md) §E). Shortis (2015) recomenda usar só quadros
parados.

Hoje:
- **não há checagem de imobilidade;** o quadro salvo é o do instante da tecla;
- **a nitidez é a variância do Laplaciano da imagem inteira**, com limiar 100
  (`stereo_calibration/src/main.cpp:78`, `:579-586`, `:963-965`). Um fundo nítido passa no teste
  com o alvo borrado.

Na bancada o alvo é movido à mão, e é exatamente ele que importa.

## Requisitos

- **R1. Imobilidade.** A ferramenta acompanha os cantos das tags comuns entre quadros consecutivos.
  - O quadro é **estável** quando o deslocamento RMS desses cantos, entre cada quadro e o
    anterior, fica abaixo de um limiar durante uma janela de duração T.
  - Padrão: T = 1,0 s, o "~1 s" do brief, ou 15 quadros a 15 fps.
  - Vale nas duas câmeras.
- **R2. Nitidez na região do alvo.** Variância do Laplaciano calculada **só nos polígonos das
  tags detectadas**, por câmera, com limiar próprio.
- **R3. Limiares em arquivo.** O deslocamento máximo (px), T (s) e a nitidez mínima ficam na seção
  `static_capture` do arquivo de sessão, sem valor fixo no código.
- **R4. Indicação ao operador.** O painel mostra o estado (parado há x s / em movimento) e a
  nitidez do alvo. Com `s` antes da estabilidade, aparece "aguarde imobilidade" e nada é salvo.
- **R5. O quadro salvo é o último da janela estável,** e não o do instante da tecla.
- **R6. Registro.** O `meta.json` do par traz:
  - o deslocamento RMS máximo na janela;
  - a duração da janela;
  - a nitidez do alvo E e D;
  - os limiares em uso.
- **R7. Calibração dos limiares.** Um modo `--measure_static_noise` grava, com o alvo parado,
  a distribuição do deslocamento entre quadros e da nitidez, em `noise.json` na pasta da captura.
  É daí que saem os limiares padrão.
- **R8.** Sem sessão, o comportamento antigo (nitidez da imagem inteira) não muda.

## Condições de implementação

- **O limiar de deslocamento é derivado do ruído medido, não de literatura.** Proposta: 3 × o
  percentil 95 do deslocamento entre quadros com o alvo parado (R7), revista na bancada. Um
  deslocamento abaixo do ruído do detector é indistinguível de imobilidade.
- **As funções de acompanhamento e de nitidez por polígono ficam em módulo puro e testável**, junto
  de `common/capture_io/` ou ao lado.
- **Custo.** A detecção já roda em todo quadro. O acompanhamento só compara os cantos das tags
  comuns, sem rastreamento óptico.

## Não-objetivos

- Corrigir o rolling shutter.
- Automatizar o disparo: o salvamento continua pedindo `s`. Um disparo automático ao estabilizar
  pode vir depois.

## Critérios de aceitação

- [x] **Imobilidade, com sequências sintéticas de cantos:**
  - parado com ruído σ fica estável depois de T;
  - um deslocamento de 1 px no meio da janela reinicia a contagem;
  - uma tag que some e volta não quebra o cálculo, que usa as comuns em cada par de quadros.
- [x] **Nitidez por polígono, com imagens sintéticas:** um alvo borrado sobre um fundo nítido é
      reprovado, e um alvo nítido sobre um fundo liso é aprovado.
- [ ] Na bancada, `--measure_static_noise` grava `noise.json`, e os limiares padrão saem dele.
- [ ] Na bancada, mexer o alvo e apertar `s` não salva; parar por ~1 s e apertar `s` salva o
      último quadro da janela, com os campos de R6 no `meta.json`.
- [x] Sem sessão, a regressão do xadrez é idêntica (2026-11-04, depois do modo roteiro e do
      `--measure_static_noise`).

## Perguntas em aberto

Nenhuma.
