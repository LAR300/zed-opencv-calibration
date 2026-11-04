"""Projection, signed residuals and target poses (spec nucleo-offline R9).

Residuals are projected - detected, per corner, with x and y kept separate (the Bias Ratio
pools both coordinates). Planar poses come from SOLVEPNP_IPPE through solvePnPGeneric, which
returns the two solutions of the planar ambiguity; the one with the lower residual is kept
and refined with Levenberg-Marquardt (Hagemann 2022, pitfall 5).
"""
from dataclasses import dataclass
from typing import Tuple

import cv2
import numpy as np


@dataclass
class Camera:
    K: np.ndarray
    D: np.ndarray
    fisheye: bool = False


def left_camera(calib) -> Camera:
    return Camera(calib.K_left, calib.D_left, calib.fisheye)


def right_camera(calib) -> Camera:
    return Camera(calib.K_right, calib.D_right, calib.fisheye)


def project(obj: np.ndarray, rvec, tvec, cam: Camera) -> np.ndarray:
    obj = np.asarray(obj, np.float64).reshape(-1, 1, 3)
    rvec = np.asarray(rvec, np.float64).reshape(3, 1)
    tvec = np.asarray(tvec, np.float64).reshape(3, 1)
    if cam.fisheye:
        pts, _ = cv2.fisheye.projectPoints(obj, rvec, tvec, cam.K, cam.D)
    else:
        pts, _ = cv2.projectPoints(obj, rvec, tvec, cam.K, cam.D)
    return pts.reshape(-1, 2)


def residuals(obj, img, rvec, tvec, cam: Camera) -> np.ndarray:
    """Signed residuals (N x 2): projected - detected."""
    return project(obj, rvec, tvec, cam) - np.asarray(img, np.float64).reshape(-1, 2)


def rms_per_corner(res: np.ndarray) -> float:
    """OpenCV's convention: sqrt(sum |e|^2 / corners)."""
    res = np.asarray(res, np.float64).reshape(-1, 2)
    return float(np.sqrt(np.mean(np.sum(res ** 2, axis=1))))


def _normalized(img, cam: Camera) -> Tuple[np.ndarray, Camera]:
    """For the fisheye model, PnP runs on undistorted normalized points."""
    pts = np.asarray(img, np.float64).reshape(-1, 1, 2)
    und = cv2.fisheye.undistortPoints(pts, cam.K, cam.D)
    return und.reshape(-1, 2), Camera(np.eye(3), np.zeros(5), False)


def pose(obj, img, cam: Camera) -> Tuple[np.ndarray, np.ndarray]:
    """Pose of a planar target (z = 0) from 4 or more corners."""
    obj = np.asarray(obj, np.float64).reshape(-1, 3)
    img = np.asarray(img, np.float64).reshape(-1, 2)
    solve_img, solve_cam = (img, cam) if not cam.fisheye else _normalized(img, cam)
    _, rvecs, tvecs, _ = cv2.solvePnPGeneric(obj, solve_img, solve_cam.K, solve_cam.D,
                                             flags=cv2.SOLVEPNP_IPPE)
    best = None
    for rv, tv in zip(rvecs, tvecs):
        rv, tv = cv2.solvePnPRefineLM(obj, solve_img, solve_cam.K, solve_cam.D, rv.copy(), tv.copy())
        err = rms_per_corner(residuals(obj, solve_img, rv, tv, solve_cam))
        if best is None or err < best[0]:
            best = (err, rv.reshape(3), tv.reshape(3))
    if best is None:
        raise ValueError("no pose found")
    return best[1], best[2]


def right_pose(rvec_l, tvec_l, R, T) -> Tuple[np.ndarray, np.ndarray]:
    """Pose of the target in the right camera from the left pose and the stereo R, T."""
    Rl, _ = cv2.Rodrigues(np.asarray(rvec_l, np.float64).reshape(3, 1))
    rvec_r, _ = cv2.Rodrigues(R @ Rl)
    return rvec_r.reshape(3), R @ np.asarray(tvec_l, np.float64).reshape(3) + np.asarray(T).reshape(3)
