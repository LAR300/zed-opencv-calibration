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
