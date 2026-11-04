"""python3 -m zedcal.m3_command --config <file>: M3 for a set of captures.

Config (YAML):
  captures:                       # rows and columns of the leave-one-capture-out matrix
    - {name: c01, detections: <detections.json>, calibration: <calib.yml>}
  extra_calibrations: {F: <calib.yml>, reference: <calib.yml>}   # optional extra rows
  other_captures:                 # optional extra columns (e.g. the other condition, R4)
    - {name: b01, detections: <detections.json>}
  distance_bins_mm: [0, 500, 800, 1200, 2000, 5000]
  output_dir: <folder; never over existing content>
"""
import argparse
import json
import os
import sys

import numpy as np

from . import config, io, m3
from .calib_file import read_calibration


def run(config_path: str) -> dict:
    cfg = config.load(config_path)
    caps = config.require(cfg, "captures", config_path)
    out = config.require(cfg, "output_dir", config_path)
    config.prepare_output(out, config_path)
    dets = {c["name"]: io.load_detections(c["detections"]) for c in caps}
    calibs = {c["name"]: read_calibration(c["calibration"]) for c in caps}
    for name, path in (cfg.get("extra_calibrations") or {}).items():
        calibs[name] = read_calibration(path)
    for c in cfg.get("other_captures") or []:
        dets[c["name"]] = io.load_detections(c["detections"])
    names = [c["name"] for c in caps]
    rows = names + [n for n in calibs if n not in names]
    cols = list(dets)
    res = m3.matrix(calibs, dets, rows=rows, cols=cols)
    bins = cfg.get("distance_bins_mm", [0, 500, 800, 1200, 2000, 5000])
    report = {"schema_version": 1, "rows": rows, "cols": cols, "rms_convention": "per_corner_2d",
              "matrices": {m: v.tolist() for m, v in res["matrices"].items()},
              "loco": m3.loco_summary(res), "own_capture": {}}
    for n in names:
        ev = m3.evaluate(calibs[n], dets[n])
        report["own_capture"][n] = {"per_view": ev.per_view_stats(), "distance": m3.by_distance(ev, bins),
                                    "region_left_rms": m3.by_region(ev, dets[n].image_size).tolist()}
    with open(os.path.join(out, "m3.json"), "w") as f:
        json.dump(report, f, indent=2, default=lambda o: None if isinstance(o, float) and np.isnan(o) else o)
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    r = run(parser.parse_args(argv).config)
    for m in m3.METRICS:
        print("== %s (rows: calibrations, columns: captures)" % m)
        print("%-14s" % "" + "".join("%12s" % c for c in r["cols"]))
        for i, row in enumerate(r["rows"]):
            print("%-14s" % row + "".join("%12.3f" % v for v in r["matrices"][m][i]))
        s = r["loco"][m]
        print("   train %.3f  test %s  ratio %s" % (s["train_mean"], "%.3f" % s["test_mean"] if s["test_mean"] else "-",
                                                   "%.2f" % s["test_over_train"] if s["test_over_train"] else "-"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
