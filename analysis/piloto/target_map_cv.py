"""Exploratory (diagnostico-vies-aprilgrid): does a measured map of the tag positions remove
the AprilGrid bias? Cross-validated: for each capture, the per-tag shifts (tag_coherence.py) are
estimated on the OTHER two captures, the target model is corrected with them, and the capture is
calibrated again; fy/fx, the RMS and the Bias Ratio are compared with the nominal grid. The
checkerboard capture is the reference (BR ~0.3).

Usage (in the container, from analysis/): python3 piloto/target_map_cv.py
"""
import sys
from dataclasses import dataclass, field, replace

import numpy as np

sys.path.insert(0, ".")
from zedcal import calib, io, m1  # noqa: E402
from zedcal.residuals import left_camera, right_camera  # noqa: E402
from zedcal.target import Target  # noqa: E402

import tag_coherence  # noqa: E402

P = tag_coherence.P
CAPTURES = tag_coherence.CAPTURES
OPTS = calib.CalibrationOptions(mono_points="common", stereo="joint", rejection="none")


@dataclass(frozen=True)
class MappedTarget(Target):
    offsets: tuple = ()   # ((id, dx_mm, dy_mm), ...)

    def tag_corners(self, tag_id):
        c = super().tag_corners(tag_id)
        for i, dx, dy in self.offsets:
            if i == tag_id:
                c = c.copy()
                c[:, 0] += dx
                c[:, 1] += dy
        return c


def summary(det):
    c = calib.calibrate(det, OPTS).calibration
    out = {"fy_fx_pct": 100 * (c.K_left[1, 1] / c.K_left[0, 0] - 1), "fx": c.K_left[0, 0],
           "baseline": c.baseline_mm}
    for side, cam in (("left", left_camera(c)), ("right", right_camera(c))):
        fit = m1.camera_fit(det, cam, side, "common")
        b = m1.bias_ratio(fit, cam, det.target, m1.N_INTRINSICS[c.model]).as_dict()
        out[side] = b
        out["rms_" + side] = float(np.sqrt(np.mean(np.sum(fit.all_residuals ** 2, axis=1))))
    return out


if __name__ == "__main__":
    dets = {n: io.load_detections("%s/%s/detections.json" % (P, r)) for n, r in CAPTURES.items()}

    def mapped_det(det, offsets):
        t = det.target
        base = {k: getattr(t, k) for k in Target.__dataclass_fields__}
        offs = tuple((int(i), float(d[0]), float(d[1])) for i, d in sorted(offsets.items()))
        return replace(det, target=MappedTarget(**base, offsets=offs))

    def estimate_map(names, passes=3):
        """Per-tag offsets (mm) from the given captures: calibrate with the current map, measure
        the per-tag shifts, add their mean over the captures; repeat."""
        offsets = {}
        for _ in range(passes):
            step = {}
            for n in names:
                d = mapped_det(dets[n], offsets)
                c = calib.calibrate(d, OPTS).calibration
                _, per_tag = tag_coherence.analyse(d, left_camera(c))
                for t, v in per_tag.items():
                    if len(v) >= 5:
                        step.setdefault(t, []).append(np.mean(v, axis=0))
            for t, v in step.items():
                if len(v) == len(names):
                    offsets[t] = offsets.get(t, np.zeros(2)) + np.mean(v, axis=0)
        return offsets

    for n, det in dets.items():
        offsets = estimate_map([o for o in dets if o != n])
        print("%-10s map from the other captures: %d tags, RMS offset %.3f mm" % (
            n, len(offsets), np.sqrt(np.mean([np.sum(v ** 2) for v in offsets.values()]))))
        mapped = mapped_det(det, offsets)
        for label, d in (("nominal", det), ("mapped", mapped)):
            s = summary(d)
            print("%-10s %-8s fy/fx-1 %+.3f%% fx %.2f B %.2f | RMS L %.3f R %.3f | BR L %.2f (eps %.3f) R %.2f (eps %.3f)" % (
                n, label, s["fy_fx_pct"], s["fx"], s["baseline"], s["rms_left"], s["rms_right"],
                s["left"]["br"], s["left"]["eps_bias_px"], s["right"]["br"], s["right"]["eps_bias_px"]))
