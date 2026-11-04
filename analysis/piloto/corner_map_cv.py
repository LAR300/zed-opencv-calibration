"""Exploratory (diagnostico-vies-aprilgrid): the same cross-validation as target_map_cv.py, with
one board-frame offset per CORNER (144) instead of per tag, to see whether what is left of the
bias is also a fixed pattern of the printed target (it repeats across captures) or not.

Usage (in the container, from analysis/): PYTHONPATH=piloto python3 piloto/corner_map_cv.py
"""
import sys
from dataclasses import dataclass, replace

import cv2
import numpy as np

sys.path.insert(0, ".")
from zedcal import calib, io, m1  # noqa: E402
from zedcal.residuals import left_camera, pose, residuals, right_camera  # noqa: E402
from zedcal.target import Target  # noqa: E402

import target_map_cv as T  # noqa: E402


@dataclass(frozen=True)
class CornerMappedTarget(Target):
    offsets: tuple = ()   # ((id, k, dx_mm, dy_mm), ...)

    def tag_corners(self, tag_id):
        c = super().tag_corners(tag_id).copy()
        for i, k, dx, dy in self.offsets:
            if i == tag_id:
                c[k, 0] += dx
                c[k, 1] += dy
        return c


def corner_shifts(det, cam):
    per = {}
    for f in det.frames:
        obj, img, ids = io.mono_correspondences(f, "left", det.target)
        if len(obj) < 48:
            continue
        rv, tv = pose(obj, img, cam)
        res = residuals(obj, img, rv, tv, cam)            # projected - detected
        proj = img + res
        Hm, _ = cv2.findHomography(proj.astype(np.float32), obj[:, :2].astype(np.float32))
        a = cv2.perspectiveTransform(proj.reshape(-1, 1, 2).astype(np.float32), Hm).reshape(-1, 2)
        b = cv2.perspectiveTransform(img.reshape(-1, 1, 2).astype(np.float32), Hm).reshape(-1, 2)
        for j, tid in enumerate(ids):
            for k in range(4):
                per.setdefault((tid, k), []).append(b[4 * j + k] - a[4 * j + k])
    return {key: np.mean(v, axis=0) for key, v in per.items() if len(v) >= 5}


def mapped(det, offsets):
    t = det.target
    base = {k: getattr(t, k) for k in Target.__dataclass_fields__}
    offs = tuple((int(i), int(k), float(d[0]), float(d[1])) for (i, k), d in sorted(offsets.items()))
    return replace(det, target=CornerMappedTarget(**base, offsets=offs))


def estimate(dets, names, passes=3):
    offsets = {}
    for _ in range(passes):
        step = {}
        for n in names:
            d = mapped(dets[n], offsets)
            c = calib.calibrate(d, T.OPTS).calibration
            for key, v in corner_shifts(d, left_camera(c)).items():
                step.setdefault(key, []).append(v)
        for key, v in step.items():
            if len(v) == len(names):
                offsets[key] = offsets.get(key, np.zeros(2)) + np.mean(v, axis=0)
    return offsets


if __name__ == "__main__":
    dets = {n: io.load_detections("%s/%s/detections.json" % (T.P, r)) for n, r in T.CAPTURES.items()}
    maps = {}
    for n, det in dets.items():
        offsets = estimate(dets, [o for o in dets if o != n])
        maps[n] = offsets
        s = T.summary(mapped(det, offsets))
        print("%-10s corner map (%d corners, RMS %.3f mm): fy/fx-1 %+.3f%% | RMS L %.3f R %.3f | BR L %.2f (eps %.3f) R %.2f (eps %.3f)" % (
            n, len(offsets), np.sqrt(np.mean([np.sum(v ** 2) for v in offsets.values()])), s["fy_fx_pct"],
            s["rms_left"], s["rms_right"], s["left"]["br"], s["left"]["eps_bias_px"], s["right"]["br"],
            s["right"]["eps_bias_px"]))
