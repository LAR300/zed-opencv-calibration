"""python3 -m zedcal.report_command --config <file>: report of one condition, or comparison of
two (spec analise-estatistica, R8).

Config (YAML):
  conditions:                      # one = condition report; two = comparison (the 1st is A)
    A: {calibrations: {c01: <calib.yml>, ...}, detections: [<detections.json>, ...]}
    B: {calibrations: {...}}
  factory: <calib_factory.yml>     # optional: F, informative (R2)
  m3_json: <m3.json>               # optional: matrix with the calibrations of both conditions as
                                   # rows and their captures as columns (same names), for R4
  m4_json: <m4.json>               # optional: per-calibration metric error (pooled span mean)
  margins:                         # TOST margins (R5); every one optional
    fx_left: 2.7
    baseline_mm: 0.6
    metric_span_pct: 0.5
  margins_approved: false          # printed in the report: provisional until the user approves
  alpha: 0.05
  mapping_step_px: 20
  output_dir: <folder; never over existing content>
Writes report.md, report.json and the figures.
"""
import argparse
import json
import os
import subprocess
import sys

import numpy as np

from . import config, io, m2, stats
from .calib_file import read_calibration
from .residuals import left_camera, right_camera

MAIN = ["fx_left", "fy_left", "cx_left", "cy_left", "fx_right", "fy_right", "cx_right", "cy_right",
        "baseline_mm", "ty_mm", "tz_mm", "rx_deg", "ry_deg", "rz_deg"]


def _code_version() -> dict:
    here = os.path.dirname(os.path.abspath(__file__))
    try:
        commit = subprocess.run(["git", "-C", here, "rev-parse", "HEAD"], capture_output=True, text=True,
                                check=True).stdout.strip()
        dirty = bool(subprocess.run(["git", "-C", here, "status", "--porcelain", "--", "."],
                                    capture_output=True, text=True).stdout.strip())
        return {"commit": commit, "dirty": dirty}
    except (OSError, subprocess.CalledProcessError):
        return {"commit": None, "dirty": None}


def _region(paths, size, step):
    if not paths:
        return None
    grid = m2.pixel_grid(size, step)
    corners = {"left": [], "right": []}
    for p in paths:
        for f in io.load_detections(p).frames:
            for side in corners:
                tags = getattr(f, side)
                if tags:
                    corners[side].append(np.concatenate(list(tags.values())))
    return {s: m2.coverage_mask(grid, np.concatenate(c)) for s, c in corners.items() if c}


def _condition(name, entry, factory, step):
    calibs = {n: read_calibration(p) for n, p in entry["calibrations"].items()}
    cl = list(calibs.values())
    q = {n: stats.calibration_quantities(c) for n, c in calibs.items()}
    fq = stats.calibration_quantities(factory) if factory is not None else {}
    table = {k: stats.describe([q[n][k] for n in q], k, fq.get(k)) for k in next(iter(q.values()))}
    ref, _, fit = m2.reference_calibration(cl, step)
    region = _region(entry.get("detections"), cl[0].image_size, step)
    eme = {}
    for side in ("left", "right"):
        mask = None if region is None else region.get(side)
        e = m2.empirical_eme(cl, ref, side, step, region=mask)
        e["errors_px"] = [float(x) for x in e["errors_px"]]
        if factory is not None:
            pick = left_camera if side == "left" else right_camera
            e["factory_px"] = m2.mapping_error(pick(factory), pick(ref), cl[0].image_size, step,
                                               region=mask).rms_per_point_px
        eme[side] = e
    return {"names": list(calibs), "calibrations": calibs, "quantities": q, "table": table,
            "eme": eme, "eme_region": "covered" if region else "full_image", "region": region,
            "reference_fit": fit}


def _comparison(cfg, A, B, a, b, step):
    calibs = list(A["calibrations"].values()) + list(B["calibrations"].values())
    labels = [a] * len(A["names"]) + [b] * len(B["names"])
    out = {"permutation": {}, "tost": {}, "dispersion": {}}
    for side in ("left", "right"):
        region = None
        if A["region"] and B["region"]:
            region = A["region"][side] & B["region"][side]
        M = m2.pair_matrix(calibs, side, step, region=region)
        r = stats.permutation_test(M, labels, a, b)
        out["permutation"][side] = r
    margins = cfg.get("margins") or {}
    values = {k: ([A["quantities"][n][k] for n in A["names"]], [B["quantities"][n][k] for n in B["names"]])
              for k in A["table"]}
    if cfg.get("m4_json"):
        m4 = json.load(open(cfg["m4_json"]))["pooled"]
        try:
            values["metric_span_pct"] = ([m4[n]["metric_pct"]["span"]["mean"] for n in A["names"]],
                                         [m4[n]["metric_pct"]["span"]["mean"] for n in B["names"]])
        except KeyError as e:
            raise config.ConfigError("m4_json has no calibration %s" % e)
    alpha = float(cfg.get("alpha", 0.05))
    for k, margin in margins.items():
        if k not in values:
            raise config.ConfigError("margin for unknown quantity %s" % k)
        out["tost"][k] = stats.tost(*values[k], margin=float(margin), alpha=alpha)
    for k in list(margins) + ["eme_left", "eme_right"]:
        if k.startswith("eme_"):
            side = k[4:]
            va, vb = A["eme"][side]["errors_px"], B["eme"][side]["errors_px"]
        else:
            va, vb = values[k]
        out["dispersion"][k] = stats.dispersion(va, vb)
    if cfg.get("m3_json"):
        m3 = json.load(open(cfg["m3_json"]))
        cond = {n: a for n in A["names"]}
        cond.update({n: b for n in B["names"]})
        out["cross_application"] = {m: stats.cross_application(np.array(v, float), m3["rows"], m3["cols"], cond, cond)
                                    for m, v in m3["matrices"].items()}
    tests = ([("permutation %s (main)" % s, out["permutation"][s]["main"]["p_value"]) for s in ("left", "right")]
             + [("TOST %s" % k, t["p_value"]) for k, t in out["tost"].items()]
             + [("Brown-Forsythe %s" % k, d["brown_forsythe"]["p_value"]) for k, d in out["dispersion"].items()]
             + [("F ratio %s" % k, d["f_ratio"]["p_value"]) for k, d in out["dispersion"].items()])
    adj = stats.holm([p for _, p in tests])
    out["p_values"] = [{"test": t, "p": p, "p_holm": float(h)} for (t, p), h in zip(tests, adj)]
    return out


def _fmt(v, nd=4):
    if v is None:
        return "—"
    if isinstance(v, bool):
        return "sim" if v else "não"
    return ("%%.%dg" % nd) % v


def _figures(out, conds, comp, factory_q):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    keys = MAIN[:9]
    fig, axes = plt.subplots(3, 3, figsize=(13, 9))
    for ax, k in zip(axes.ravel(), keys):
        for i, (c, d) in enumerate(conds.items()):
            v = [d["quantities"][n][k] for n in d["names"]]
            ax.plot(np.full(len(v), i) + np.linspace(-0.15, 0.15, len(v)), v, "o", ms=4)
            lo, hi = d["table"][k]["ci95"]
            ax.errorbar(i + 0.3, d["table"][k]["mean"], yerr=[[d["table"][k]["mean"] - lo], [hi - d["table"][k]["mean"]]],
                        fmt="s", color="k", capsize=4)
        if factory_q:
            ax.axhline(factory_q[k], color="r", linestyle="--", label="F")
        ax.set_xticks(range(len(conds)))
        ax.set_xticklabels(list(conds))
        ax.set_title(k)
    fig.tight_layout()
    fig.savefig(os.path.join(out, "parameters.png"), dpi=100)
    plt.close(fig)
    if comp is None:
        return
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))
    for ax, side in zip(axes, ("left", "right")):
        r = comp["permutation"][side]["main"]
        ax.hist(r["null"], bins=80, color="0.6")
        ax.axvline(r["statistic"], color="r")
        ax.set_title("permutation null, %s camera: p = %.2g" % (side, r["p_value"]))
        ax.set_xlabel("mean(A-B) - mean(A-A) mapping error [px]")
    fig.tight_layout()
    fig.savefig(os.path.join(out, "permutation.png"), dpi=100)
    plt.close(fig)


def _markdown(cfg, conds, comp, factory_q, version) -> str:
    L = ["# Relatório: %s" % " × ".join(conds), ""]
    L.append("Código `%s`%s. Configuração em `config.yaml`." % (version["commit"], " (com mudanças locais)" if version["dirty"] else ""))
    L.append("")
    for c, d in conds.items():
        L += ["## Condição %s (n = %d)" % (c, len(d["names"])), "",
              "| Grandeza | Média | IC95 % | DP | CV | F | F no IC95 % | Percentil de F |",
              "|---|---:|---|---:|---:|---:|---|---:|"]
        for k, t in d["table"].items():
            L.append("| %s | %s | [%s, %s] | %s | %s | %s | %s | %s |" % (
                k, _fmt(t["mean"], 7), _fmt(t["ci95"][0], 7), _fmt(t["ci95"][1], 7), _fmt(t["sd"], 3),
                _fmt(t["cv"], 3) if t["cv_informative"] else "não informativo",
                _fmt(t.get("factory"), 7), _fmt(t.get("factory_in_ci95")), _fmt(t.get("factory_percentile"), 3)))
        L += ["", "EME empírico (%s): esquerda %.3f px, direita %.3f px%s." % (
            d["eme_region"], d["eme"]["left"]["eme_px"], d["eme"]["right"]["eme_px"],
            "; F a %.3f / %.3f px da referência" % (d["eme"]["left"]["factory_px"], d["eme"]["right"]["factory_px"])
            if "factory_px" in d["eme"]["left"] else ""), ""]
    L += ["A posição de F é informativa (brief R5): não é critério de aceitação.", "", "![parâmetros](parameters.png)", ""]
    if comp is None:
        return "\n".join(L)
    a, b = list(conds)
    L += ["## Comparação %s × %s" % (a, b), "", "### Permutação dos erros de mapeamento (R3)", "",
          "| Câmera | A–B médio | A–A médio | B–B médio | Estatística | p | Simétrica | p |", "|---|---:|---:|---:|---:|---:|---:|---:|"]
    for s, r in comp["permutation"].items():
        L.append("| %s | %.3f | %.3f | %.3f | %.3f | %.2g | %.3f | %.2g |" % (
            s, r["mean_between_px"], r["mean_within_a_px"], r["mean_within_b_px"], r["main"]["statistic"],
            r["main"]["p_value"], r["symmetric"]["statistic"], r["symmetric"]["p_value"]))
    L += ["", "![permutação](permutation.png)", ""]
    if comp["tost"]:
        L += ["### Equivalência, TOST de Welch (R5)", "",
              "Margens %s." % ("aprovadas" if cfg.get("margins_approved") else "**provisórias, não aprovadas pelo usuário**"), "",
              "| Grandeza | μB − μA | IC90 % | Margem | p | Equivalente |", "|---|---:|---|---:|---:|---|"]
        for k, t in comp["tost"].items():
            L.append("| %s | %s | [%s, %s] | ±%s | %.2g | %s |" % (k, _fmt(t["difference"]), _fmt(t["ci90"][0]),
                                                                 _fmt(t["ci90"][1]), _fmt(t["margin"]), t["p_value"], _fmt(t["equivalent"])))
        L.append("")
    L += ["### Dispersão (R6): B mais disperso que A?", "",
          "| Grandeza | DP A | DP B | DP B / DP A | IC95 % | Brown–Forsythe p | F | p (unilateral) |",
          "|---|---:|---:|---:|---|---:|---:|---:|"]
    for k, d in comp["dispersion"].items():
        L.append("| %s | %s | %s | %.2f | [%.2f, %.2f] | %.2g | %.3g | %.2g |" % (
            k, _fmt(d["sd_a"]), _fmt(d["sd_b"]), d["sd_ratio"], d["sd_ratio_ci95"][0], d["sd_ratio_ci95"][1],
            d["brown_forsythe"]["p_value"], d["f_ratio"]["statistic"], d["f_ratio"]["p_value"]))
    L += ["", "Com n = 10, um p alto não mostra dispersões iguais: o IC da razão dos DPs diz quanto "
          "a mais de dispersão em B os dados ainda permitem."]
    if "cross_application" in comp:
        L += ["", "### Aplicação cruzada (R4): na outra condição menos na própria", "",
              "| Métrica do M3 | Calibrações de | Diferença média | IC95 % | n |", "|---|---|---:|---|---:|"]
        for m, per in comp["cross_application"].items():
            for c, e in per.items():
                ci = e.get("ci95")
                L.append("| %s | %s | %+.3f px | %s | %d |" % (m, c, e["mean_difference"],
                                                              "[%+.3f, %+.3f]" % tuple(ci) if ci else "—", e["n"]))
    L += ["", "### Valores p (brutos e Holm, informativo — R7)", "", "| Teste | p | p Holm |", "|---|---:|---:|"]
    L += ["| %s | %.3g | %.3g |" % (p["test"], p["p"], p["p_holm"]) for p in comp["p_values"]]
    return "\n".join(L) + "\n"


def _jsonable(o):
    if isinstance(o, np.ndarray):
        return o.tolist()
    if isinstance(o, (np.floating, np.integer)):
        return o.item()
    if isinstance(o, np.bool_):
        return bool(o)
    raise TypeError(type(o))


def run(config_path: str) -> dict:
    cfg = config.load(config_path)
    conds_cfg = config.require(cfg, "conditions", config_path)
    if len(conds_cfg) not in (1, 2):
        raise config.ConfigError("%s: one or two conditions" % config_path)
    out = config.require(cfg, "output_dir", config_path)
    step = float(cfg.get("mapping_step_px", 20))
    factory = read_calibration(cfg["factory"]) if cfg.get("factory") else None
    config.prepare_output(out, config_path)
    conds = {c: _condition(c, e, factory, step) for c, e in conds_cfg.items()}
    comp = _comparison(cfg, *conds.values(), *conds, step) if len(conds) == 2 else None
    factory_q = stats.calibration_quantities(factory) if factory is not None else {}
    version = _code_version()
    _figures(out, conds, comp, factory_q)
    with open(os.path.join(out, "report.md"), "w") as f:
        f.write(_markdown(cfg, conds, comp, factory_q, version))
    report = {"schema_version": 1, "code": version,
              "conditions": {c: {k: v for k, v in d.items() if k not in ("calibrations", "region")} for c, d in conds.items()}}
    if comp is not None:
        report["comparison"] = {k: v for k, v in comp.items()}
        for side in comp["permutation"]:
            for variant in ("main", "symmetric"):
                report["comparison"]["permutation"][side][variant] = {
                    k: v for k, v in comp["permutation"][side][variant].items() if k != "null"}
    with open(os.path.join(out, "report.json"), "w") as f:
        json.dump(report, f, indent=2, default=_jsonable)
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    try:
        run(parser.parse_args(argv).config)
    except (config.ConfigError, FileExistsError, FileNotFoundError) as e:
        print("configuration error: %s" % e, file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
