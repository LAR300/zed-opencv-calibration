"""Exploratory (diagnostico-vies-aprilgrid): is the AprilGrid bias in the corner refinement?

detections.json keeps every corner before and after the X-junction refinement. The same capture
is calibrated with each set and the systematic error compared: eps_bias (px), not only BR, since
noisier corners lower BR without any less bias. Variant 'compat', rejection 'none' (the decided
offline settings), so both sets keep the same views.

Usage (in the container, from analysis/): python3 piloto/refinement.py <out.json>
"""
import json
import sys
from dataclasses import replace

import numpy as np

sys.path.insert(0, ".")
from zedcal import calib, io, m1  # noqa: E402
from zedcal.residuals import left_camera, right_camera  # noqa: E402

P = "/workspaces/isaac_ros-dev/dev/calib_runs/piloto"
CAPTURES = {"v2": "2026-10-20_ar_aprilgrid_v2", "acrilico": "2026-10-23_acrilico_ar_aprilgrid",
            "controles": "2026-11-03_camera_settings"}


def unrefined(det):
    frames = [replace(f, left=dict(f.left_unrefined), right=dict(f.right_unrefined)) for f in det.frames]
    return replace(det, frames=frames)


def summary(det):
    r = calib.calibrate(det, calib.CalibrationOptions(mono_points="common", stereo="joint", rejection="none"))
    c = r.calibration
    out = {"rms": r.report["rms_px"], "fy_fx_left": c.K_left[1, 1] / c.K_left[0, 0],
           "fx_left": c.K_left[0, 0], "baseline_mm": c.baseline_mm}
    for side, cam in (("left", left_camera(c)), ("right", right_camera(c))):
        fit = m1.camera_fit(det, cam, side, "common")
        b = m1.bias_ratio(fit, cam, det.target, m1.N_INTRINSICS[c.model]).as_dict()
        out[side] = {k: b[k] for k in ("br", "eps_bias_px", "sigma_d_px")}
    return out


if __name__ == "__main__":
    res = {}
    for name, run in CAPTURES.items():
        det = io.load_detections("%s/%s/detections.json" % (P, run))
        res[name] = {"refined": summary(det), "unrefined": summary(unrefined(det))}
        for k in ("refined", "unrefined"):
            s = res[name][k]
            print("%-10s %-9s fy/fx %.5f fx %.2f B %.2f | L BR %.2f eps %.3f sd %.3f | R BR %.2f eps %.3f sd %.3f" % (
                name, k, s["fy_fx_left"], s["fx_left"], s["baseline_mm"], s["left"]["br"], s["left"]["eps_bias_px"],
                s["left"]["sigma_d_px"], s["right"]["br"], s["right"]["eps_bias_px"], s["right"]["sigma_d_px"]))
    if len(sys.argv) > 1:
        json.dump(res, open(sys.argv[1], "w"), indent=1)
