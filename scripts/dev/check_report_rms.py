"""Bench check (saida-calibracao, plan step 6): recompute the per-camera RMS of a
session calibration report from its YAML and detections.json, independently of the C++.

- free pose: one solvePnP per view and camera with the final intrinsics
- stereo fit: one 6-DoF pose per view, fitted to both cameras through R, T (the poses the
  stereo optimisation ends with, with the intrinsics and R, T fixed)
Only the tags seen by both cameras are used, as in the calibration.

Usage: python3 -I check_report_rms.py <capture_dir> [model]
"""
import json
import sys

import cv2
import numpy as np
from scipy.optimize import least_squares

capture = sys.argv[1]
model = sys.argv[2] if len(sys.argv) > 2 else "rational8"
fs = cv2.FileStorage("%s/calib_cpp_%s.yml" % (capture, model), cv2.FILE_STORAGE_READ)
KL, DL, KR, DR = (fs.getNode(k).mat() for k in ("K_LEFT", "D_LEFT", "K_RIGHT", "D_RIGHT"))
R, _ = cv2.Rodrigues(fs.getNode("R").mat())
T = fs.getNode("T").mat().reshape(3)
report = json.load(open("%s/calib_cpp_%s.report.json" % (capture, model)))
det = json.load(open("%s/detections.json" % capture))
tgt = det["target"]
tag, pitch = tgt["tag_size_mm"], tgt["tag_size_mm"] * (1 + tgt["tag_spacing_ratio"])


def corners3d(tag_id):
    x0, y0 = (tag_id % tgt["cols"]) * pitch, (tag_id // tgt["cols"]) * pitch
    return [[x0 + tag, y0, 0], [x0, y0, 0], [x0, y0 + tag, 0], [x0 + tag, y0 + tag, 0]]


removed = {(v["pose_id"], v["n"]) for v in report["views"] if v["removed"]}
free_l, free_r, fit_l, fit_r, corners = [], [], [], [], []
for frame in det["frames"]:
    if (frame["pose_id"], frame["n"]) in removed:
        continue
    left = {t["id"]: t["corners"] for t in frame["left"]["tags"]}
    right = {t["id"]: t["corners"] for t in frame["right"]["tags"]}
    ids = sorted(set(left) & set(right))
    if len(ids) < 12:
        continue
    obj = np.array([p for i in ids for p in corners3d(i)], np.float64)
    pl = np.array([p for i in ids for p in left[i]], np.float64)
    pr = np.array([p for i in ids for p in right[i]], np.float64)
    n = len(obj)

    def rms(img, rv, tv, K, D):
        proj, _ = cv2.projectPoints(obj, rv, tv, K, D)
        return np.sqrt(np.mean(np.sum((proj.reshape(-1, 2) - img) ** 2, axis=1)))

    _, rvl, tvl = cv2.solvePnP(obj, pl, KL, DL)
    _, rvr, tvr = cv2.solvePnP(obj, pr, KR, DR)
    free_l.append(rms(pl, rvl, tvl, KL, DL))
    free_r.append(rms(pr, rvr, tvr, KR, DR))

    def residuals(x):
        rv, tv = x[:3], x[3:]
        a, _ = cv2.projectPoints(obj, rv, tv, KL, DL)
        Rl, _ = cv2.Rodrigues(rv)
        rvr2, _ = cv2.Rodrigues(R @ Rl)
        b, _ = cv2.projectPoints(obj, rvr2, R @ tv + T, KR, DR)
        return np.concatenate([(a.reshape(-1, 2) - pl).ravel(), (b.reshape(-1, 2) - pr).ravel()])

    x = least_squares(residuals, np.concatenate([rvl.ravel(), tvl.ravel()]), xtol=1e-15,
                      ftol=1e-15, gtol=1e-15).x
    res = residuals(x).reshape(2, -1, 2)
    fit_l.append(np.sqrt(np.mean(np.sum(res[0] ** 2, axis=1))))
    fit_r.append(np.sqrt(np.mean(np.sum(res[1] ** 2, axis=1))))
    corners.append(n)


def agg(values):
    c = np.array(corners, float)
    return float(np.sqrt(np.sum(c * np.array(values) ** 2) / np.sum(c)))


rms_c = report["rms_px"]
for name, value in (("free_pose_left", agg(free_l)), ("free_pose_right", agg(free_r)),
                    ("stereo_fit_left", agg(fit_l)), ("stereo_fit_right", agg(fit_r))):
    print("%-17s python %.6f  report %.6f  diff %.1e px" % (name, value, rms_c[name],
                                                         abs(value - rms_c[name])))
