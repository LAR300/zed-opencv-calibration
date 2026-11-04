import os

import numpy as np
import pytest

from zedcal import calib, io, synthetic
from zedcal.calib_file import read_calibration

BENCH = "/workspaces/isaac_ros-dev/dev/calib_runs/testes_persistencia/recalib_cs/captura_01"


def synthetic_detections(tmp_path, n=30, sigma=0.0, seed=1, model="rational8", **kw):
    rig = synthetic.zed_like_rig(model)
    poses = synthetic.random_poses(n, np.random.default_rng(seed))
    path = str(tmp_path / ("det_%s_%s.json" % (model, seed)))
    synthetic.write(path, synthetic.generate(rig, poses, sigma_px=sigma,
                                             rng=np.random.default_rng(seed + 100), **kw))
    return rig, io.load_detections(path)


def rel(a, b):
    return float(np.max(np.abs(np.asarray(a) - np.asarray(b)) / np.maximum(np.abs(np.asarray(b)), 1e-9)))


def board_projection(c, poses):
    from zedcal.residuals import left_camera, project, right_camera, right_pose
    pts = np.concatenate([synthetic.KALIBR_6X6.tag_corners(i) for i in range(36)])
    out = []
    for rv, tv in poses:
        out.append(project(pts, rv, tv, left_camera(c)))
        rvr, tvr = right_pose(rv, tv, c.R, c.T)
        out.append(project(pts, rvr, tvr, right_camera(c)))
    return np.concatenate(out)


@pytest.mark.parametrize("model", ["rational8", "radtan5"])
def test_exact_data_recovers_the_rig(tmp_path, model):
    # The points are float32, as in the C++ tool: "exact" data carries ~1e-5 px of
    # quantization, and the rational coefficients are strongly correlated, so the distortion
    # is compared through the projection, not coefficient by coefficient
    rig, det = synthetic_detections(tmp_path, model=model)
    r = calib.calibrate(det, calib.CalibrationOptions(model=model, rejection="none"))
    c = r.calibration
    assert rel(c.K_left, rig.K_left) < 1e-5 and rel(c.K_right, rig.K_right) < 1e-5
    assert rel(c.T, rig.T) < 1e-5
    assert np.max(np.abs(c.R - rig.R)) < 1e-7
    assert r.report["rms_px"]["stereo"] < 1e-4
    poses = synthetic.random_poses(10, np.random.default_rng(99))
    assert np.max(np.abs(board_projection(c, poses) - board_projection(rig, poses))) < 1e-3


def test_noise_gives_rms_near_sigma_sqrt2(tmp_path):
    _, det = synthetic_detections(tmp_path, sigma=0.1)
    r = calib.calibrate(det, calib.CalibrationOptions(rejection="none"))
    for key in ("stereo", "stereo_fit_left", "free_pose_right", "mono_stage_left"):
        assert abs(r.report["rms_px"][key] - 0.1 * np.sqrt(2)) < 0.015, key
    assert abs(r.report["mse_per_coordinate_px2"]["stereo"] - r.report["rms_px"]["stereo"] ** 2 / 2) < 1e-15


def test_variants_run_and_are_named(tmp_path):
    _, det = synthetic_detections(tmp_path, sigma=0.05)
    for opts in (calib.CalibrationOptions(stereo="fixed"), calib.CalibrationOptions(mono_points="common"),
                 calib.cpp_compatible()):
        r = calib.calibrate(det, opts)
        assert r.report["variant"] == opts.variant()
        assert r.report["rms_px"]["stereo"] < 0.2


def test_fisheye_mono_works_and_the_stereo_fit_says_why_not(tmp_path):
    rig, det = synthetic_detections(tmp_path, model="fisheye4")
    views = calib.build_views(det)
    rms, K, D, _ = calib._mono([v.obj_left for v in views if v.mono_ok[0]],
                               [v.img_left for v in views if v.mono_ok[0]],
                               det.image_size, np.eye(3), np.zeros(4), "fisheye4", False)
    assert rms < 1e-3 and rel(K, rig.K_left) < 1e-4
    with pytest.raises(NotImplementedError, match="fisheye stereo"):
        calib.calibrate(det, calib.CalibrationOptions(model="fisheye4", rejection="none"))


def test_too_few_views_is_an_error(tmp_path):
    _, det = synthetic_detections(tmp_path, n=10)
    with pytest.raises(ValueError, match="at least 20"):
        calib.calibrate(det)


def test_save_refuses_to_overwrite(tmp_path):
    _, det = synthetic_detections(tmp_path, n=22)
    r = calib.calibrate(det, calib.CalibrationOptions(rejection="none"))
    yml, rep = calib.save(r, str(tmp_path))
    assert read_calibration(yml).model == "rational8"
    with pytest.raises(FileExistsError):
        calib.save(r, str(tmp_path))


def test_cpp_compatible_mode_reproduces_the_cpp_calibration():
    if not os.path.exists(os.path.join(BENCH, "calib_cpp_rational8.yml")):
        pytest.skip("bench data not available")
    det = io.load_detections(os.path.join(BENCH, "detections.json"))
    r = calib.calibrate(det, calib.cpp_compatible())
    cpp = read_calibration(os.path.join(BENCH, "calib_cpp_rational8.yml"))
    c = r.calibration
    for a, b in ((c.K_left, cpp.K_left), (c.K_right, cpp.K_right), (c.T, cpp.T)):
        assert rel(a, b) < 1e-6
    assert np.max(np.abs(c.D_left - cpp.D_left)) < 1e-6
    assert np.max(np.abs(c.R - cpp.R)) < 1e-9
