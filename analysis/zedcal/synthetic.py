"""Synthetic stereo captures with a known truth, for the tests of the metrics
(spec nucleo-offline R11).

The generated detections.json has the format of the C++ tools. The board can be bent (a
dome with a given sagitta at its centre) or printed at a wrong scale: the file always
declares the nominal target, as a real capture would.
"""
import json
from typing import List, Optional, Tuple

import cv2
import numpy as np

from .calib_file import StereoCalibration
from .residuals import Camera, project, right_pose
from .target import Target

KALIBR_6X6 = Target(type="aprilgrid", rows=6, cols=6, tag_size_mm=88.0, tag_spacing_ratio=0.3)


def zed_like_rig(model: str = "rational8") -> StereoCalibration:
    """A ZED 2i-like pair at 1280x720: f ~ 534 px, baseline 120 mm."""
    if model == "fisheye4":
        d = np.array([0.02, -0.01, 0.002, -0.0005])
    else:
        d = np.array([-0.05, 0.03, 0.0003, -0.0002, -0.01, 0.0, 0.0, 0.0])[:8 if model == "rational8" else 5]
    R, _ = cv2.Rodrigues(np.array([0.0013, -0.0003, 0.0021]))
    return StereoCalibration(
        image_size=(1280, 720), model=model,
        K_left=np.array([[534.0, 0, 632.0], [0, 534.5, 369.0], [0, 0, 1]]), D_left=d,
        K_right=np.array([[533.5, 0, 641.5], [0, 534.0, 370.0], [0, 0, 1]]), D_right=d * 1.05,
        R=R, T=np.array([-120.0, -0.23, 0.08]))


def board_center(target: Target) -> np.ndarray:
    if target.type == "aprilgrid":
        side = target.cols * target.tag_size_mm + (target.cols - 1) * target.tag_size_mm * target.tag_spacing_ratio
        return np.array([side / 2, side / 2, 0.0])
    return np.array([(target.cols - 1) * target.square_size_mm / 2,
                     (target.rows - 1) * target.square_size_mm / 2, 0.0])


def random_poses(n: int, rng: np.random.Generator, target: Target = KALIBR_6X6,
                 distance_mm: Tuple[float, float] = (600.0, 1300.0), max_tilt_deg: float = 40.0,
                 max_offset: float = 0.35) -> List[Tuple[np.ndarray, np.ndarray]]:
    """Board poses (rvec, tvec) facing the camera, tilted and spread over the field."""
    poses = []
    c = board_center(target)
    for _ in range(n):
        z = rng.uniform(*distance_mm)
        tilt = np.radians(rng.uniform(-max_tilt_deg, max_tilt_deg, 2))
        spin = rng.uniform(-np.pi, np.pi)
        Rb, _ = cv2.Rodrigues(np.array([np.pi + tilt[0], tilt[1], 0.0]))  # board facing the camera
        Rs, _ = cv2.Rodrigues(np.array([0.0, 0.0, spin]))
        R = Rb @ Rs
        offset = np.array([rng.uniform(-max_offset, max_offset) * z,
                           rng.uniform(-max_offset, max_offset) * z * 0.56, z])
        t = offset - R @ c  # the board centre lands at 'offset'
        rvec, _ = cv2.Rodrigues(R)
        poses.append((rvec.reshape(3), t))
    return poses


def _true_points(nominal: np.ndarray, target: Target, flex_mm: float, scale: float) -> np.ndarray:
    pts = nominal * scale
    if flex_mm:
        c = board_center(target) * scale
        r2 = np.sum((pts[:, :2] - c[:2]) ** 2, axis=1)
        pts = pts.copy()
        pts[:, 2] += flex_mm * (1.0 - r2 / np.max(np.sum(c[:2] ** 2)))
    return pts


def generate(calib: StereoCalibration, poses, target: Target = KALIBR_6X6, sigma_px: float = 0.0,
             rng: Optional[np.random.Generator] = None, flex_mm: float = 0.0,
             scale: float = 1.0) -> dict:
    """A detections.json (as a dict) of the board seen in every pose by both cameras.

    A tag is kept by a camera when its 4 corners are inside the image and in front of it.
    Noise is Gaussian, sigma_px per coordinate; unrefined corners equal the refined ones.
    """
    if target.type != "aprilgrid":
        raise ValueError("the generator writes AprilGrid captures")
    rng = rng or np.random.default_rng(0)
    w, h = calib.image_size
    cams = {"left": Camera(calib.K_left, calib.D_left, calib.fisheye),
            "right": Camera(calib.K_right, calib.D_right, calib.fisheye)}
    frames = []
    for n, (rvec, tvec) in enumerate(poses):
        frame = {"pose_id": "synthetic", "n": n,
                 "files": {"left": "left/synthetic_%d.png" % n, "right": "right/synthetic_%d.png" % n}}
        pose_of = {"left": (rvec, tvec), "right": right_pose(rvec, tvec, calib.R, calib.T)}
        for side, cam in cams.items():
            rv, tv = pose_of[side]
            Rm, _ = cv2.Rodrigues(np.asarray(rv, np.float64).reshape(3, 1))
            tags = []
            for tag_id in range(target.rows * target.cols):
                pts = _true_points(target.tag_corners(tag_id), target, flex_mm, scale)
                if np.any((pts @ Rm.T + tv)[:, 2] <= 0):
                    continue
                img = project(pts, rv, tv, cam)
                if np.any(img < 0) or np.any(img[:, 0] >= w) or np.any(img[:, 1] >= h):
                    continue
                if sigma_px:
                    img = img + rng.normal(0.0, sigma_px, img.shape)
                tags.append({"id": tag_id, "corners": img.tolist(), "corners_unrefined": img.tolist()})
            frame[side] = {"tags": tags}
        frames.append(frame)
    return {
        "schema_version": 1, "image_size": [w, h],
        "target": {"type": "aprilgrid", "rows": target.rows, "cols": target.cols,
                   "tag_size_mm": target.tag_size_mm, "tag_spacing_ratio": target.tag_spacing_ratio,
                   "family": "tag36h11", "id_convention": "kalibr",
                   "corner_order": ["BR", "BL", "TL", "TR"], "source": "synthetic"},
        "detector": {"refinement": "synthetic", "params": {}},
        "frames": frames,
    }


def write(path: str, detections: dict) -> None:
    with open(path, "w") as f:
        json.dump(detections, f)
