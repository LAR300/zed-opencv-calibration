import os

import numpy as np
import pytest
from dataclasses import replace

from zedcal import calib, io, m1, m3, synthetic
from zedcal.residuals import left_camera


@pytest.fixture(scope="module")
def captures(tmp_path_factory):
    tmp = tmp_path_factory.mktemp("m3")
    rig = synthetic.zed_like_rig()
    dets = {}
    for k in range(3):
        path = str(tmp / ("c%d.json" % k))
        synthetic.write(path, synthetic.generate(rig, synthetic.random_poses(22, np.random.default_rng(500 + k)),
                                                 sigma_px=0.1, rng=np.random.default_rng(600 + k)))
        dets["c%d" % k] = io.load_detections(path)
    return rig, dets


def test_diagonal_reproduces_the_free_pose_rms_of_m1(captures):
    rig, dets = captures
    det = dets["c0"]
    c = calib.calibrate(det, calib.CalibrationOptions(rejection="none")).calibration
    ev = m3.evaluate(c, det)
    fit = m1.camera_fit(det, left_camera(c), "left", points="all")
    assert abs(ev.rms()["left"] - m1.rms_per_corner(fit.all_residuals)) < 1e-6


def test_true_model_on_another_capture_is_at_the_noise_level(captures):
    rig, dets = captures
    r = m3.evaluate(rig, dets["c1"]).rms()
    assert abs(r["left"] - 0.1 * np.sqrt(2)) < 0.015
    assert abs(r["right_transfer"] - 0.1 * np.sqrt(2)) < 0.02


def test_wrong_fx_is_worse_off_the_diagonal(captures):
    rig, dets = captures
    bad = replace(rig, K_left=rig.K_left * np.array([[1.01, 1, 1], [1, 1, 1], [1, 1, 1]]))
    good = m3.evaluate(rig, dets["c2"]).rms()["left"]
    assert m3.evaluate(bad, dets["c2"]).rms()["left"] > 2 * good


def test_wrong_baseline_only_hits_the_right_transfer(captures):
    rig, dets = captures
    bad = replace(rig, T=rig.T + np.array([1.0, 0, 0]))
    a, b = m3.evaluate(rig, dets["c0"]).rms(), m3.evaluate(bad, dets["c0"]).rms()
    assert abs(b["left"] - a["left"]) < 1e-9
    assert b["right_transfer"] > 2 * a["right_transfer"]


def test_loco_matrix_and_summary(captures):
    rig, dets = captures
    calibs = {k: calib.calibrate(d, calib.CalibrationOptions(rejection="none")).calibration for k, d in dets.items()}
    calibs["F"] = rig
    res = m3.matrix(calibs, dets, rows=["c0", "c1", "c2", "F"])
    assert res["matrices"]["left"].shape == (4, 3)
    s = m3.loco_summary(res)
    assert s["left"]["test_over_train"] > 1.0          # testing is harder than training
    assert set(s) == set(m3.METRICS)


def test_distance_and_region(captures):
    rig, dets = captures
    ev = m3.evaluate(rig, dets["c0"])
    d = m3.by_distance(ev, [0, 800, 1000, 5000])
    assert sum(b["views"] for b in d["bins"]) == len(ev.left_per_view)
    reg = m3.by_region(ev, dets["c0"].image_size)
    assert reg.shape == (8, 8) and np.isnan(reg).sum() < 64
    stats = ev.per_view_stats()
    assert stats["left"]["p90"] >= stats["left"]["median"]
