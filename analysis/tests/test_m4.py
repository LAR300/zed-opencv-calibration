"""M4: task metrics (spec metricas-tarefa)."""
import copy

import cv2
import numpy as np

from zedcal import io, m4
from zedcal.calib_file import StereoCalibration
from zedcal.residuals import Camera, project
from zedcal.synthetic import KALIBR_6X6, board_center, generate, random_poses, write, zed_like_rig


def capture(tmp_path, rig, n=15, seed=3, name="c.json", **kw):
    poses = random_poses(n, np.random.default_rng(seed), distance_mm=(700.0, 1600.0), max_tilt_deg=30.0,
                         max_offset=0.25)
    write(str(tmp_path / name), generate(rig, poses, **kw))
    return io.load_detections(str(tmp_path / name))


def test_parallel_pair_rectifies_to_the_pinhole_projection():
    K = np.array([[534.0, 0, 640.0], [0, 534.0, 360.0], [0, 0, 1]])
    rig = StereoCalibration((1280, 720), "rational8", K, np.zeros(8), K.copy(), np.zeros(8), np.eye(3),
                            np.array([-120.0, 0.0, 0.0]))
    rect = m4.rectify(rig)
    X = np.array([[0, 0, 1500.0], [200, -100, 1200], [-300, 150, 2000]])
    img = project(X, np.zeros(3), np.zeros(3), Camera(K, np.zeros(8)))
    got = m4.rectify_points(img, Camera(K, np.zeros(8)), rect.R1, rect.P1)
    Xh = np.c_[X, np.ones(3)] @ rect.P1.T
    assert np.max(np.abs(got - Xh[:, :2] / Xh[:, 2:])) < 1e-9


def test_true_calibration_without_noise_is_exact(tmp_path):
    rig = zed_like_rig()
    det = capture(tmp_path, rig)
    s = m4.summary(m4.evaluate(rig, det), det.image_size, [0, 1000, 5000])
    assert s["views"] >= 10
    assert s["epipolar_px"]["max_abs"] < 1e-6
    for name in m4.METRIC_SETS:
        assert s["metric_pct"][name]["n"] > 0
        assert s["metric_pct"][name]["max_abs"] < 1e-6, name
    assert s["plane"]["max_mm"] < 1e-6
    assert s["epipolar_px"]["fraction_below_0.1_px"] == 1.0


def test_rotation_about_the_optical_axis_tilts_the_epipolar_error(tmp_path):
    rig = zed_like_rig()
    det = capture(tmp_path, rig)
    theta = np.radians(0.05)
    bad = copy.deepcopy(rig)
    bad.R = cv2.Rodrigues(np.array([0.0, 0.0, theta]))[0] @ rig.R
    ev = m4.evaluate(bad, det)
    e = np.concatenate(ev.epipolar)
    x = np.concatenate([m4.rectify_points(p, Camera(bad.K_left, bad.D_left), ev.rect.R1, ev.rect.P1)[:, 0]
                        for p in ev.left_points]) - ev.rect.P1[0, 2]
    slope, offset = np.polyfit(x, e, 1)
    # the right image turns by -theta relative to the left: y_l - y_r = theta * (x - cx)
    assert abs(slope / theta - 1.0) < 0.05, slope / theta
    assert abs(offset) < 0.02
    far = np.abs(x) > 250
    near = np.abs(x) < 50
    assert np.mean(np.abs(e[far])) > 4 * np.mean(np.abs(e[near]))


def test_baseline_one_percent_long_gives_one_percent_metric_error(tmp_path):
    rig = zed_like_rig()
    det = capture(tmp_path, rig)
    bad = copy.deepcopy(rig)
    bad.T = rig.T * 1.01
    s = m4.summary(m4.evaluate(bad, det), det.image_size, [0, 5000])
    for name in ("pitch", "span"):
        assert abs(s["metric_pct"][name]["mean"] - 1.0) < 0.02, (name, s["metric_pct"][name]["mean"])
    assert s["epipolar_px"]["max_abs"] < 1e-6


def test_measured_target_is_the_truth(tmp_path):
    rig = zed_like_rig()
    det = capture(tmp_path, rig, scale=1.02)          # printed 2 % large, file says nominal
    nominal = m4.summary(m4.evaluate(rig, det), det.image_size, [0, 5000])
    assert abs(nominal["metric_pct"]["pitch"]["mean"] - 2.0) < 1e-6
    measured = KALIBR_6X6.with_measured(88.0 * 1.02, 26.4 * 1.02)
    s = m4.summary(m4.evaluate(rig, det, target=measured), det.image_size, [0, 5000])
    assert s["metric_pct"]["pitch"]["max_abs"] < 1e-6


def test_view_with_two_distant_tags_gives_the_possible_sets(tmp_path):
    rig = zed_like_rig()
    det = capture(tmp_path, rig, n=1)
    f = det.frames[0]
    keep = sorted(set(f.left) & set(f.right))[:1]
    far = [t for t in sorted(set(f.left) & set(f.right)) if t not in (keep[0] + 1, keep[0] + 6)][-1]
    for side in ("left", "right"):
        tags = getattr(f, side)
        for t in list(tags):
            if t not in (keep[0], far):
                del tags[t]
    ev = m4.evaluate(rig, det)
    assert len(ev.metric["tag_sides"][0]) == 8
    assert ev.metric["pitch"] == []
    assert len(ev.metric["span"][0]) >= 1
    s = m4.summary(ev, det.image_size, [0, 5000])
    assert s["metric_pct"]["pitch"] == {"n": 0}


def test_plane_residual_of_a_bent_board(tmp_path):
    rig = zed_like_rig()
    det = capture(tmp_path, rig, flex_mm=1.0)
    ev = m4.evaluate(rig, det)
    # truth: the same dome, fitted directly on the 3D corners the views kept
    f = det.frames[0]
    ids = sorted(set(f.left) & set(f.right))
    pts = np.concatenate([KALIBR_6X6.tag_corners(i) for i in ids])
    c = board_center(KALIBR_6X6)
    r2 = np.sum((pts[:, :2] - c[:2]) ** 2, axis=1)
    pts[:, 2] += 1.0 * (1.0 - r2 / np.sum(c[:2] ** 2))
    expected = np.sqrt(np.mean(m4.plane_residual(pts) ** 2))
    assert abs(ev.plane_rms_mm[0] - expected) < 1e-6
    assert 0.1 < expected < 0.5


def test_known_disparity_bias(tmp_path):
    rig = zed_like_rig()
    det = capture(tmp_path, rig)
    shifted = copy.deepcopy(rig)
    shifted.K_right[0, 2] += 0.5
    ev_a, ev_b = m4.evaluate(rig, det), m4.evaluate(shifted, det)
    b = m4.disparity_bias(ev_a, ev_b)
    # 0.5 px of the original image is 0.5 * f_rect / f px after rectification; a cx_right
    # that is too large makes the disparity d = x_l - x_r grow
    expected = 0.5 * ev_a.rect.f / rig.K_right[0, 0]
    assert abs(b["delta_d_px"]["median"] - expected) < 0.01 * expected, b
    # nearly constant: moving cx also moves the distortion centre, so not exactly
    assert b["delta_d_px"]["p90"] - b["delta_d_px"]["p10"] < 0.05 * expected
    fb = ev_a.rect.f * ev_a.rect.baseline_mm
    assert abs(b["delta_z_over_z_pct"]["3000"] + 100 * b["delta_d_px"]["median"] * 3000 / fb) < 1e-9
    # the triangulated depths follow the rule to first order
    za, zb = np.concatenate(ev_a.depth_mm), np.concatenate(ev_b.depth_mm)
    rel = (zb - za) / za
    pred = -b["delta_d_px"]["median"] * za / fb
    assert np.max(np.abs(rel - pred)) < 0.05 * np.max(np.abs(pred))
    # and the cx difference of P1/P2 is not that bias (it is the framing of stereoRectify)
    assert abs(b["cx_difference_change_px"]) > 0.6


def test_command(tmp_path):
    import os
    import yaml
    from zedcal import m4_command
    from zedcal.calib_file import write_calibration
    rig = zed_like_rig()
    capture(tmp_path, rig, name="a.json")
    capture(tmp_path, rig, seed=4, name="b.json")
    long_b = copy.deepcopy(rig)
    long_b.T = rig.T * 1.01
    write_calibration(str(tmp_path / "true.yml"), rig)
    write_calibration(str(tmp_path / "long.yml"), long_b)
    cfg = {"captures": [{"name": "a", "detections": str(tmp_path / "a.json")},
                        {"name": "b", "detections": str(tmp_path / "b.json")}],
           "calibrations": {"true": str(tmp_path / "true.yml"), "long": str(tmp_path / "long.yml")},
           "reference_calibration": "true", "output_dir": str(tmp_path / "out")}
    yaml.safe_dump(cfg, open(tmp_path / "c.yaml", "w"))
    assert m4_command.main(["--config", str(tmp_path / "c.yaml")]) == 2   # no measured target
    assert not os.path.exists(tmp_path / "out")
    cfg["target_measured"] = {"tag_size_mm": 88.0, "tag_spacing_mm": 26.4}
    yaml.safe_dump(cfg, open(tmp_path / "c.yaml", "w"))
    assert m4_command.main(["--config", str(tmp_path / "c.yaml")]) == 0
    assert sorted(os.listdir(tmp_path / "out")) == ["config.yaml", "m4.json", "m4_long.png", "m4_true.png"]
    import json
    r = json.load(open(tmp_path / "out" / "m4.json"))
    assert abs(r["pooled"]["long"]["metric_pct"]["span"]["mean"] - 1.0) < 0.02
    assert r["pooled"]["true"]["metric_pct"]["span"]["max_abs"] < 1e-6
    # a longer baseline is a scale error, not a bias: the fitted slope moves, the offset stays ~0
    assert abs(r["disparity_bias"]["long"]["a"]["fit"]["slope"] - 1 / 1.01) < 1e-3
