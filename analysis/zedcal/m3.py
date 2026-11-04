"""M3, generalisation: a calibration evaluated on the images of other captures (spec
validacao-cruzada).

For every view of the evaluated capture: the target pose by PnP in the LEFT image with the
calibration's intrinsics fixed (every left tag), the left RMS in that pose, the RIGHT transfer
RMS (the left pose composed with the calibration's R, T, projected with the right intrinsics
onto every right tag; this tests the extrinsics), and the stereo value of zed_stereo_checker,
sqrt((e_L^2 + e_R^2) / 2). Per-corner RMS convention.
"""
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence

import numpy as np

from . import io
from .residuals import left_camera, pose, residuals, right_camera, right_pose, rms_per_corner

METRICS = ("left", "right_transfer", "stereo")


@dataclass
class Evaluation:
    """One calibration on one capture."""
    left_per_view: List[float] = field(default_factory=list)
    right_per_view: List[float] = field(default_factory=list)
    stereo_per_view: List[float] = field(default_factory=list)
    distance_mm: List[float] = field(default_factory=list)
    left_points: List[np.ndarray] = field(default_factory=list)
    left_residuals: List[np.ndarray] = field(default_factory=list)
    right_residuals: List[np.ndarray] = field(default_factory=list)

    def rms(self) -> Dict[str, float]:
        l = np.concatenate(self.left_residuals) if self.left_residuals else np.zeros((0, 2))
        r = np.concatenate(self.right_residuals) if self.right_residuals else np.zeros((0, 2))
        el, er = rms_per_corner(l), rms_per_corner(r)
        return {"left": el, "right_transfer": er, "stereo": float(np.sqrt((el ** 2 + er ** 2) / 2))}

    def per_view_stats(self) -> Dict[str, Dict[str, float]]:
        out = {}
        for name, v in (("left", self.left_per_view), ("right_transfer", self.right_per_view),
                        ("stereo", self.stereo_per_view)):
            out[name] = {"median": float(np.median(v)), "p90": float(np.percentile(v, 90))}
        return out


def evaluate(calib, det: io.Detections, min_corners: int = 48) -> Evaluation:
    cam_l, cam_r = left_camera(calib), right_camera(calib)
    ev = Evaluation()
    for f in det.frames:
        try:
            obj_l, img_l, _ = io.mono_correspondences(f, "left", det.target)
            obj_r, img_r, _ = io.mono_correspondences(f, "right", det.target)
        except ValueError:
            continue
        if len(obj_l) < min_corners or len(obj_r) < min_corners:
            continue
        rv, tv = pose(obj_l, img_l, cam_l)
        res_l = residuals(obj_l, img_l, rv, tv, cam_l)
        rvr, tvr = right_pose(rv, tv, calib.R, calib.T)
        res_r = residuals(obj_r, img_r, rvr, tvr, cam_r)
        el, er = rms_per_corner(res_l), rms_per_corner(res_r)
        ev.left_per_view.append(el)
        ev.right_per_view.append(er)
        ev.stereo_per_view.append(float(np.sqrt((el ** 2 + er ** 2) / 2)))
        ev.distance_mm.append(float(np.linalg.norm(tv)))
        ev.left_points.append(img_l)
        ev.left_residuals.append(res_l)
        ev.right_residuals.append(res_r)
    if not ev.left_per_view:
        raise ValueError("no view with enough corners in both cameras")
    return ev


def matrix(calibs: Dict[str, object], dets: Dict[str, io.Detections],
           rows: Optional[Sequence[str]] = None, cols: Optional[Sequence[str]] = None) -> Dict:
    """Calibrations (rows) evaluated on captures (columns), per metric. With the same names in
    rows and columns this is the leave-one-capture-out matrix: the diagonal is training."""
    rows = list(rows or calibs)
    cols = list(cols or dets)
    out = {m: np.full((len(rows), len(cols)), np.nan) for m in METRICS}
    for i, r in enumerate(rows):
        for j, c in enumerate(cols):
            rms = evaluate(calibs[r], dets[c]).rms()
            for m in METRICS:
                out[m][i, j] = rms[m]
    return {"rows": rows, "cols": cols, "matrices": out}


def loco_summary(result: Dict) -> Dict:
    """Mean of the diagonal (training), mean off the diagonal (test) and their ratio, for the
    square part of a matrix whose rows and columns share names."""
    rows, cols = result["rows"], result["cols"]
    shared = [n for n in rows if n in cols]
    out = {}
    for m, mat in result["matrices"].items():
        idx_r = [rows.index(n) for n in shared]
        idx_c = [cols.index(n) for n in shared]
        sub = mat[np.ix_(idx_r, idx_c)]
        diag = np.diag(sub)
        off = sub[~np.eye(len(shared), dtype=bool)]
        out[m] = {"train_mean": float(np.mean(diag)), "test_mean": float(np.mean(off)) if off.size else None,
                  "test_over_train": float(np.mean(off) / np.mean(diag)) if off.size else None}
    return out


def by_distance(ev: Evaluation, bins_mm: Sequence[float]) -> Dict:
    d = np.array(ev.distance_mm)
    out = []
    for lo, hi in zip(bins_mm[:-1], bins_mm[1:]):
        sel = (d >= lo) & (d < hi)
        if not np.any(sel):
            out.append({"from_mm": lo, "to_mm": hi, "views": 0})
            continue
        l = np.concatenate([ev.left_residuals[i] for i in np.where(sel)[0]])
        r = np.concatenate([ev.right_residuals[i] for i in np.where(sel)[0]])
        out.append({"from_mm": lo, "to_mm": hi, "views": int(np.sum(sel)),
                    "left": rms_per_corner(l), "right_transfer": rms_per_corner(r)})
    return {"bins": out}


def by_region(ev: Evaluation, image_size, grid=(8, 8)) -> np.ndarray:
    """Left RMS per image cell (NaN where empty)."""
    from .m1 import residual_map
    return residual_map(np.concatenate(ev.left_points), np.concatenate(ev.left_residuals),
                        image_size, grid)["rms_px"]
