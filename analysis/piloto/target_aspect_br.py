"""Exploratory (diagnostico-vies-aprilgrid): calibrate with the target model stretched along Y by
the aspect target_aspect.py found, and compare fy/fx and the Bias Ratio with the square model.

Usage (in the container, from analysis/): python3 piloto/target_aspect_br.py <stretch_pct>
"""
import sys
from dataclasses import dataclass, replace

import numpy as np

sys.path.insert(0, ".")
from zedcal import calib, io, m1  # noqa: E402
from zedcal.residuals import left_camera, right_camera  # noqa: E402
from zedcal.target import Target  # noqa: E402

P = "/workspaces/isaac_ros-dev/dev/calib_runs/piloto"
CAPTURES = {"v2": "2026-10-20_ar_aprilgrid_v2", "acrilico": "2026-10-23_acrilico_ar_aprilgrid",
            "controles": "2026-11-03_camera_settings"}


@dataclass(frozen=True)
class StretchedTarget(Target):
    stretch_y: float = 0.0

    def tag_corners(self, tag_id):
        c = super().tag_corners(tag_id)
        c[:, 1] *= 1.0 + self.stretch_y
        return c


def summary(det):
    r = calib.calibrate(det, calib.CalibrationOptions(mono_points="common", stereo="joint", rejection="none"))
    c = r.calibration
    out = [c.K_left[1, 1] / c.K_left[0, 0] - 1, c.K_right[1, 1] / c.K_right[0, 0] - 1, r.report["rms_px"]]
    for side, cam in (("left", left_camera(c)), ("right", right_camera(c))):
        b = m1.bias_ratio(m1.camera_fit(det, cam, side, "common"), cam, det.target, m1.N_INTRINSICS[c.model]).as_dict()
        out.append((b["br"], b["eps_bias_px"]))
    return out


if __name__ == "__main__":
    a = float(sys.argv[1]) / 100.0
    for name, run in CAPTURES.items():
        det = io.load_detections("%s/%s/detections.json" % (P, run))
        t = det.target
        stretched = replace(det, target=StretchedTarget(**{k: getattr(t, k) for k in t.__dataclass_fields__}, stretch_y=a))
        for label, d in (("square", det), ("stretched", stretched)):
            fl, fr, rms, (brl, el), (brr, er) = summary(d)
            print("%-10s %-9s fy/fx-1 L %+.3f%% R %+.3f%% | rms %s | BR L %.2f (eps %.3f) R %.2f (eps %.3f)" % (
                name, label, 100 * fl, 100 * fr, {k: round(v, 3) for k, v in rms.items()} if isinstance(rms, dict) else rms,
                brl, el, brr, er))
