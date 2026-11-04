"""Exploratory (diagnostico-vies-aprilgrid): is the printed target stretched in one direction?

A target printed with different scales along X and Y makes a calibration that assumes a square
grid put the difference into fy/fx (when the views keep the target upright). With the intrinsics
of F fixed (independent of these images), each view gets a free pose with the target model
stretched along its Y axis by (1 + a); the a that minimises the residual is the print aspect error
relative to what F sees. In-plane rotation of the views is reported too: with the target always
upright the aspect error and fy/fx cannot be told apart by the calibration.

Usage (in the container, from analysis/): python3 piloto/target_aspect.py <out.json>
"""
import glob
import json
import sys

import cv2
import numpy as np

sys.path.insert(0, ".")
from zedcal import io  # noqa: E402
from zedcal.calib_file import read_calibration  # noqa: E402
from zedcal.residuals import left_camera, pose, residuals, right_camera  # noqa: E402

P = "/workspaces/isaac_ros-dev/dev/calib_runs/piloto"
CAPTURES = {"xadrez": "2026-10-16_ar_9x6", "v2": "2026-10-20_ar_aprilgrid_v2",
            "acrilico": "2026-10-23_acrilico_ar_aprilgrid", "controles": "2026-11-03_camera_settings"}
ASPECTS = np.linspace(-0.008, 0.008, 33)


def total_mse(det, cam, side, a):
    sq, n, rots = 0.0, 0, []
    for f in det.frames:
        try:
            obj, img, _ = io.mono_correspondences(f, side, det.target)
        except ValueError:
            continue
        if len(obj) < 24:
            continue
        obj = obj.copy()
        obj[:, 1] *= 1.0 + a
        rv, tv = pose(obj, img, cam)
        res = residuals(obj, img, rv, tv, cam)
        sq += float(np.sum(res ** 2))
        n += len(res)
        R = cv2.Rodrigues(np.asarray(rv, float).reshape(3, 1))[0]
        x_img = R[:2, 0]                     # target X axis, roughly as seen in the image
        rots.append(float(np.degrees(np.arctan2(x_img[1], x_img[0]))))
    return sq / n, rots


if __name__ == "__main__":
    F = read_calibration(glob.glob(P + "/fabrica/SN*/calib_factory_rational8.yml")[0])
    out = {}
    for name, run in CAPTURES.items():
        det = io.load_detections("%s/%s/detections.json" % (P, run))
        out[name] = {}
        for side, cam in (("left", left_camera(F)), ("right", right_camera(F))):
            curve = [total_mse(det, cam, side, a)[0] for a in ASPECTS]
            k = int(np.argmin(curve))
            # parabola through the three points around the minimum
            if 0 < k < len(ASPECTS) - 1:
                y0, y1, y2 = curve[k - 1:k + 2]
                h = ASPECTS[1] - ASPECTS[0]
                best = ASPECTS[k] + h * (y0 - y2) / (2 * (y0 - 2 * y1 + y2))
            else:
                best = ASPECTS[k]
            mse0 = total_mse(det, cam, side, 0.0)
            out[name][side] = {"best_aspect_pct": 100 * best, "rms_at_0": float(np.sqrt(2 * mse0[0] / 2)),
                               "rms_at_best": float(np.sqrt(min(curve))),
                               "rotation_deg_p10_p90": [float(np.percentile(mse0[1], 10)), float(np.percentile(mse0[1], 90))]}
            o = out[name][side]
            print("%-10s %-5s best Y stretch %+.3f %% | RMS %.3f -> %.3f px | in-plane rotation p10..p90 %.0f..%.0f deg" % (
                name, side, o["best_aspect_pct"], np.sqrt(mse0[0]), o["rms_at_best"], *o["rotation_deg_p10_p90"]))
    if len(sys.argv) > 1:
        json.dump(out, open(sys.argv[1], "w"), indent=1)
