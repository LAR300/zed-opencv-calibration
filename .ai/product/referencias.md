# Pesquisa bibliográfica — calibração estéreo ZED 2i (ar → porta plana)

Relatório do agente de pesquisa, 2026-11-03. Texto integral lido: Hagemann (LaTeX arXiv), Peng & Sturm,
Treibitz (PDF do autor), Agrawal (MERL TR), Jordt-Sedlazeck ECCV, Shortis (PMC), Singh et al., She et al.
NÃO lidos: Sedlazeck & Koch 2012 (paywall) e o artigo Pinax em si (403) — substituídos pela tese de Jordt
(2014) e pela tese de Łuczyński (2017) + código Pinax.

## Referências (verificadas em Crossref/arXiv)

1. Treibitz, Schechner, Kunz, Singh. Flat Refractive Geometry. IEEE TPAMI 34(1):51–65, 2012. DOI 10.1109/TPAMI.2011.105. PDF: https://webee.technion.ac.il/~yoav/publications/jFlat_published.pdf
2. Agrawal, Ramalingam, Taguchi, Chari. A Theory of Multi-Layer Flat Refractive Geometry. CVPR 2012, 3346–3353. DOI 10.1109/CVPR.2012.6248073. PDF: https://shadow.merl.com/publications/docs/TR2012-047.pdf
3. Sedlazeck, Koch. Perspective and Non-perspective Camera Models in Underwater Imaging – Overview and Error Analysis. LNCS 7474, 212–242, Springer 2012. DOI 10.1007/978-3-642-34091-8_10. (não lido)
4. Jordt-Sedlazeck, Koch. Refractive Calibration of Underwater Cameras. ECCV 2012, LNCS 7576, 846–859. DOI 10.1007/978-3-642-33715-4_61. PDF: https://www.geomar.de/fileadmin/personal/fb2/mg/ajordt/finalEccv.pdf
5. Łuczyński, Pfingsthorn, Birk. The Pinax-model for accurate and efficient refraction correction of underwater cameras in flat-pane housings. Ocean Engineering 133:9–22, 2017. DOI 10.1016/j.oceaneng.2017.01.029. Código: https://github.com/tomluc/Pinax-camera-model. Tese: https://opus.constructor.university/files/779/TLuczynski_thesis.pdf
6. Shortis. Calibration Techniques for Accurate Measurements by Underwater Camera Systems. Sensors 15(12):30810–30826, 2015. DOI 10.3390/s151229831 (PMC4721751).
7. Hagemann, Knorr, Janssen, Stiller. Inferring Bias and Uncertainty in Camera Calibration. IJCV 130(1):17–32, 2022. DOI 10.1007/s11263-021-01528-x. arXiv 2107.13484. Sem código público.
8. Peng, Sturm. Calibration Wizard. ICCV 2019, 1497–1505. DOI 10.1109/ICCV.2019.00158. arXiv 1811.03264. Código: https://github.com/pengsongyou/CalibrationWizard
9. Singh, Dharmadhikari, Alexis. An Online Self-calibrating Refractive Camera Model with Application to Underwater Odometry. ICRA 2024, 10005–10011. DOI 10.1109/ICRA57147.2024.10610643. arXiv 2310.16658.
10. She, Seegräber, Nakath, Köser. Refractive COLMAP: Refractive Structure-from-Motion Revisited. IROS 2024, 12816–12823. DOI 10.1109/IROS58592.2024.10802043. arXiv 2403.08640.

Extras: Jordt, tese de doutorado, Kiel 2014 (https://macau.uni-kiel.de/servlets/MCRFileNodeServlet/dissertation_derivate_00005329/diss_jordt_underwater_rec.pdf);
Seegräber, She, Woelk, Köser, A Calibration Tool for Refractive Underwater Vision, arXiv 2405.18018 (toolbox aberto, porta plana e domo; só o resumo lido).

## A. Hagemann — Bias Ratio e EME

- N = número de **coordenadas** (2 × cantos). MSE_calib = RMSE², por coordenada. N_P = parâmetros livres intrínsecos + extrínsecos.
- ŝ_d² = MSE_calib / (1 − N_P/N) (Luhmann 2013).
- Decomposição: MSE_calib = σ_d²(1 − N_P/N) + ε_bias²(1 − N_P/N).
- σ_d: alvo dividido em "alvos virtuais" de 4 cantos exclusivos; pose de cada um reestimada com intrínsecos fixos (redundância 2: 8 obs, 6 parâmetros) → σ̂_d,v² = MSE_v/(1 − 6/8) = 4·MSE_v. Agregação robusta com MAD (×1,4826).
- ε̂_bias² = max(ŝ_d² − σ̂_d², 0); BR = ε̂_bias²(1 − N_P/N)/MSE_calib. Reportar BR e ε_bias. Relacionado a um teste F.
- Calibração robusta recomendada (kernel de Cauchy). Ruído real do detector deles: σ_d ≈ 0,03 px; BR detectou não planicidade de ~10⁻⁴ m com RMSE < 0,1 px.
- **A implementação proposta bate com o artigo.** Armadilhas:
  1. Convenção por coordenada em tudo. O RMS do OpenCV é por canto (norma 2D): RMS² = 2 × MSE por coordenada. Misturar convenções dá BR ≈ 0,5 numa calibração sem viés. (A confirmar no fonte do OpenCV 4.5.4.)
  2. MAD dá σ, não σ²: σ̂_d² = 4·(1,4826·MAD)², MAD sobre componentes com sinal (x e y juntos).
  3. Robusto dos dois lados (MSE_calib também).
  4. Mesmos cantos/detector/refino/modelo de distorção da calibração (cantos de tag AprilGrid são conjuntos exclusivos: ok).
  5. PnP de 4 pontos planares é ambíguo: SOLVEPNP_IPPE_SQUARE, menor resíduo, refinar com solvePnPRefineLM. Pose mal convergida infla σ_d e esconde viés.
  6. Estéreo (extrapolação do agente): N_P = 2·n_intr + 6 + 6·N_F; N conta coordenadas das duas imagens.
- Mapping error: K(θ̂, θ̄) = min_R 1/(2N_G) Σ‖u_g − p(R·p⁻¹(u_g, θ̄), θ̂)‖² (só rotação, sem translação). Densidade da grade não especificada no arXiv.
- EME = E[K] = trace(Σ_θθ H), H = (1/(2N_G)) J_resᵀJ_res. Σ padrão é otimista em dados reais; bootstrap/aBS calibram bem (superestima com N_F ≤ 15).
- Validação deles: erro de mapeamento "verdadeiro" contra referência = média de 10 calibrações com 50 imagens aleatórias cada. É o "EME empírico" do brief.

## B. Peng & Sturm
- Próxima pose minimiza trace(Σ) (complemento de Schur). Incerteza de canto via autocorrelação de Harris, depende do ângulo de abertura do canto projetado e do borrão.
- Sem o modelo de canto, a melhor pose é rasante demais; com ele: inclinação grande, alvo cobrindo o FOV e/ou tocando a borda.
- 3 + 12 imagens guiadas superam 50 livres em teste real.

## C. Porta plana
- fx_água/fx_ar ≈ n vale **só paraxial** (centro), porta perpendicular. SVP só com d = 0. Treibitz eq. 14 (d = 0): r_persp = r·[1 + (r/f_eff)²(n² − 1)]^(−1/2).
- Treibitz: não usar SVP com FOV largo (meio-ângulo > ~30°); 30→40° aumenta muito o erro. Inclinação de 2° entre sensor e porta causa erros grandes e assimétricos. Calibração SVP vale na distância de calibração; calibrar em toda a faixa de trabalho. Experimento: f_eff = 1,3 f; erro de comprimento 10–15 % (SVP) vs 1–2 % (modelo físico). Vidro: só desloca o raio lateralmente, efeito pequeno. Cáustica: Z_caustic = n·d.
- Jordt 2014: substituição perspectiva só vale na distância de calibração; RMS 0,05 px na calibração e até ~20 px em outras distâncias; inclinação absorvida pelo ponto principal.
- Shortis Tab. 1: GoPro, 3,676 → 4,922 mm (razão 1,34). Pinax: f = 1,34·f com n_w = 1,34. Singh 2024: n estimado 1,332.
- Conta do agente (Snell, porta fina, n = 1,333): lente 2,1 mm, HFOV 110° → ~76° na água; na borda m/n ≈ 1,38. Lente 4 mm, 72° → ~52°.
- Parâmetros por modelo: Treibitz (d; n conhecido; vidro desprezado); Agrawal (eixo, espessuras, índices — tudo de uma imagem; FOV pequeno torna ambíguos); Jordt (d, normal da porta por CMA-ES; espessura e índices conhecidos; calibração no ar antes); Pinax (intrínsecos no ar + d0, d1, n_g, n_w; exige d0 ≈ d0* ≈ 0,15·d1); She (normal e d_int no BA; espessura fixa).
- Pinax d0* (físico/virtual): d1 = 5 mm → 0,76/0,31; 10 mm → 1,52/0,61; 20 mm → 3,04/1,22 (água doce). Câmera o mais perto possível do vidro. Pinax só com calibração no ar superou calibrações na água; calibração de água doce usada em água salgada degradou.
- She 2024: porta ortogonal → distorção radial absorve em faixas razoáveis; falha com porta inclinada; pinhole com 0,277 px de RMS e reconstrução distorcida.
- Shortis: rede 3D, vistas convergentes, alvo enchendo o FOV, rolagem 0/90/180/270°; foco travado; **ligação rígida lente–porta** é o mais importante; barra estéreo rígida; rolling shutter → quadros parados; tabuleiro pequeno 2D limita. Nada sobre aquecimento térmico nem número de imagens. RMS típico: ar 0,03–0,1 px, água 0,1–0,3 px; validação 0,1 % (melhor) a 0,2–0,7 %; validar em distâncias maiores que a média da calibração.

## D. Índices de refração (589 nm)
- PMMA: 1,4905 (20 °C; Sultanova et al. 2009). Dispersão: 1,4926 @ 546 nm.
- Água pura: 1,3333 (20 °C; Daimon & Masumura 2007, DOI 10.1364/AO.46.003811).
- Água do mar: Quan & Fry 1995 (DOI 10.1364/AO.34.003477), função de S, T, λ. S = 35 ‰: 1,3406/1,3399/1,3389 a 5/15/25 °C (doce: 1,3339/1,3334/1,3325). Doce → salgada ≈ +0,5 % na focal.

## E. ZED 2i / SDK
- Datasheet: baseline 12 cm, 1/3" 4 MP, pixel 2 µm, rolling shutter sincronizado. Lente 2,1 mm: 110°H × 70°V; 4 mm: 72°H × 44°V.
- camera_disable_self_calib = true → usa os parâmetros raw do arquivo. Forum: autocalibração também roda em SVO se não desabilitada; parâmetros do arquivo referem-se às imagens raw.
- optional_opencv_calibration_file: YAML OpenCV com Size, K_LEFT, K_RIGHT, D_LEFT, D_RIGHT (1×14 no exemplo), R (Rodrigues 3×1), T (mm, T[0] negativo). Não renomear chaves. "SDK adapta à resolução" — não diz se considera crop/binning entre modos.
- calibration_parameters = retificado; calibration_parameters_raw = não retificado/distorcido.
