"""Exploratory (diagnostico-vies-aprilgrid): are the residuals coherent per tag?

With a free pose per view and the capture's own calibration, the residuals of the 4 corners of
each tag are split into their mean (a shift of the whole tag) and the rest. With independent
corner errors the mean holds 1/4 of the residual energy. Much more means errors shared by the
corners of a tag (print placement of the tag, or a per-tag detection error): the per-tag virtual
targets of the Bias Ratio absorb those, so they count as bias. Then, per tag id, the shift is
expressed in the board frame (mm, through the local homography of the view) and compared across
views and across captures: a print error of the target repeats; a detection error does not.

Usage (in the container, from analysis/): python3 piloto/tag_coherence.py
"""
import sys

import cv2
import numpy as np

sys.path.insert(0, ".")
from zedcal import calib, io  # noqa: E402
from zedcal.residuals import left_camera, pose, residuals  # noqa: E402

P = "/workspaces/isaac_ros-dev/dev/calib_runs/piloto"
CAPTURES = {"v2": "2026-10-20_ar_aprilgrid_v2", "acrilico": "2026-10-23_acrilico_ar_aprilgrid",
            "controles": "2026-11-03_camera_settings"}


def analyse(det, cam):
    share_num = share_den = 0.0
    per_tag = {}  # id -> list of board-frame shifts (mm)
    for f in det.frames:
        obj, img, ids = io.mono_correspondences(f, "left", det.target)
        if len(obj) < 48:
            continue
        rv, tv = pose(obj, img, cam)
        res = residuals(obj, img, rv, tv, cam)     # projected - detected
        proj = img + res
        # image -> board plane, locally: homography from the projected (model) points
        Hm, _ = cv2.findHomography(proj.astype(np.float32), obj[:, :2].astype(np.float32))
        for k, tid in enumerate(ids):
            r = res[4 * k:4 * k + 4]
            m = r.mean(axis=0)
            share_num += 4 * float(np.sum(m ** 2))
            share_den += float(np.sum(r ** 2))
            # the detected tag sits at the model projection minus the mean residual
            c_img = proj[4 * k:4 * k + 4].mean(axis=0)
            a = cv2.perspectiveTransform(np.array([[c_img]], np.float32), Hm)[0, 0]
            b = cv2.perspectiveTransform(np.array([[c_img - m]], np.float32), Hm)[0, 0]
            per_tag.setdefault(tid, []).append(b - a)
    return share_num / share_den, per_tag


if __name__ == "__main__":
    shifts = {}
    for name, run in CAPTURES.items():
        det = io.load_detections("%s/%s/detections.json" % (P, run))
        c = calib.calibrate(det, calib.CalibrationOptions(mono_points="common", stereo="joint",
                                                          rejection="none")).calibration
        share, per_tag = analyse(det, left_camera(c))
        means = {t: np.mean(v, axis=0) for t, v in per_tag.items() if len(v) >= 5}
        sd_within = np.sqrt(np.mean([np.sum(np.var(v, axis=0, ddof=1)) for v in per_tag.values() if len(v) >= 5]))
        spread_means = np.sqrt(np.mean([np.sum(m ** 2) for m in means.values()]))
        shifts[name] = means
        print("%-10s tag-mean share of residual energy %.2f (independent corners: 0.25) | per-tag shift in the "
              "board: RMS of the means %.3f mm, sd across views %.3f mm (%d tags)" % (
                  name, share, spread_means, sd_within, len(means)))
    names = list(shifts)
    for i in range(len(names)):
        for j in range(i + 1, len(names)):
            common = sorted(set(shifts[names[i]]) & set(shifts[names[j]]))
            a = np.array([shifts[names[i]][t] for t in common]).ravel()
            b = np.array([shifts[names[j]][t] for t in common]).ravel()
            print("per-tag shift, %s vs %s: correlation %+.2f over %d tags" % (names[i], names[j],
                                                                             np.corrcoef(a, b)[0, 1], len(common)))
