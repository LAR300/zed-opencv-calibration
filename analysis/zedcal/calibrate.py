"""python3 -m zedcal.calibrate --config <file>: calibrate a capture with one or more variants.

Config (YAML):
  detections: <capture>/detections.json
  output_dir: <folder, created; never over existing content>
  model: rational8                  # rational8 | radtan5
  variants:                         # each: mono_points, stereo, rejection (defaults: all, joint, cpp)
    - {mono_points: common, stereo: joint, rejection: cpp}   # the C++ calibration
    - {mono_points: all, stereo: fixed, rejection: none}
  prior: <calib.yml, optional>      # initial guess, e.g. the exported factory calibration
  bootstrap: {n: 0, seed: 0}        # n > 0: bootstrap std of the parameters, per variant
"""
import argparse
import json
import os
import sys

from . import bootstrap as boot
from . import calib, config, io
from .calib_file import read_calibration


def run(config_path: str) -> int:
    cfg = config.load(config_path)
    det = io.load_detections(config.require(cfg, "detections", config_path))
    out = config.require(cfg, "output_dir", config_path)
    model = cfg.get("model", "rational8")
    prior = read_calibration(cfg["prior"]) if cfg.get("prior") else None
    b = cfg.get("bootstrap") or {}
    n_boot, seed = int(b.get("n", 0)), int(b.get("seed", 0))
    config.prepare_output(out, config_path)

    for v in cfg.get("variants") or [{}]:
        opts = calib.CalibrationOptions(model=model, mono_points=v.get("mono_points", "all"),
                                        stereo=v.get("stereo", "joint"),
                                        rejection=v.get("rejection", "cpp"), prior=prior)
        result = calib.calibrate(det, opts)
        if n_boot > 0:
            b_res = boot.bootstrap(det, opts, n=n_boot, seed=seed)
            result.report["bootstrap"] = {"n": n_boot, "seed": seed,
                                          "std": {k: float(x) for k, x in b_res.std.items()}}
        result.report["config"] = os.path.abspath(config_path)
        yml, rep = calib.save(result, out)
        r = result.report["rms_px"]
        print("%-28s stereo %.4f px, fit L/R %.4f / %.4f px, %d of %d views -> %s"
              % (opts.variant(), r["stereo"], r["stereo_fit_left"] or float("nan"),
                 r["stereo_fit_right"] or float("nan"), result.report["pairs_used"],
                 result.report["pairs_input"], os.path.basename(yml)))
    return 0


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    args = parser.parse_args(argv)
    return run(args.config)


if __name__ == "__main__":
    sys.exit(main())
