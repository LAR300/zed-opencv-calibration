"""Coverage check of a capture before calibrating (spec cobertura; brief R3).

- Corner density per camera on a cols x rows grid (every tag of the camera), with a minimum per
  cell and the four image-corner cells reported apart.
- Target tilt about the camera's horizontal and vertical axes, from the pose of the target
  (PnP with a given calibration, F by default), with the conventions of
  common/pose_script/pose_script.hpp: tilt_h > 0 when the top edge is farther, tilt_v > 0 when
  the right edge is farther. At least K poses beyond the threshold on each axis, optionally
  with both signs.
"""
from dataclasses import dataclass, field
from typing import Dict, List, Optional

import cv2
import numpy as np

from . import io
from .residuals import Camera, pose


def board_size_mm(target) -> float:
    if target.type == "aprilgrid":
        return target.cols * target.tag_size_mm + (target.cols - 1) * target.tag_size_mm * target.tag_spacing_ratio
    return (target.cols - 1) * target.square_size_mm


def view_geometry(cam: Camera, image_size, board_mm, rvec, tvec) -> Dict[str, float]:
    """Python twin of viewGeometry() in common/pose_script (checked on the shared fixture)."""
    W = H = float(board_mm)
    pts = np.array([[0, 0, 0], [W, 0, 0], [W, H, 0], [0, H, 0], [W / 2, H / 2, 0], [W / 2 + 1, H / 2, 0]], float)
    img, _ = cv2.projectPoints(pts, np.asarray(rvec, float).reshape(3, 1), np.asarray(tvec, float).reshape(3, 1),
                               cam.K, cam.D)
    img = img.reshape(-1, 2)
    w, h = image_size
    axis = img[5] - img[4]
    rot = np.degrees(np.arctan2(axis[1], axis[0])) % 360.0
    R, _ = cv2.Rodrigues(np.asarray(rvec, float).reshape(3, 1))
    n = R[:, 2].copy()
    if n[2] > 0:
        n = -n
    centre = R @ np.array([W / 2, H / 2, 0]) + np.asarray(tvec, float).ravel()
    return {"fraction": float((img[:4, 0].max() - img[:4, 0].min()) / w),
            "center_x": float(img[4, 0] / w), "center_y": float(img[4, 1] / h),
            "tilt_h_deg": float(np.degrees(np.arctan2(-n[1], -n[2]))),
            "tilt_v_deg": float(np.degrees(np.arctan2(n[0], -n[2]))),
            "rotation_deg": float(rot), "distance_m": float(np.linalg.norm(centre) / 1000.0)}


def density(det: io.Detections, side: str, cells=(8, 8)) -> np.ndarray:
    """Corner count per cell (rows x cols), every tag of the camera. A corner on a border goes
    to the cell on its right / below; the last row and column take the image edge."""
    cols, rows = cells
    w, h = det.image_size
    count = np.zeros((rows, cols), int)
    for f in det.frames:
        try:
            _, img, _ = io.mono_correspondences(f, side, det.target)
        except ValueError:
            continue
        if len(img) == 0:
            continue
        c = np.clip((img[:, 0] / w * cols).astype(int), 0, cols - 1)
        r = np.clip((img[:, 1] / h * rows).astype(int), 0, rows - 1)
        np.add.at(count, (r, c), 1)
    return count


def views(det: io.Detections, cam: Camera, min_corners: int = 16) -> List[Dict[str, float]]:
    out = []
    bm = board_size_mm(det.target)
    for f in det.frames:
        try:
            obj, img, _ = io.mono_correspondences(f, "left", det.target)
        except ValueError:
            continue
        if len(obj) < min_corners:
            continue
        rv, tv = pose(obj, img, cam)
        g = view_geometry(cam, det.image_size, bm, rv, tv)
        g.update({"pose_id": f.pose_id, "n": f.n})
        out.append(g)
    return out


@dataclass
class CoverageCriteria:
    min_corners_per_cell: int = 1      # NEEDS the user's N (spec cobertura); 1 = presence only
    cells: tuple = (8, 8)
    tilt_threshold_deg: float = 30.0
    min_tilted_per_axis: int = 2
    both_signs: bool = True


def check(det: io.Detections, cam: Camera, criteria: CoverageCriteria = CoverageCriteria()) -> Dict:
    reasons = []
    report = {"criteria": criteria.__dict__.copy(), "density": {}, "image_corner_cells": {}}
    rows, cols = criteria.cells[1], criteria.cells[0]
    corners = {"top-left": (0, 0), "top-right": (0, cols - 1), "bottom-left": (rows - 1, 0),
               "bottom-right": (rows - 1, cols - 1)}
    for side in ("left", "right"):
        d = density(det, side, criteria.cells)
        report["density"][side] = d.tolist()
        low = np.argwhere(d < criteria.min_corners_per_cell)
        report["image_corner_cells"][side] = {k: int(d[r, c]) for k, (r, c) in corners.items()}
        for k, (r, c) in corners.items():
            if d[r, c] < criteria.min_corners_per_cell:
                reasons.append("%s camera: image corner cell (%d,%d) %s has %d corners, below %d"
                               % (side, r, c, k, d[r, c], criteria.min_corners_per_cell))
        others = [(r, c) for r, c in low if (r, c) not in corners.values()]
        if others:
            reasons.append("%s camera: %d cells below %d corners: %s" % (
                side, len(others), criteria.min_corners_per_cell, ", ".join("(%d,%d)" % rc for rc in others)))
    v = views(det, cam)
    report["views"] = v
    th = criteria.tilt_threshold_deg
    for axis, key in (("horizontal", "tilt_h_deg"), ("vertical", "tilt_v_deg")):
        tilts = np.array([x[key] for x in v])
        pos, neg = int(np.sum(tilts > th)), int(np.sum(tilts < -th))
        report["tilt_" + axis] = {"above_plus": pos, "below_minus": neg,
                                  "histogram": np.histogram(tilts, bins=np.arange(-60, 61, 10))[0].tolist()}
        if pos + neg < criteria.min_tilted_per_axis:
            reasons.append("%s tilt insufficient: %d poses beyond +-%g deg, %d needed"
                           % (axis, pos + neg, th, criteria.min_tilted_per_axis))
        elif criteria.both_signs and (pos == 0 or neg == 0):
            reasons.append("%s tilt insufficient: poses beyond %g deg on one side only (+%d / -%d)"
                           % (axis, th, pos, neg))
    report["passed"] = not reasons
    report["reasons"] = reasons
    return report
