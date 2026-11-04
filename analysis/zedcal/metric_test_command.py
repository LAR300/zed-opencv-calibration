"""python3 -m zedcal.metric_test_command --config <file>: report of a metric test session
(spec validacao-metrica, R5 and R6).

Config (YAML):
  detections: <metrico_A/captura_01/detections.json>
  meta: <meta.json>                 # optional; default next to the detections
  calibrations: {F: <calib.yml>, c01: <calib.yml>, reference: <calib.yml>}
  target_measured: {tag_size_mm: 88.0, tag_spacing_mm: 26.4}   # optional: else the session's
  measured_distances_mm: {50: 476.0, 100: 975.0}   # optional, per distance label (cm), for
                                                    # frames whose meta.json has no measured_distance
  alpha: 0
  output_dir: <folder; never over existing content>
"""
import argparse
import json
import os
import sys

from . import config, io, metric_test
from .calib_file import read_calibration


def _target(cfg, det, meta, config_path):
    tm = cfg.get("target_measured") or ((meta or {}).get("session") or {}).get("target_measured")
    if not tm or tm.get("tag_size_mm") is None or tm.get("tag_spacing_mm") is None:
        raise config.ConfigError("%s: no measured target (target_measured in the config or the session)"
                                 % config_path)
    sides = tm.get("tag_sides_mm") or []
    gaps = tm.get("spacings_mm") or []
    size = sum(sides) / len(sides) if sides else float(tm["tag_size_mm"])
    space = sum(gaps) / len(gaps) if gaps else float(tm["tag_spacing_mm"])
    return det.target.with_measured(size, space)


def _figure(report, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))
    for name, r in report["calibrations"].items():
        d = [b["measured_mm_mean"] / 1000 for b in r["by_distance"]]
        axes[0].plot(d, [b["relative_error_after_d0_pct"] for b in r["by_distance"]], "o-", label=name)
        axes[1].plot(d, [b["positions"].get("centre", {}).get("span_pct", {}).get("mean") for b in r["by_distance"]],
                     "o-", label=name)
    axes[0].set_title("target distance error after d0 [%]")
    axes[1].set_title("span error, target at the centre [%]")
    for ax in axes:
        ax.set_xlabel("measured distance [m]")
        ax.axhline(0, color="k", lw=0.5)
        ax.legend()
    fig.tight_layout()
    fig.savefig(os.path.join(out, "metric_test.png"), dpi=100)
    plt.close(fig)


def _markdown(report) -> str:
    L = ["# Teste métrico", "", "Alvo: lado %.3f mm, espaço %.3f mm. d0: deslocamento comum entre o ponto de "
         "referência da medida e o centro óptico (medida = Z_plano − d0)." % (report["target"]["tag_size_mm"],
                                                                              report["target"]["tag_spacing_mm"]), ""]
    for name, r in report["calibrations"].items():
        L += ["## %s" % name, "",
              "d0 = %.1f mm; ajuste de dois parâmetros: escala %+.3f %%, d0 %.1f mm." % (
                  r["d0_mm"], r["two_parameter_fit"]["scale_error_pct"], r["two_parameter_fit"]["d0_mm"]), "",
              "| Distância | Vistas | Medida média | Resíduo após d0 | Erro relativo | Epipolar RMS | Vão, centro | Vão, bordas | Passo, centro |",
              "|---|---:|---:|---:|---:|---:|---:|---:|---:|"]
        for b in r["by_distance"]:
            pc, pe = b["positions"].get("centre", {}), b["positions"].get("edges", {})
            f = lambda g, s: "%+.3f %%" % g[s]["mean"] if s in g else "—"
            L.append("| %d cm | %d | %.1f mm | %+.2f mm | %+.3f %% | %.3f px | %s | %s | %s |" % (
                b["label_cm"], b["views"], b["measured_mm_mean"], b["residual_after_d0_mm"],
                b["relative_error_after_d0_pct"], b["epipolar_px"]["rms"], f(pc, "span_pct"), f(pe, "span_pct"),
                f(pc, "pitch_pct")))
        L.append("")
    L += ["![teste métrico](metric_test.png)", ""]
    return "\n".join(L)


def run(config_path: str) -> dict:
    cfg = config.load(config_path)
    det_path = config.require(cfg, "detections", config_path)
    det = io.load_detections(det_path)
    meta_path = cfg.get("meta") or os.path.join(os.path.dirname(det_path), "meta.json")
    meta = io.load_meta(meta_path) if os.path.exists(meta_path) else None
    target = _target(cfg, det, meta, config_path)
    distances = metric_test.measured_distances(det, meta, cfg.get("measured_distances_mm"))
    if all(d is None for d in distances):
        raise config.ConfigError("%s: no measured distance (meta.json frames or measured_distances_mm)" % config_path)
    calibs = {n: read_calibration(p) for n, p in config.require(cfg, "calibrations", config_path).items()}
    out = config.require(cfg, "output_dir", config_path)
    config.prepare_output(out, config_path)
    report = {"schema_version": 1, "target": {"tag_size_mm": target.tag_size_mm,
                                              "tag_spacing_mm": target.tag_size_mm * target.tag_spacing_ratio},
              "calibrations": {n: metric_test.analyse(c, det, distances, target, float(cfg.get("alpha", 0.0)))
                               for n, c in calibs.items()}}
    with open(os.path.join(out, "metric_test.json"), "w") as f:
        json.dump(report, f, indent=2)
    with open(os.path.join(out, "report.md"), "w") as f:
        f.write(_markdown(report))
    _figure(report, out)
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    try:
        r = run(parser.parse_args(argv).config)
    except (config.ConfigError, FileExistsError, FileNotFoundError) as e:
        print("configuration error: %s" % e, file=sys.stderr)
        return 2
    for name, c in r["calibrations"].items():
        print("%-10s d0 %.1f mm, scale %+.3f %%: %s" % (name, c["d0_mm"], c["two_parameter_fit"]["scale_error_pct"], ", ".join(
            "%d cm %+.2f mm" % (b["label_cm"], b["residual_after_d0_mm"]) for b in c["by_distance"])))
    return 0


if __name__ == "__main__":
    sys.exit(main())
