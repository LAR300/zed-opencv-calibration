"""python3 -m zedcal.m2_command --config <file>: M2 for the calibrations of a condition.

Config (YAML):
  calibrations: [<calib.yml>, ...]     # the captures of one condition
  factory: <calib_factory.yml>          # optional: F
  detections: [<detections.json>, ...]  # optional: corners that define the covered region
  output_dir: <folder; never over existing content>
  step_px: 20
Writes m2.json, calib_reference.yml (the image-space mean), calib_parameter_mean.yml, the
configuration and heatmaps. Every mapping error is given over the whole image and, when
detections are given, over the region covered by data; with and without the rotation.
"""
import argparse
import json
import os
import sys

import numpy as np

from . import config, io, m2
from .calib_file import read_calibration, write_calibration
from .residuals import left_camera, right_camera


def _heatmap(err: m2.MappingError, size, title, path):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    g = err.grid
    xs, ys = np.unique(g[:, 0]), np.unique(g[:, 1])
    img = err.errors_px.reshape(len(ys), len(xs))
    fig, ax = plt.subplots(figsize=(8, 4.8))
    im = ax.imshow(img, extent=(xs[0], xs[-1], ys[-1], ys[0]), cmap="magma")
    fig.colorbar(im, ax=ax, label="mapping error [px]")
    ax.set_title(title)
    fig.tight_layout()
    fig.savefig(path, dpi=100)
    plt.close(fig)


def run(config_path: str) -> dict:
    cfg = config.load(config_path)
    paths = config.require(cfg, "calibrations", config_path)
    calibs = [read_calibration(p) for p in paths]
    names = [os.path.splitext(os.path.basename(os.path.dirname(p)) or p)[0] for p in paths]
    factory = read_calibration(cfg["factory"]) if cfg.get("factory") else None
    step = float(cfg.get("step_px", 20.0))
    out = config.require(cfg, "output_dir", config_path)
    config.prepare_output(out, config_path)
    size = calibs[0].image_size
    grid = m2.pixel_grid(size, step)

    ref, simple, fit = m2.reference_calibration(calibs, step)
    write_calibration(os.path.join(out, "calib_reference.yml"), ref)
    write_calibration(os.path.join(out, "calib_parameter_mean.yml"), simple)

    regions = {"full_image": None}
    if cfg.get("detections"):
        corners = {"left": [], "right": []}
        for d in cfg["detections"]:
            det = io.load_detections(d)
            for f in det.frames:
                for side in ("left", "right"):
                    tags = getattr(f, side)
                    if tags:
                        corners[side].append(np.concatenate(list(tags.values())))
        regions["covered"] = {s: m2.coverage_mask(grid, np.concatenate(c)) for s, c in corners.items()}

    report = {"schema_version": 1, "calibrations": [os.path.abspath(p) for p in paths], "names": names,
              "step_px": step, "reference_fit": fit, "error_convention": "rms_per_point_px", "sides": {}}
    for side, pick in (("left", left_camera), ("right", right_camera)):
        entry = {}
        for region_name, region in regions.items():
            mask = None if region is None else region[side]
            for rotation in (False, True):
                key = "%s%s" % (region_name, "_rotation" if rotation else "")
                e = {"eme": m2.empirical_eme(calibs, ref, side, step, rotation, mask),
                     "pairs_px": m2.pair_matrix(calibs, side, step, rotation, mask).tolist(),
                     "parameter_mean_vs_reference":
                         m2.mapping_error(pick(simple), pick(ref), size, step, rotation, region=mask).summary()}
                if factory is not None:
                    f_err = m2.mapping_error(pick(factory), pick(ref), size, step, rotation, region=mask)
                    e["factory_vs_reference"] = f_err.summary()
                    e["factory_percentile"] = m2.percentile_of(f_err.rms_per_point_px, e["eme"]["errors_px"])
                    if not rotation:
                        _heatmap(f_err, size, "F -> reference, %s camera, %s" % (side, region_name),
                                 os.path.join(out, "m2_factory_%s_%s.png" % (side, region_name)))
                entry[key] = e
        report["sides"][side] = entry
    with open(os.path.join(out, "m2.json"), "w") as f:
        json.dump(report, f, indent=2)
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    r = run(parser.parse_args(argv).config)
    for side, entry in r["sides"].items():
        for key, e in entry.items():
            line = "%-5s %-18s EME %.3f px  (each: %s)" % (side, key, e["eme"]["eme_px"],
                                                       ", ".join("%.3f" % x for x in e["eme"]["errors_px"]))
            if "factory_vs_reference" in e:
                line += "  F %.3f px (p%.0f)" % (e["factory_vs_reference"]["rms_per_point_px"], e["factory_percentile"])
            print(line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
