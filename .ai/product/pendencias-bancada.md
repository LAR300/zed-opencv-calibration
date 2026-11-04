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
