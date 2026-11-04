import os

import cv2
import numpy as np
import pytest

from zedcal import calib, io, m1, synthetic
from zedcal.residuals import Camera, left_camera


@pytest.fixture(scope="module")
def unbiased(tmp_path_factory):
    """sigma = 0.1 px, true model = fitted model"""
    tmp = tmp_path_factory.mktemp("m1")
    rig = synthetic.zed_like_rig()
    poses = synthetic.random_poses(30, np.random.default_rng(11))
    path = str(tmp / "d.json")
    synthetic.write(path, synthetic.generate(rig, poses, sigma_px=0.1, rng=np.random.default_rng(12)))
    det = io.load_detections(path)
    c = calib.calibrate(det, calib.CalibrationOptions(rejection="none")).calibration
    return det, c


def test_robust_sigma_of_a_gaussian():
    x = np.random.default_rng(0).normal(0, 0.2, 100000)
    assert abs(m1.robust_sigma(x) / 0.2 - 1) < 0.01


def test_robust_mse_ignores_outliers_but_the_plain_mse_does_not():
    rng = np.random.default_rng(1)
    x = rng.normal(0, 0.2, 100000)
    y = x.copy()
    y[:5000] = 5.0                       # 5% gross outliers
    assert abs(m1.robust_mse(y) / m1.robust_mse(x) - 1) < 0.03
    assert abs(m1.robust_mse(x) / 0.04 - 1) < 0.02      # unbiased on clean Gaussian data
    assert m1.mse_plain(y) > 10 * m1.mse_plain(x)
    # The MAD itself shifts ~6% with 5% contamination
    assert abs(m1.robust_sigma(y) / m1.robust_sigma(x) - 1) < 0.10


def test_mad_is_biased_on_a_scale_mixture_and_the_trimmed_mse_is_not():
    # Residuals with unequal variances, as the corners of a 4-point pose: the reason for
    # robust_mse (module docstring)
    rng = np.random.default_rng(2)
    # (variances 2x apart; the real virtual-target residuals are milder: 0.5% error measured)
    x = np.concatenate([rng.normal(0, 0.07, 50000), rng.normal(0, 0.14, 50000)])
    rms = np.sqrt(np.mean(x ** 2))
    mad_error = abs(m1.robust_sigma(x) / rms - 1)
    trimmed_error = abs(np.sqrt(m1.robust_mse(x)) / rms - 1)
    assert mad_error > 0.04
    assert trimmed_error < 0.01 and trimmed_error < mad_error / 4


def test_dof_factor():
    assert m1.dof_factor(8, 6) == 0.25
    with pytest.raises(ValueError):
        m1.dof_factor(6, 6)


def test_unbiased_data_gives_a_small_bias_ratio(unbiased):
    det, c = unbiased
    cam = left_camera(c)
    fit = m1.camera_fit(det, cam, "left")
    b = m1.bias_ratio(fit, cam, det.target, m1.N_INTRINSICS["rational8"])
    assert b.br < 0.05, b
    assert abs(b.sigma_d_px / 0.1 - 1) < 0.10, b
    assert b.virtual_discarded == 0


def test_decomposition_closes(unbiased):
    det, c = unbiased
    cam = left_camera(c)
    b = m1.bias_ratio(m1.camera_fit(det, cam, "left"), cam, det.target, 12)
    factor = 1 - b.n_params / b.n_obs
    # MSE_calib = (sigma_d^2 + eps^2) (1 - N_P/N) whenever eps^2 > 0
    if b.eps_bias_px > 0:
        assert abs((b.sigma_d_px ** 2 + b.eps_bias_px ** 2) * factor - b.mse_calib_robust_px2) < 1e-12


def test_wrong_convention_gives_about_one_half(unbiased):
    # Pitfall 1: OpenCV's RMS^2 taken as the per-coordinate MSE doubles MSE_calib
    det, c = unbiased
    cam = left_camera(c)
    fit = m1.camera_fit(det, cam, "left")
    rms_cv = m1.rms_per_corner(fit.all_residuals)
    b = m1.bias_ratio(fit, cam, det.target, 12, mse_calib=rms_cv ** 2)
    assert abs(b.br - 0.5) < 0.08, b


def test_omitted_distortion_gives_a_high_bias_ratio(tmp_path):
    rig = synthetic.zed_like_rig()
    rig.D_left = np.array([-0.30, 0.10, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0])
    poses = synthetic.random_poses(30, np.random.default_rng(21))
    path = str(tmp_path / "d.json")
    synthetic.write(path, synthetic.generate(rig, poses, sigma_px=0.1, rng=np.random.default_rng(22)))
    det = io.load_detections(path)
    views = [v for v in calib.build_views(det) if v.mono_ok[0]]
    flags = (cv2.CALIB_FIX_K1 | cv2.CALIB_FIX_K2 | cv2.CALIB_FIX_K3 | cv2.CALIB_ZERO_TANGENT_DIST)
    _, K, _, _, _ = cv2.calibrateCamera([v.obj_left.astype(np.float32) for v in views],
                                        [v.img_left.astype(np.float32) for v in views],
                                        det.image_size, np.eye(3), np.zeros(5), flags=flags)
    cam = Camera(K, np.zeros(5))
    b = m1.bias_ratio(m1.camera_fit(det, cam, "left", points="all"), cam, det.target,
                      m1.N_INTRINSICS["pinhole"])
    assert b.br > 0.5, b


def test_bent_board_raises_the_bias_ratio(tmp_path, unbiased):
    det0, c0 = unbiased
    b0 = m1.bias_ratio(m1.camera_fit(det0, left_camera(c0), "left"), left_camera(c0), det0.target, 12)
    rig = synthetic.zed_like_rig()
    poses = synthetic.random_poses(30, np.random.default_rng(11))
    path = str(tmp_path / "bent.json")
    synthetic.write(path, synthetic.generate(rig, poses, sigma_px=0.1, rng=np.random.default_rng(12),
                                             flex_mm=1.0))
    det = io.load_detections(path)
    c = calib.calibrate(det, calib.CalibrationOptions(rejection="none")).calibration
    b = m1.bias_ratio(m1.camera_fit(det, left_camera(c), "left"), left_camera(c), det.target, 12)
    assert b.br > b0.br, (b.br, b0.br)


def test_rms_report_without_noise_and_with_noise(tmp_path):
    rig = synthetic.zed_like_rig()
    poses = synthetic.random_poses(8, np.random.default_rng(31))
    for sigma, expected in ((0.0, 0.0), (0.1, 0.1 * np.sqrt(2))):
        path = str(tmp_path / ("r%s.json" % sigma))
        synthetic.write(path, synthetic.generate(rig, poses, sigma_px=sigma, rng=np.random.default_rng(32)))
        det = io.load_detections(path)
        r = m1.rms_report(det, rig)           # evaluated with the true calibration
        for key in ("stereo", "stereo_fit_left", "free_pose_right"):
            assert abs(r["rms_px"][key] - expected) < (1e-6 if sigma == 0 else 0.015), (key, r)
        assert r["filter"]["passed"]
        assert m1.rms_report(det, rig, max_rms_px=0.05)["filter"]["passed"] == (sigma == 0)


def test_residual_map_shows_an_imposed_vertical_bias():
    rng = np.random.default_rng(41)
    pts = rng.uniform([0, 0], [1280, 720], (20000, 2))
    res = rng.normal(0, 0.05, (20000, 2))
    res[pts[:, 1] < 360, 1] += 0.2           # +0.2 px in y in the upper half
    m = m1.residual_map(pts, res, (1280, 720), (8, 8))
    assert np.all(m["mean_y_px"][:4] > 0.15) and np.all(np.abs(m["mean_y_px"][4:]) < 0.05)
    assert np.all(np.abs(m["mean_x_px"]) < 0.05)


def test_residual_map_empty_cell_is_nan_not_zero():
    m = m1.residual_map(np.array([[10.0, 10.0]]), np.array([[0.1, 0.1]]), (1280, 720), (8, 8))
    assert m["count"][0, 0] == 1 and np.isnan(m["rms_px"][7, 7]) and np.isnan(m["mean_x_px"][3, 3])


def test_m1_command_writes_report_and_figures(tmp_path):
    import yaml
    from zedcal import m1_command
    from zedcal.calib_file import write_calibration
    rig = synthetic.zed_like_rig()
    det_path = str(tmp_path / "d.json")
    synthetic.write(det_path, synthetic.generate(rig, synthetic.random_poses(10, np.random.default_rng(5)),
                                                 sigma_px=0.1, rng=np.random.default_rng(6)))
    write_calibration(str(tmp_path / "truth.yml"), rig)
    cfg = {"detections": det_path, "calibration": str(tmp_path / "truth.yml"),
           "output_dir": str(tmp_path / "m1")}
    yaml.safe_dump(cfg, open(tmp_path / "m1.yaml", "w"))
    assert m1_command.main(["--config", str(tmp_path / "m1.yaml")]) == 0
    files = sorted(os.listdir(tmp_path / "m1"))
    assert files == ["config.yaml", "m1_truth.json", "m1_truth_left.png", "m1_truth_right.png"]
