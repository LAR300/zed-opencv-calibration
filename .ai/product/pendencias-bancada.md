# Pendências de bancada

> Verificações que precisam do laboratório: do alvo AprilGrid diante da câmera, ou de uma condição
> que não havia quando a feature foi feita. Cada item aponta o critério do spec que ficou parcial.
> Feito o teste, marque `[x]`, registre o resultado no `plan.md` do spec e no critério.

## persistencia-captura

- [ ] **Captura com o alvo em movimento,** com pelo menos 3 pares salvos. Com o alvo parado, só o
      primeiro par passa no filtro de dissimilaridade (plan, etapa 7).
- [ ] **Calibração gravada na pasta da captura:** uma captura de sessão com 20 pares ou mais, até a
      calibração rodar, conferindo o YAML em `captura_<XX>/` e nada no diretório corrente (spec,
      critério 2).
- [ ] **Temperaturas `null` sem o HID:** com o HID da câmera sem permissão, o `meta.json` traz
      `null` e o aviso aparece uma vez (spec, critério 7).
      - Depois do reinício de 2026-11-03, o `hidraw` da ZED 2i apareceu como `rw-rw-rw-` sem
        nenhuma regra udev da Stereolabs no host. É preciso entender de onde vem essa permissão
        (spec `ambiente-sensores`).

## exportar-fabrica

- [ ] **Checker ao vivo com F:** o `zed_stereo_checker` com a calibração interna e com
      `--calib_opencv calib_factory_rational8.yml`, alvo parado, ~30 quadros cada. A comparação nos
      pares gravados deu 0,0 px de diferença (plan, etapa 4).

## saida-calibracao

- [ ] **Calibração de uma captura de sessão ao vivo,** com 20 pares ou mais: o relatório e o
      `calib_cpp_rational8.yml` saem na pasta da captura, com `prior: none` e com `prior: factory`
      (este com o chute inicial do SDK, e não da F exportada). Ao vivo, isso cobre também o item
      "calibração gravada na pasta da captura" da `persistencia-captura`. Sem a câmera, isso foi
      verificado com `--recalibrate`.

## roteiro-poses

- [ ] **Modo roteiro com o alvo** (plan, etapas 4–5; implementado em 2026-11-04). Primeira tentativa
      em 2026-11-04, com um AprilGrid 5×7 de 45 mm (26 × 37 cm) e uma variante do roteiro com
      +90° de rotação: o painel mediu a pose (rotação 4° com o alvo em pé, inclinações, distância
      0,55 m) e recusou o `s` com os motivos certos; nenhum par salvo. Aprendido: o contorno amarelo
      cortado nas poses de borda confundiu o operador (explicado em `fluxo-coleta.md`), e com um alvo
      pequeno as poses "perto" nos cantos não chegam a 12 tags nas duas câmeras. Falta: as 9 poses do
      bloco 1 "perto" salvas com os ids certos; uma pose do bloco 3 a 90° salva logo depois da de
      0°; `o` salvando com `out_of_tolerance: true`; conferir no painel se as grandezas medidas
      batem com o que se vê (sinais das inclinações e da rotação).
- [ ] **Captura completa das 63 poses** (plan, etapa 6), com o tempo, a revisão das tolerâncias
      e a decisão sobre a faixa "perto" (plan, riscos); depois o guia de bancada (etapa 7).
      - **2026-11-10, não concluída:** com o alvo do laboratório, nenhuma pose salva no modo
        roteiro. A imobilidade não fechou nem com 0,5 px (veja captura-estatica). A
        `dados/ensaio_A/captura_01` tem as 63 poses puladas com `k` e está marcada `REPROVADA`.
        No lugar dela, duas capturas em sessão livre guiadas por `config/roteiro_v1_guia.html`
        (`dados/ensaio_A_livre/captura_01..02`, 41 e 42 pares), ambas aceitas: cobertura
        aprovada, RMS estéreo 0,396 e 0,407 px. Comentário em
        `calib_runs/dados/analises/ensaio_A_livre/report_comentado.md`. Os sinais das
        inclinações no painel continuam sem conferência.

## captura-estatica

- [ ] **`--measure_static_noise 30`** (plan, etapa 4; implementado): com o alvo parado, nas
      distâncias P, M e L, e trocar os limiares provisórios de `config/session_example.yaml`
      (1,0 s, 0,5 px, nitidez 0) pelos propostos no `noise.json`, revistos.
      - **Feito em parte em 2026-11-04**, com um AprilGrid 5×7 de 45 mm a ~0,4 m: com os controles
        fixos (`--camera_settings`), deslocamento p50 0,32 px e p95 0,50 px, nitidez mínima
        proposta 1428; com a exposição automática, p50 0,52 e p95 0,78 px. Falta medir com o alvo
        do laboratório nas três distâncias.
      - **Medido em 2026-11-10** com o alvo do laboratório, câmera aquecida, controles fixos
        (`calib_runs/dados/ruido/captura_01..03` = L ~1,2 m, M ~0,6 m, P ~0,4 m; comentado em
        `calib_runs/dados/analises/ruido_estatico/report_comentado.md`). Deslocamento p95:
        0,028 / 0,019 / 0,048 px. Nitidez p05 (esquerda): 6688 / 2767 / 1516, porque a medida
        cai com a proximidade. Pior caso (P) em `sessao_ensaio_A.yaml`: 0,145 px (3 × p95) e 757
        (0,5 × p05), janela de 1,0 s. Falta o `config/session_example.yaml`, depois do ensaio.
- [ ] **Corrigir a proposta do `noise.json`:** a ferramenta propõe `max_displacement_px` =
      1 × p95, mas o spec (condições de implementação) diz 3 × p95, e o plan, etapa 4, descreve
      1× como se fosse a regra do spec. Ajustar `proposeThresholds`, o teste e o plan.
- [ ] **A nitidez mínima depende da distância** (2026-11-10: ~6700 em L e ~1500 em P, com o alvo
      parado). Um limiar único, o de P, fica frouxo em L. Avaliar a normalização pelo tamanho
      das tags em pixels, depois do ensaio.
- [ ] **Integração na coleta com o alvo** (plan, etapa 5): no modo roteiro, o bloqueio do `s`
      até a imobilidade, com os limiares medidos na etapa 4.
      - **2026-11-10:** com 0,145 px e depois com 0,5 px, o operador não conseguiu fechar a janela
        de 1 s. A causa não foi diagnosticada. Hipóteses:
        - vibração real do conjunto (tripé, alvo encostado);
        - o RMS dos cantos não ser robusto: um único canto instável, por exemplo de uma tag cortada
          na borda nas poses P, levanta o RMS e reinicia a janela.

        Próximo passo: ler o "moving (x px)" do painel com tudo parado. Se aparecerem picos
        isolados, trocar o RMS pela mediana em `StabilityTracker::update`. O limiar sai do
        orçamento de erro (deformação ≤ d no pior caso) ou da relação erro por vista ×
        deslocamento numa captura com limiar folgado.
- [ ] **Par aceito ao vivo e perdido na releitura:** em todas as 5 capturas livres
      (`dados/ensaio_A_livre`), 1 ou 2 pares foram salvos com ≥ 12 tags ao vivo e, relidos do
      PNG, deram 10 ou 11 tags: captura_01 #31, 02 #32, 03 #32, 04 #27, 05 #30 e #65. A causa não
      foi investigada: a detecção ao vivo e a do arquivo salvo divergem perto do mínimo de 12. Hoje
      a calibração só descarta o par.
- [ ] **Vista `free_1` removida como outlier** nas capturas livres 04 e 05, com RMS de 1,26–1,27 px
      contra o limiar de ~0,84. A 03 removeu outras vistas (`free_30`, `free_61`). Ver se é sempre a
      mesma pose do guia e por quê: canto da imagem, movimento sem o filtro de imobilidade?
- [ ] **Filtro de similaridade satura na sessão livre** (2026-11-12, captura_05 com
      `--min_samples 200`): depois de 75 pares, 31 `s` seguidos foram recusados por "similar".
      É o filtro original: uma amostra nova é recusada se |Δx| + |Δy| + |Δsize| + |Δskew| < 0,15
      em relação a qualquer amostra salva. Não há teto de amostras. Se for preciso coletar mais,
      avaliar uma opção para relaxar ou desligar o filtro na sessão livre.

## cobertura

- [ ] **Rever N depois da primeira captura completa com roteiro** (plan, risco). O N foi escolhido
      a partir de capturas que não seguiram o roteiro.

## validacao-metrica

- [ ] **Distância medida na coleta** (plan, etapa 2): ao trocar de distância, pedir o valor, o
      instrumento, o ponto de referência e a incerteza, e gravar em
      `frames[k].measured_distance` (formato no plan, etapa 4). Chamar
      `checkMetricTargetMeasured` ao abrir a sessão métrica.
- [ ] **Sessão métrica real** (plan, etapa 5): rodar `zedcal.metric_test_command` com F e uma
      calibração existente.
      - **2026-11-13:** é o que decide a profundidade longe. Contra F, as nossas calibrações (AprilGrid
        e xadrez) têm deslocamento de disparidade de +0,1 a +0,2 px e, as AprilGrid, escala ~0,6 %
        diferente: −0,5 % a 3 m. Afastar o alvo 1, 2 e 3 m medidos com trena (diferenças de
        distância, sem depender do centro óptico) basta. Uma versão em sessão livre dispensa o modo
        roteiro (relatório final das 9 capturas livres, seção 6.6).
- [ ] **Perguntas do spec ao usuário:** instrumento e ponto de referência da distância, distância
      mínima, instrumento da planicidade.

## aquecimento-termico

- [x] **`--timelapse 30 60` com o alvo** (plan, etapa 1): 120 pares espaçados 30 ± 1 s, com as
      temperaturas no `meta.json`. Sem o alvo, o modo já foi exercitado ao vivo. Feito em
      2026-11-10 (dia 1, `calib_runs/dados/termico/captura_01`): 120 pares, espaçamento
      29,94–30,07 s, temperatura em todos.
- [ ] **Três registros a frio** em dias diferentes (plan, etapa 4), câmera desligada há ≥ 2 h,
      controles manuais (`--camera_settings`), luz fixa; depois `zedcal.thermal_command` e o X
      proposto para aprovação. **Dia 1 (2026-11-10, 11:06–12:06, alvo a ~0,91 m):** sem regime
      em 60 min (`calib_runs/dados/analises/termico_dia1/`). A temperatura (ONBOARD_LEFT) subiu
      de 26,8 °C para 32,4 °C, ainda +0,25 °C nos últimos 20 min. A transferência estéreo, que
      não depende de movimento rígido, assentou o RMS aos 22 min e o dy aos 4 min; o dx (deriva
      de disparidade) desceu 0,04 px até o fim, ~0,5 mm em 0,91 m com F. A pose deriva até o fim
      (ry +0,20°, tx −0,5 mm), mas o alvo estava só encostado na parede: a pose não separa a
      câmera do alvo escorregando (degrau em rx aos ~8,5 min). Próximos registros: alvo
      preso; decidir se ficam com 90–120 min.

## ambiente-sensores

- [ ] **Instalar a regra udev no host** (`scripts/bench/install_zed_udev.sh`, pede `sudo`). Em
      2026-11-04 o `--check` no host passou em tudo menos na regra (HID `0666` sem regra, perde-se
      ao reconectar); o mesmo script dentro do container falha com "container not running", porque
      roda no host. Depois de instalar,
      reconectar a câmera e rodar `scripts/bench/check_zed_access.sh --check`; uma segunda
      instalação não deve mudar nada (plan, etapa 2).
- [ ] **Recriar o container** e conferir que o `--check` reprova o SDK e que
      `scripts/bench/fix_zed_group.sh` o conserta (plan, etapas 1 e 3).

## diagnostico-vies-aprilgrid

- [ ] **Medir largura e altura do alvo** (régua de aço ou trena boa, de borda externa a borda
      externa das tags): o offline prevê o eixo Y do alvo ~0,23 % (~1,5 mm em 660 mm) maior que o
      X, em relação à grade nominal.
- [ ] **Planicidade** do alvo (já pedida em `validacao-metrica` R4).
- [ ] **Alvo apoiado numa base plana e rígida** (MDF, vidro ou chapa), sem segurar pelas bordas:
      repetir 2–3 capturas e comparar fx e fy com o xadrez e com F. Em 2026-11-13, mesmo com o
      alvo corrigido (esticamento + mapa por canto), as focais ficaram ~0,3 % acima de F, enquanto o
      xadrez fica a ~0,1 %. A hipótese é o painel empenado (relatório final das 9 capturas livres,
      seção 8.2: `calib_runs/dados/analises/ensaio_A_livre/relatorio_final/relatorio.md`).
- [ ] **Captura com o alvo girado 90° em metade das vistas** (bloco 3 do roteiro): é o que separa
      o esticamento do alvo do fy/fx sem instrumento. Nas 9 capturas livres só 5 de 626 vistas têm
      o alvo girado, e a estimativa só pelos dados variou de −0,4 a +0,8 %. Hoje o filtro de
      similaridade recusa essas vistas: precisa da opção para relaxá-lo na sessão livre, ou do modo
      roteiro funcionando (relatório final, seção 8.1).
