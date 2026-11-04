"""Exploratory (diagnostico-vies-aprilgrid): is the AprilGrid bias motion during the readout?

The ZED 2i sensors read row by row. A target moving during the readout is displaced in proportion
to the row: with a free pose per view and the intrinsics fixed, the residuals keep a part linear in
the row, d = v * (y - y_mean), that no rigid pose absorbs. Signatures of motion, not of the
detector or of the target: the per-view shear changes sign between views (the hand moves either
way) and is the same in the left and right images of a pair (the two sensors are synchronised).

Usage (in the container, from analysis/): python3 piloto/rolling_shutter.py <out.json>
"""
import json
import sys

import numpy as np

sys.path.insert(0, ".")
from zedcal import io  # noqa: E402
from zedcal.calib_file import read_calibration  # noqa: E402
from zedcal.residuals import left_camera, pose, residuals, right_camera  # noqa: E402

P = "/workspaces/isaac_ros-dev/dev/calib_runs/piloto"
CAPTURES = {
    "xadrez": ("2026-10-16_ar_9x6", "xadrez"),
    "v2": ("2026-10-20_ar_aprilgrid_v2", "v2"),
    "acrilico": ("2026-10-23_acrilico_ar_aprilgrid", "acrilico"),
    "controles": ("2026-11-03_camera_settings", "controles"),
}


def shear_fit(img, res):
    """Least squares res = a + s * (y - mean y), per coordinate. Returns s (px per 100 rows) for
    x and y, and the fraction of the residual MSE the fit explains."""
    y = img[:, 1] - img[:, 1].mean()
    A = np.c_[np.ones_like(y), y]
    out, explained = [], 0.0
    total = float(np.sum(res ** 2))
    for k in range(2):
        coef, *_ = np.linalg.lstsq(A, res[:, k], rcond=None)
        out.append(100.0 * coef[1])
        explained += float(np.sum((A @ coef) ** 2))
    return out, explained / total if total > 0 else 0.0


def analyse(name):
    run, calib_dir = CAPTURES[name]
    det = io.load_detections("%s/%s/detections.json" % (P, run))
    calib = read_calibration("%s/resultados/%s/calib_compat.yml" % (P, calib_dir))
    rows = []
    for f in det.frames:
        per = {}
        try:
            for side, cam in (("left", left_camera(calib)), ("right", right_camera(calib))):
                obj, img, _ = io.mono_correspondences(f, side, det.target)
                if len(obj) < 24:
                    raise ValueError
                rv, tv = pose(obj, img, cam)
                res = residuals(obj, img, rv, tv, cam)
                (sx, sy), frac = shear_fit(img, res)
                per[side] = {"sx": sx, "sy": sy, "explained": frac,
                             "rms": float(np.sqrt(np.mean(np.sum(res ** 2, axis=1)))),
                             "rows": float(np.ptp(img[:, 1]))}
        except ValueError:
            continue
        rows.append(per)
    sxl = np.array([r["left"]["sx"] for r in rows])
    sxr = np.array([r["right"]["sx"] for r in rows])
    syl = np.array([r["left"]["sy"] for r in rows])
    syr = np.array([r["right"]["sy"] for r in rows])
    expl = np.array([r[s]["explained"] for r in rows for s in ("left", "right")])
    return {"views": len(rows),
            "corr_left_right_sx": float(np.corrcoef(sxl, sxr)[0, 1]),
            "corr_left_right_sy": float(np.corrcoef(syl, syr)[0, 1]),
            "sx_left_mean": float(sxl.mean()), "sx_left_sd": float(sxl.std(ddof=1)),
            "sy_left_mean": float(syl.mean()), "sy_left_sd": float(syl.std(ddof=1)),
            "explained_median": float(np.median(expl)), "explained_p90": float(np.percentile(expl, 90)),
            "per_view": rows}


if __name__ == "__main__":
    out = {n: analyse(n) for n in CAPTURES}
    for n, r in out.items():
        print("%-10s views %2d | corr L-R shear x %+.2f y %+.2f | shear x %+.3f±%.3f y %+.3f±%.3f px/100 rows"
              " | explained median %.2f p90 %.2f" % (
                  n, r["views"], r["corr_left_right_sx"], r["corr_left_right_sy"], r["sx_left_mean"],
                  r["sx_left_sd"], r["sy_left_mean"], r["sy_left_sd"], r["explained_median"], r["explained_p90"]))
    if len(sys.argv) > 1:
        json.dump(out, open(sys.argv[1], "w"), indent=1)
