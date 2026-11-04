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

- [ ] **Fase B inteira** (plan, etapas 4–7): modo roteiro na coleta (pose esperada e grandezas na
      tela, salvamento só dentro da tolerância), teclas `o`/`k`/`b`, a captura completa das 63
      poses com o tempo registrado e a revisão das tolerâncias, e o guia de bancada. A lógica pura
      (Fase A) está pronta e testada.

## captura-estatica

- [ ] **`--measure_static_noise`** (plan, etapa 4): medir com o alvo parado e trocar os limiares
      provisórios de `config/session_example.yaml` (1,0 s, 0,5 px, nitidez 0) pelos medidos.
- [ ] **Integração na coleta** (plan, etapa 5): painel, bloqueio do `s` e campos no `meta.json`,
      com a regressão do xadrez idêntica.

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
- [ ] **Perguntas do spec ao usuário:** instrumento e ponto de referência da distância, distância
      mínima, instrumento da planicidade.

## aquecimento-termico

- [ ] **`--timelapse 30 60` com o alvo** (plan, etapa 1): 120 pares espaçados 30 ± 1 s, com as
      temperaturas no `meta.json`. Sem o alvo, o modo já foi exercitado ao vivo.
- [ ] **Três registros a frio** em dias diferentes (plan, etapa 4), câmera desligada há ≥ 2 h,
      controles manuais (`--camera_settings`), luz fixa; depois `zedcal.thermal_command` e o X
      proposto para aprovação.
