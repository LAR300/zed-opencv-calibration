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


UNDISTORT_CRITERIA = (cv2.TERM_CRITERIA_COUNT | cv2.TERM_CRITERIA_EPS, 100, 1e-12)


def pose(obj, img, cam: Camera) -> Tuple[np.ndarray, np.ndarray]:
    """Pose of a planar target (z = 0) from 4 or more corners.

    Candidates: IPPE on points undistorted with tight criteria (the default undistortion inside
    solvePnPGeneric is too coarse at the border of a wide-angle lens: a 35 deg tilted board came
    out with 196 px of residual), IPPE as OpenCV does it, and the iterative PnP. Each is refined
    with LM on the distorted points and the lowest residual wins.
    """
    obj = np.asarray(obj, np.float64).reshape(-1, 3)
    img = np.asarray(img, np.float64).reshape(-1, 2)
    if cam.fisheye:
        norm = cv2.fisheye.undistortPoints(img.reshape(-1, 1, 2), cam.K, cam.D).reshape(-1, 2)
    else:
        norm = cv2.undistortPointsIter(img.reshape(-1, 1, 2), cam.K, cam.D, None, None,
                                       UNDISTORT_CRITERIA).reshape(-1, 2)
    eye, zero = np.eye(3), np.zeros(5)
    candidates = []
    _, rvecs, tvecs, _ = cv2.solvePnPGeneric(obj, norm, eye, zero, flags=cv2.SOLVEPNP_IPPE)
    candidates += list(zip(rvecs, tvecs))
    if not cam.fisheye:
        _, rvecs, tvecs, _ = cv2.solvePnPGeneric(obj, img, cam.K, cam.D, flags=cv2.SOLVEPNP_IPPE)
        candidates += list(zip(rvecs, tvecs))
        if len(obj) >= 6:
            ok, rv, tv = cv2.solvePnP(obj, img, cam.K, cam.D, flags=cv2.SOLVEPNP_ITERATIVE)
            if ok:
                candidates.append((rv, tv))
    best = None
    for rv, tv in candidates:
        rv, tv = np.asarray(rv, np.float64).reshape(3, 1).copy(), np.asarray(tv, np.float64).reshape(3, 1).copy()
        if cam.fisheye:
            rv, tv = cv2.solvePnPRefineLM(obj, norm, eye, zero, rv, tv)
        else:
            rv, tv = cv2.solvePnPRefineLM(obj, img, cam.K, cam.D, rv, tv)
        err = rms_per_corner(residuals(obj, img, rv, tv, cam))
        if np.isfinite(err) and (best is None or err < best[0]):
            best = (err, rv.reshape(3), tv.reshape(3))
    if best is None:
        raise ValueError("no pose found")
    return best[1], best[2]


def right_pose(rvec_l, tvec_l, R, T) -> Tuple[np.ndarray, np.ndarray]:
    """Pose of the target in the right camera from the left pose and the stereo R, T."""
    Rl, _ = cv2.Rodrigues(np.asarray(rvec_l, np.float64).reshape(3, 1))
    rvec_r, _ = cv2.Rodrigues(R @ Rl)
    return rvec_r.reshape(3), R @ np.asarray(tvec_l, np.float64).reshape(3) + np.asarray(T).reshape(3)
