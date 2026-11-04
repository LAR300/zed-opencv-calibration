"""python3 -m zedcal.coverage_command --config <file>: coverage check of a capture (brief R3).

Exit code 0 when the capture passes, 1 when it fails (it must be captured again), 2 on a
configuration error.

Config (YAML):
  detections: <capture>/detections.json
  calibration: <calib.yml>              # for the target poses; F by default
  output_dir: <folder; never over existing content>
  criteria: {min_corners_per_cell: 1, cells: [8, 8], tilt_threshold_deg: 30,
             min_tilted_per_axis: 2, both_signs: true}
"""
import argparse
import json
import os
import sys

import numpy as np

from . import config, coverage, io
from .calib_file import read_calibration
from .residuals import left_camera


def _figures(report, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(2, 2, figsize=(12, 7))
    for k, side in enumerate(("left", "right")):
        d = np.array(report["density"][side])
        im = axes[0, k].imshow(d, cmap="viridis")
        for (r, c), v in np.ndenumerate(d):
            axes[0, k].text(c, r, str(v), ha="center", va="center", color="w", fontsize=7)
        axes[0, k].set_title("corners per cell, %s camera" % side)
        fig.colorbar(im, ax=axes[0, k])
    centres = np.arange(-55, 60, 10)
    for k, axis in enumerate(("horizontal", "vertical")):
        axes[1, k].bar(centres, report["tilt_" + axis]["histogram"], width=9)
        th = report["criteria"]["tilt_threshold_deg"]
        for x in (-th, th):
            axes[1, k].axvline(x, color="r", linestyle="--")
        axes[1, k].set_title("tilt about the %s axis [deg]" % axis)
    fig.suptitle("coverage: %s" % ("PASSED" if report["passed"] else "FAILED"))
    fig.tight_layout()
    fig.savefig(os.path.join(out, "coverage.png"), dpi=100)
    plt.close(fig)


def run(config_path: str) -> dict:
    cfg = config.load(config_path)
    det = io.load_detections(config.require(cfg, "detections", config_path))
    cam = left_camera(read_calibration(config.require(cfg, "calibration", config_path)))
    out = config.require(cfg, "output_dir", config_path)
    crit = cfg.get("criteria") or {}
    criteria = coverage.CoverageCriteria(**{k: (tuple(v) if k == "cells" else v) for k, v in crit.items()})
    config.prepare_output(out, config_path)
    report = coverage.check(det, cam, criteria)
    report["criteria"]["cells"] = list(criteria.cells)
    with open(os.path.join(out, "coverage.json"), "w") as f:
        json.dump(report, f, indent=2)
    _figures(report, out)
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    try:
        r = run(parser.parse_args(argv).config)
    except (config.ConfigError, FileExistsError, FileNotFoundError, TypeError) as e:
        print("configuration error: %s" % e, file=sys.stderr)
        return 2
    print("coverage %s" % ("PASSED" if r["passed"] else "FAILED"))
    for m in r["reasons"]:
        print("  - " + m)
    return 0 if r["passed"] else 1


if __name__ == "__main__":
    sys.exit(main())
