import cv2
import numpy as np

from zedcal.residuals import Camera, pose, project, residuals, right_pose, rms_per_corner
from zedcal.target import Target

K = np.array([[534.0, 0, 632.0], [0, 534.5, 369.0], [0, 0, 1]])
D = np.array([-0.05, 0.03, 0.0003, -0.0001, -0.01, 0.0, 0.0, 0.0])
CAM = Camera(K, D)
GRID = Target(type="aprilgrid", rows=6, cols=6, tag_size_mm=88.0, tag_spacing_ratio=0.3)


def board():
    return np.concatenate([GRID.tag_corners(i) for i in range(36)])


def test_exact_data_has_no_residual():
    obj = board()
    rv, tv = np.array([0.3, -0.2, 0.1]), np.array([-300.0, -250.0, 900.0])
    img = project(obj, rv, tv, CAM)
    rv2, tv2 = pose(obj, img, CAM)
    assert np.max(np.abs(residuals(obj, img, rv2, tv2, CAM))) < 1e-9
    np.testing.assert_allclose(tv2, tv, atol=1e-6)


def test_single_tag_keeps_the_lower_of_the_two_planar_solutions():
    obj = GRID.tag_corners(14)
    rv, tv = np.array([0.5, 0.4, 0.2]), np.array([-200.0, -150.0, 600.0])
    img = project(obj, rv, tv, CAM) + np.random.default_rng(0).normal(0, 0.05, (4, 2))
    rv2, tv2 = pose(obj, img, CAM)
    _, rvecs, tvecs, _ = cv2.solvePnPGeneric(obj, img, K, D, flags=cv2.SOLVEPNP_IPPE)
    others = [rms_per_corner(residuals(obj, img, a, b, CAM)) for a, b in zip(rvecs, tvecs)]
    assert rms_per_corner(residuals(obj, img, rv2, tv2, CAM)) <= min(others) + 1e-12


def test_fisheye_pose():
    cam = Camera(K, np.array([0.1, -0.02, 0.003, -0.0004]), fisheye=True)
    obj = board()
    rv, tv = np.array([0.2, 0.3, -0.1]), np.array([-330.0, -330.0, 1000.0])
    img = project(obj, rv, tv, cam)
    rv2, tv2 = pose(obj, img, cam)
    assert np.max(np.abs(residuals(obj, img, rv2, tv2, cam))) < 1e-6


def test_right_pose_matches_the_composition():
    R, _ = cv2.Rodrigues(np.array([0.001, -0.002, 0.003]))
    T = np.array([-120.0, 0.2, -0.1])
    rv, tv = np.array([0.1, 0.2, 0.3]), np.array([10.0, 20.0, 800.0])
    rvr, tvr = right_pose(rv, tv, R, T)
    p = np.array([100.0, 50.0, 0.0])
    Rl, _ = cv2.Rodrigues(rv)
    Rr, _ = cv2.Rodrigues(rvr)
    np.testing.assert_allclose(Rr @ p + tvr, R @ (Rl @ p + tv) + T, atol=1e-9)


def test_rms_convention():
    assert abs(rms_per_corner(np.array([[3.0, 4.0], [0.0, 0.0]])) - np.sqrt(12.5)) < 1e-12
