"""python3 -m zedcal.m1_command --config <file>: M1 for a calibration on a capture.

Config (YAML):
  detections: <capture>/detections.json
  calibration: <calib.yml>
  output_dir: <folder; never over existing content>
  points: common          # common (the observations of the C++ calibration) | all
  grid: [8, 8]
  max_rms_px: 0.5
Writes m1_<calibration name>.json, the configuration and the residual map figures.
"""
import argparse
import json
import os
import sys

import numpy as np

from . import config, io, m1
from .calib_file import read_calibration
from .residuals import left_camera, right_camera


def _jsonable(m):
    return {k: (v.tolist() if isinstance(v, np.ndarray) else v) for k, v in m.items()}


def run(config_path: str) -> dict:
    cfg = config.load(config_path)
    det = io.load_detections(config.require(cfg, "detections", config_path))
    calib_path = config.require(cfg, "calibration", config_path)
    c = read_calibration(calib_path)
    out = config.require(cfg, "output_dir", config_path)
    points = cfg.get("points", "common")
    grid = tuple(cfg.get("grid", [8, 8]))
    config.prepare_output(out, config_path)
    name = os.path.splitext(os.path.basename(calib_path))[0]

    report = {"schema_version": 1, "calibration": os.path.abspath(calib_path),
              "detections": os.path.abspath(cfg["detections"]), "points": points}
    report.update(m1.rms_report(det, c, cfg.get("max_rms_px", 0.5), points))
    for side, cam in (("left", left_camera(c)), ("right", right_camera(c))):
        fit = m1.camera_fit(det, cam, side, points)
        report["bias_ratio_" + side] = m1.bias_ratio(fit, cam, det.target,
                                                     m1.N_INTRINSICS[c.model]).as_dict()
        rmap = m1.residual_map(np.concatenate(fit.image_points), fit.all_residuals, det.image_size, grid)
        report["residual_map_" + side] = _jsonable(rmap)
        m1.plot_residual_map(rmap, "%s, %s camera" % (name, side),
                             os.path.join(out, "m1_%s_%s.png" % (name, side)))
    with open(os.path.join(out, "m1_%s.json" % name), "w") as f:
        json.dump(report, f, indent=2)
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    r = run(parser.parse_args(argv).config)
    for side in ("left", "right"):
        b = r["bias_ratio_" + side]
        print("%-5s BR %.3f  eps_bias %.3f px  sigma_d %.3f px  RMS free pose %.3f px"
              % (side, b["br"], b["eps_bias_px"], b["sigma_d_px"], r["rms_px"]["free_pose_" + side]))
    print("stereo RMS %.4f px, filter %s" % (r["rms_px"]["stereo"], "passed" if r["filter"]["passed"] else "FAILED"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
