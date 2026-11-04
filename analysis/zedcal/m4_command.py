"""python3 -m zedcal.m4_command --config <file>: M4 task metrics (spec metricas-tarefa).

Config (YAML):
  captures:                        # the validation captures
    - {name: v01, detections: <detections.json>}
  calibrations: {F: <calib.yml>, c01: <calib.yml>, reference: <calib.yml>}
  reference_calibration: F         # required: disparity bias of every other one relative to it
  target_measured: {tag_size_mm: 88.0, tag_spacing_mm: 26.4}
                                   # optional: else the session's, from meta.json next to the
                                   # detections; a capture with neither is a configuration error
  alpha: 0                         # stereoRectify
  distance_bins_mm: [0, 500, 800, 1200, 2000, 5000]
  grid: [8, 8]
  output_dir: <folder; never over existing content>
"""
import argparse
import json
import os
import sys

import numpy as np

from . import config, io, m4
from .calib_file import read_calibration


def _measured_target(cap, cfg, det, config_path):
    tm = cap.get("target_measured") or cfg.get("target_measured")
    source = "config"
    if tm is None:
        meta = os.path.join(os.path.dirname(cap["detections"]), "meta.json")
        if os.path.exists(meta):
            tm = (io.load_meta(meta).get("session") or {}).get("target_measured")
            source = "meta.json"
    if det.target.type != "aprilgrid":
        return det.target, "nominal (checkerboard)"
    if not tm or tm.get("tag_size_mm") is None or tm.get("tag_spacing_mm") is None:
        raise config.ConfigError("%s: no measured target for capture %s (target_measured in the "
                                 "config or in the session's meta.json)" % (config_path, cap["name"]))
    return det.target.with_measured(float(tm["tag_size_mm"]), float(tm["tag_spacing_mm"])), source


def _json(o):
    if isinstance(o, np.ndarray):
        return [[None if np.isnan(v) else float(v) for v in row] for row in o]
    if isinstance(o, float) and np.isnan(o):
        return None
    raise TypeError(type(o))


def _figure(name, s, out):
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(1, 2, figsize=(12, 3.8))
    for ax, data, title in ((axes[0], s["epipolar_px"]["region_rms_px"], "epipolar RMS [px]"),
                            (axes[1], s["metric_pct"]["pitch"].get("region_rms_pct"), "pitch error RMS [%]")):
        if data is None:
            ax.set_axis_off()
            continue
        im = ax.imshow(np.asarray(data, float), cmap="viridis")
        ax.set_title("%s, %s" % (title, name))
        fig.colorbar(im, ax=ax)
    fig.tight_layout()
    fig.savefig(os.path.join(out, "m4_%s.png" % name), dpi=100)
    plt.close(fig)


def run(config_path: str) -> dict:
    cfg = config.load(config_path)
    caps = config.require(cfg, "captures", config_path)
    calib_paths = config.require(cfg, "calibrations", config_path)
    out = config.require(cfg, "output_dir", config_path)
    ref = config.require(cfg, "reference_calibration", config_path)
    if ref not in calib_paths:
        raise config.ConfigError("%s: reference_calibration %s is not in calibrations" % (config_path, ref))
    alpha = float(cfg.get("alpha", 0.0))
    bins = cfg.get("distance_bins_mm", [0, 500, 800, 1200, 2000, 5000])
    grid = tuple(cfg.get("grid", [8, 8]))
    dets, targets = {}, {}
    for c in caps:
        dets[c["name"]] = io.load_detections(c["detections"])
        targets[c["name"]] = _measured_target(c, cfg, dets[c["name"]], config_path)
    calibs = {n: read_calibration(p) for n, p in calib_paths.items()}
    config.prepare_output(out, config_path)

    report = {"schema_version": 1, "alpha": alpha, "reference_calibration": ref,
              "targets": {n: {"source": src, "tag_size_mm": t.tag_size_mm,
                              "tag_spacing_mm": t.tag_size_mm * t.tag_spacing_ratio}
                          for n, (t, src) in targets.items()},
              "per_capture": {}, "pooled": {}, "disparity_bias": {}}
    evs = {}
    for cn, cal in calibs.items():
        report["per_capture"][cn] = {}
        for c in caps:
            ev = m4.evaluate(cal, dets[c["name"]], target=targets[c["name"]][0], alpha=alpha)
            evs[(cn, c["name"])] = ev
            report["per_capture"][cn][c["name"]] = m4.summary(ev, dets[c["name"]].image_size, bins, grid)
        pooled = m4.summary(m4.merge([evs[(cn, c["name"])] for c in caps]),
                            dets[caps[0]["name"]].image_size, bins, grid)
        report["pooled"][cn] = pooled
        _figure(cn, pooled, out)
    for cn in calibs:
        if cn == ref:
            continue
        report["disparity_bias"][cn] = {c["name"]: m4.disparity_bias(evs[(ref, c["name"])], evs[(cn, c["name"])])
                                        for c in caps}
    with open(os.path.join(out, "m4.json"), "w") as f:
        json.dump(report, f, indent=2, default=_json)
    return report


def main(argv=None) -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--config", required=True)
    try:
        r = run(parser.parse_args(argv).config)
    except (config.ConfigError, FileExistsError, FileNotFoundError) as e:
        print("configuration error: %s" % e, file=sys.stderr)
        return 2
    print("%-12s %8s %8s %8s %9s %9s %9s %9s" % ("calibration", "epi_rms", "<0.25", "<0.1", "sides%",
                                                "pitch%", "span%", "plane_mm"))
    for cn, s in r["pooled"].items():
        e, m = s["epipolar_px"], s["metric_pct"]
        print("%-12s %8.3f %8.2f %8.2f %+9.3f %+9.3f %+9.3f %9.3f" % (
            cn, e["rms"], e["fraction_below_0.25_px"], e["fraction_below_0.1_px"],
            m["tag_sides"].get("mean", float("nan")), m["pitch"].get("mean", float("nan")),
            m["span"].get("mean", float("nan")), s["plane"]["rms_mm_median"]))
    for cn, per in r["disparity_bias"].items():
        print("disparity bias of %s vs %s: %s" % (cn, r["reference_calibration"], ", ".join(
            "%s %+.3f px (dZ/Z at 3 m %+.2f %%)" % (c, b["delta_d_px"]["median"], b["delta_z_over_z_pct"]["3000"])
            for c, b in per.items())))
    return 0


if __name__ == "__main__":
    sys.exit(main())
