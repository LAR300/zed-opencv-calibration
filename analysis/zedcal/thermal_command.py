"""python3 -m zedcal.thermal_command --config <file>: warm-up report (spec aquecimento-termico, R5).

Config (YAML):
  records:                          # cold starts, on different days (R4: at least 3)
    - {name: dia1, detections: <termico/captura_01/detections.json>}   # meta.json next to it
  calibration: <calib_factory_rational8.yml>   # fixed: F
  window_min: 5
  factor: 2
  output_dir: <folder; never over existing content>
"""
import argparse
import json
import os
import sys

import numpy as np

from . import config, io, thermal
from .calib_file import read_calibration

PLOTTED = ("tz_mm", "tx_mm", "rx_deg", "transfer_dx_px", "transfer_dy_px", "transfer_rms_px")


def _figures(results, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    for axis in ("time", "temperature"):
        fig, axes = plt.subplots(2, 3, figsize=(14, 7))
        any_line = False
        for name, r in results.items():
            s = r["series_data"]
            if axis == "time":
                x = s["time_min"]
            else:
                if not s["has_temperature"]:
                    continue
                x = [row["temperature_c"]["onboard_left"] for row in s["rows"]]
            any_line = True
            for ax, q in zip(axes.ravel(), PLOTTED):
                ax.plot(x, [row[q] for row in s["rows"]], ".-", ms=3, label=name)
                st = r["analysis"]["series"][q]["steady_min"]
                if axis == "time" and st is not None:
                    ax.axvline(st, color=ax.lines[-1].get_color(), linestyle="--", lw=0.8)
        for ax, q in zip(axes.ravel(), PLOTTED):
            ax.set_title(q)
            ax.set_xlabel("min since the first pair" if axis == "time" else "ONBOARD_LEFT [°C]")
        axes[0, 0].legend()
        fig.tight_layout()
        if any_line:
            fig.savefig(os.path.join(out, "thermal_vs_%s.png" % axis), dpi=100)
        plt.close(fig)


def run(config_path: str) -> dict:
    cfg = config.load(config_path)
    records = config.require(cfg, "records", config_path)
    calib = read_calibration(config.require(cfg, "calibration", config_path))
    out = config.require(cfg, "output_dir", config_path)
    window, factor = float(cfg.get("window_min", 5.0)), float(cfg.get("factor", 2.0))
    config.prepare_output(out, config_path)
    results = {}
    for rec in records:
        det = io.load_detections(rec["detections"])
        meta_path = rec.get("meta") or os.path.join(os.path.dirname(rec["detections"]), "meta.json")
        meta = io.load_meta(meta_path) if os.path.exists(meta_path) else None
        s = thermal.series(calib, det, meta)
        results[rec["name"]] = {"series_data": s, "analysis": thermal.analyse(s, window, factor)}
    x = thermal.proposed_x([r["analysis"]["steady_min"] for r in results.values()])
    report = {"schema_version": 1, "window_min": window, "factor": factor, "proposed_x_min": x,
              "records": {n: {"analysis": r["analysis"], "pairs": len(r["series_data"]["rows"]),
                              "duration_min": r["series_data"]["time_min"][-1],
                              "rows": r["series_data"]["rows"], "time_min": r["series_data"]["time_min"]}
                          for n, r in results.items()}}
    if len(records) < 3:
        report["warning"] = "fewer than 3 cold starts (R4): X is provisional"
    with open(os.path.join(out, "thermal.json"), "w") as f:
        json.dump(report, f, indent=2)
    _figures(results, out)
    L = ["# Aquecimento até o regime térmico", "",
         "Janela %.1f min, fator %.1f. X proposto: **%s**%s." % (
             window, factor, "%d min" % x if x is not None else "nenhum (algum registro não chegou ao regime)",
             " (menos de 3 partidas a frio: provisório)" if len(records) < 3 else ""), "",
         "| Registro | Pares | Duração | Regime | Temperatura no regime (E/D/IMU) | Séries sem regime |",
         "|---|---:|---:|---:|---|---|"]
    for n, r in report["records"].items():
        a = r["analysis"]
        temp = a.get("temperature_at_steady_c")
        L.append("| %s | %d | %.1f min | %s | %s | %s |" % (
            n, r["pairs"], r["duration_min"], "%.1f min" % a["steady_min"] if a["steady_min"] is not None else "—",
            "/".join("%.1f" % temp[k] if temp and temp.get(k) is not None else "—" for k in ("onboard_left", "onboard_right", "imu")),
            ", ".join(a["not_settled"]) or "—"))
        if not a["has_temperature"]:
            L.append("")
            L.append("Aviso: %s não tem temperatura; a análise é só em função do tempo." % n)
    L += ["", "| Série | " + " | ".join("%s: regime / variação total / σ" % n for n in results) + " |",
          "|---|" + "---|" * len(results)]
    for q in thermal.SERIES:
        cells = []
        for n, r in results.items():
            e = r["analysis"]["series"][q]
            cells.append("%s / %+.4g / %.3g" % ("%.1f" % e["steady_min"] if e["steady_min"] is not None else "—",
                                                  e["total_change"], e["sigma"]))
        L.append("| %s | %s |" % (q, " | ".join(cells)))
    L += ["", "![tempo](thermal_vs_time.png)", "", "![temperatura](thermal_vs_temperature.png)", ""]
    with open(os.path.join(out, "report.md"), "w") as f:
        f.write("\n".join(L))
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    try:
        r = run(parser.parse_args(argv).config)
    except (config.ConfigError, FileExistsError, FileNotFoundError) as e:
        print("configuration error: %s" % e, file=sys.stderr)
        return 2
    for n, rec in r["records"].items():
        print("%s: steady at %s min" % (n, rec["analysis"]["steady_min"]))
    print("proposed X: %s min%s" % (r["proposed_x_min"], " (provisional)" if "warning" in r else ""))
    return 0


if __name__ == "__main__":
    sys.exit(main())
