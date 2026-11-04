"""Exploratory (diagnostico-vies-aprilgrid): is the Bias Ratio of the AprilGrid a border effect?

Each capture is calibrated with all its data (compat, rejection none); the Bias Ratio is then
computed for that calibration on the tags whose centre falls in the central part of the image
and, separately, on the others. The 9x6 checkerboard never reached the image borders, so a
model error there would show in the AprilGrid captures only.

Usage (in the container, from analysis/): python3 piloto/br_by_region.py [central_fraction]
"""
import sys
from dataclasses import replace

import numpy as np

sys.path.insert(0, ".")
from zedcal import calib, io, m1  # noqa: E402
from zedcal.residuals import left_camera, right_camera  # noqa: E402

P = "/workspaces/isaac_ros-dev/dev/calib_runs/piloto"
CAPTURES = {"v2": "2026-10-20_ar_aprilgrid_v2", "acrilico": "2026-10-23_acrilico_ar_aprilgrid",
            "controles": "2026-11-03_camera_settings"}


def keep(det, side_inside, frac):
    w, h = det.image_size

    def inside(c):
        m = np.mean(c, axis=0)
        return abs(m[0] - w / 2) < frac * w / 2 and abs(m[1] - h / 2) < frac * h / 2

    frames = []
    for f in det.frames:
        sel = lambda tags: {i: c for i, c in tags.items() if inside(c) == side_inside}
        frames.append(replace(f, left=sel(f.left), right=sel(f.right)))
    return replace(det, frames=frames)


if __name__ == "__main__":
    frac = float(sys.argv[1]) if len(sys.argv) > 1 else 0.6
    for name, run in CAPTURES.items():
        det = io.load_detections("%s/%s/detections.json" % (P, run))
        c = calib.calibrate(det, calib.CalibrationOptions(mono_points="common", stereo="joint",
                                                          rejection="none")).calibration
        for label, d in (("all", det), ("central", keep(det, True, frac)), ("border", keep(det, False, frac))):
            line = "%-10s %-8s" % (name, label)
            for side, cam in (("left", left_camera(c)), ("right", right_camera(c))):
                fit = m1.camera_fit(d, cam, side, "all", min_corners=8)
                b = m1.bias_ratio(fit, cam, d.target, m1.N_INTRINSICS[c.model]).as_dict()
                line += " | %s BR %.2f eps %.3f sd %.3f n %d" % (side[0].upper(), b["br"], b["eps_bias_px"],
                                                              b["sigma_d_px"], b["virtual_targets"])
            print(line)
