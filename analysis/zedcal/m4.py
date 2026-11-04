"""M4, task metrics: what the SLAM sees of a calibration (spec metricas-tarefa).

- Rectification on the points, not on the images: stereoRectify WITHOUT CALIB_ZERO_DISPARITY,
  common corners taken to rectified pixels with undistortPointsIter (tight criteria).
- Epipolar error: y_left - y_right of every rectified common corner (signed; the statistics
  use its absolute value).
- Metric error: corners triangulated with P1, P2 (mm) and distances compared with the
  MEASURED target: (a) the 4 sides of every tag, (b) homologous corners of neighbouring tags
  along a row and a column (the pitch), (c) the spans of the visible corners: the pairs at the
  two ends along X, along Y and along both diagonals of the board.
- Plane residual: plane fitted by SVD to the triangulated corners of a view.
- Disparity bias between two calibrations: d = f*B/Z, so for the same observed corners
  delta_d = f*B * (1/Z_b - 1/Z_a). The cx difference of P1/P2 that the brief names is reported
  too, but it is a framing choice of stereoRectify and not the bias (see the spec).
"""
from dataclasses import dataclass, field
from typing import Dict, List, Optional, Sequence, Tuple

import cv2
import numpy as np

from . import io
from .residuals import left_camera, right_camera
from .target import Target

UNDISTORT_CRITERIA = (cv2.TERM_CRITERIA_COUNT | cv2.TERM_CRITERIA_EPS, 100, 1e-12)
EPIPOLAR_THRESHOLDS_PX = (0.25, 0.1)  # brief: acceptable, excellent
DEPTHS_MM = (500.0, 1000.0, 2000.0, 3000.0)
METRIC_SETS = ("tag_sides", "pitch", "span")


@dataclass
class Rectification:
    R1: np.ndarray
    R2: np.ndarray
    P1: np.ndarray
    P2: np.ndarray

    @property
    def f(self) -> float:
        return float(self.P1[0, 0])

    @property
    def baseline_mm(self) -> float:
        return float(abs(self.P2[0, 3] / self.P2[0, 0]))

    @property
    def cx_difference(self) -> float:
        """cx_left - cx_right of P1, P2 (rectified px)."""
        return float(self.P1[0, 2] - self.P2[0, 2])


def rectify(calib, alpha: float = 0.0) -> Rectification:
    """stereoRectify without CALIB_ZERO_DISPARITY (R1)."""
    if calib.fisheye:
        R1, R2, P1, P2, _ = cv2.fisheye.stereoRectify(
            calib.K_left, calib.D_left, calib.K_right, calib.D_right, tuple(calib.image_size),
            calib.R, np.asarray(calib.T, np.float64).reshape(3, 1), 0, balance=1.0 - alpha)
    else:
        R1, R2, P1, P2, _, _, _ = cv2.stereoRectify(
            calib.K_left, calib.D_left, calib.K_right, calib.D_right, tuple(calib.image_size),
            calib.R, np.asarray(calib.T, np.float64).reshape(3, 1), flags=0, alpha=alpha)
    return Rectification(R1, R2, P1, P2)


def rectify_points(img, cam, Rr: np.ndarray, P: np.ndarray) -> np.ndarray:
    pts = np.asarray(img, np.float64).reshape(-1, 1, 2)
    if cam.fisheye:
        out = cv2.fisheye.undistortPoints(pts, cam.K, cam.D, R=Rr, P=P)
    else:
        out = cv2.undistortPointsIter(pts, cam.K, cam.D, Rr, P, UNDISTORT_CRITERIA)
    return out.reshape(-1, 2)


def triangulate(rect: Rectification, left: np.ndarray, right: np.ndarray) -> np.ndarray:
    """Points in the LEFT camera frame (mm), from rectified pixels."""
    X = cv2.triangulatePoints(rect.P1, rect.P2, left.T.astype(np.float64), right.T.astype(np.float64))
    X = (X[:3] / X[3]).T
    return X @ rect.R1  # rectified-left -> left: R1^T applied to row vectors


def plane_residual(X: np.ndarray) -> np.ndarray:
    """Signed orthogonal distances to the least-squares plane (SVD)."""
    c = X - X.mean(axis=0)
    normal = np.linalg.svd(c, full_matrices=False)[2][-1]
    return c @ normal


def metric_pairs(obj: np.ndarray, ids: Optional[List[int]], target: Target) -> Dict[str, List[Tuple[int, int]]]:
    """Index pairs of the three sets (a), (b), (c) over the corners of a view.

    obj holds the corners in the order of io.common_correspondences (4 per tag, sorted by id)
    for an AprilGrid, row-major for a checkerboard."""
    pairs = {s: [] for s in METRIC_SETS}
    if target.type == "aprilgrid":
        where = {t: 4 * k for k, t in enumerate(ids)}
        for t, base in where.items():
            pairs["tag_sides"] += [(base + i, base + (i + 1) % 4) for i in range(4)]
            for nb in ((t + 1) if (t + 1) % target.cols else None, t + target.cols):
                if nb is not None and nb in where:
                    pairs["pitch"] += [(base + i, where[nb] + i) for i in range(4)]
    else:
        n_r, n_c = target.rows, target.cols
        for r in range(n_r):
            for c in range(n_c):
                k = r * n_c + c
                if c + 1 < n_c:
                    pairs["pitch"].append((k, k + 1))
                if r + 1 < n_r:
                    pairs["pitch"].append((k, k + n_c))
    p = obj[:, :2]
    span = []
    for u in ((1, 0), (0, 1), (1, 1), (1, -1)):
        proj = p @ np.asarray(u, np.float64)
        pair = (int(np.argmin(proj)), int(np.argmax(proj)))
        if pair[0] != pair[1] and pair not in span and pair[::-1] not in span:
            span.append(pair)
    pairs["span"] = span
    return pairs


@dataclass
class Evaluation:
    """One calibration on one capture: values per corner or per distance pair."""
    epipolar: List[np.ndarray] = field(default_factory=list)      # per view, signed px
    left_points: List[np.ndarray] = field(default_factory=list)   # per view, original left px
    view_distance_mm: List[float] = field(default_factory=list)
    metric: Dict[str, List[np.ndarray]] = field(default_factory=lambda: {s: [] for s in METRIC_SETS})
    metric_points: Dict[str, List[np.ndarray]] = field(default_factory=lambda: {s: [] for s in METRIC_SETS})
    metric_distance: Dict[str, List[np.ndarray]] = field(default_factory=lambda: {s: [] for s in METRIC_SETS})
    metric_view: Dict[str, List[int]] = field(default_factory=lambda: {s: [] for s in METRIC_SETS})
    plane_rms_mm: List[float] = field(default_factory=list)
    plane_max_mm: List[float] = field(default_factory=list)
    depth_mm: List[np.ndarray] = field(default_factory=list)      # per view, Z of every corner
    points_3d: List[np.ndarray] = field(default_factory=list)     # per view, left camera frame, mm
    frame_index: List[int] = field(default_factory=list)          # per view, into det.frames
    rect: Optional[Rectification] = None


def evaluate(calib, det: io.Detections, target: Optional[Target] = None, alpha: float = 0.0,
             min_corners: int = 8) -> Evaluation:
    """target: the MEASURED target (Target.with_measured); the capture's own by default."""
    target = target or det.target
    cam_l, cam_r = left_camera(calib), right_camera(calib)
    rect = rectify(calib, alpha)
    ev = Evaluation(rect=rect)
    for k, f in enumerate(det.frames):
        try:
            _, img_l, img_r, ids = io.common_correspondences(f, det.target)
        except ValueError:
            continue
        if len(img_l) < min_corners:
            continue
        obj = (target.checkerboard_points() if target.type == "checkerboard"
               else np.concatenate([target.tag_corners(i) for i in ids]))
        rl = rectify_points(img_l, cam_l, rect.R1, rect.P1)
        rr = rectify_points(img_r, cam_r, rect.R2, rect.P2)
        X = triangulate(rect, rl, rr)
        dist = float(np.linalg.norm(X.mean(axis=0)))
        ev.epipolar.append(rl[:, 1] - rr[:, 1])
        ev.left_points.append(np.asarray(img_l, np.float64))
        ev.view_distance_mm.append(dist)
        ev.depth_mm.append(X[:, 2])
        ev.points_3d.append(X)
        ev.frame_index.append(k)
        for s, prs in metric_pairs(obj, ids, target).items():
            if not prs:
                continue
            i, j = np.array(prs).T
            true = np.linalg.norm(obj[i] - obj[j], axis=1)
            meas = np.linalg.norm(X[i] - X[j], axis=1)
            ev.metric[s].append(100.0 * (meas - true) / true)
            ev.metric_points[s].append((img_l[i] + img_l[j]) / 2.0)
            ev.metric_distance[s].append(np.full(len(prs), dist))
            ev.metric_view[s].append(len(ev.epipolar) - 1)
        res = plane_residual(X)
        ev.plane_rms_mm.append(float(np.sqrt(np.mean(res ** 2))))
        ev.plane_max_mm.append(float(np.max(np.abs(res))))
    if not ev.epipolar:
        raise ValueError("no view with at least %d common corners" % min_corners)
    return ev


def merge(evs: Sequence[Evaluation]) -> Evaluation:
    """The views of several captures pooled, for a calibration evaluated on all of them."""
    out = Evaluation(rect=evs[0].rect)
    offset = 0
    for ev in evs:
        for name in ("epipolar", "left_points", "view_distance_mm", "plane_rms_mm", "plane_max_mm", "depth_mm",
                     "points_3d"):
            getattr(out, name).extend(getattr(ev, name))
        for s in METRIC_SETS:
            for d in ("metric", "metric_points", "metric_distance"):
                getattr(out, d)[s].extend(getattr(ev, d)[s])
            out.metric_view[s].extend(v + offset for v in ev.metric_view[s])
        offset += len(ev.epipolar)
    return out


def _stats_abs(v: np.ndarray) -> Dict[str, float]:
    a = np.abs(v)
    return {"n": int(v.size), "rms": float(np.sqrt(np.mean(v ** 2))), "median_abs": float(np.median(a)),
            "p90_abs": float(np.percentile(a, 90)), "max_abs": float(np.max(a)), "mean": float(np.mean(v))}


def _grid_rms(points: np.ndarray, values: np.ndarray, image_size, grid=(8, 8)) -> np.ndarray:
    """RMS of 'values' per image cell (NaN where empty); same cell rule as m1.residual_map."""
    w, h = image_size
    cols, rows = grid
    cx = np.clip((points[:, 0] / w * cols).astype(int), 0, cols - 1)
    cy = np.clip((points[:, 1] / h * rows).astype(int), 0, rows - 1)
    out = np.full((rows, cols), np.nan)
    for r in range(rows):
        for c in range(cols):
            sel = (cy == r) & (cx == c)
            if np.any(sel):
                out[r, c] = float(np.sqrt(np.mean(values[sel] ** 2)))
    return out


def _bands(values: np.ndarray, distance: np.ndarray, bins_mm: Sequence[float]) -> List[Dict]:
    out = []
    for lo, hi in zip(bins_mm[:-1], bins_mm[1:]):
        sel = (distance >= lo) & (distance < hi)
        b = {"from_mm": lo, "to_mm": hi, "n": int(np.sum(sel))}
        if np.any(sel):
            b.update(_stats_abs(values[sel]))
        out.append(b)
    return out


def summary(ev: Evaluation, image_size, bins_mm: Sequence[float], grid=(8, 8)) -> Dict:
    e = np.concatenate(ev.epipolar)
    pts = np.concatenate(ev.left_points)
    d = np.concatenate([np.full(len(v), ev.view_distance_mm[k]) for k, v in enumerate(ev.epipolar)])
    epi = _stats_abs(e)
    for th in EPIPOLAR_THRESHOLDS_PX:
        epi["fraction_below_%g_px" % th] = float(np.mean(np.abs(e) < th))
    epi["by_distance"] = _bands(e, d, bins_mm)
    epi["region_rms_px"] = _grid_rms(pts, e, image_size, grid)
    metric = {}
    for s in METRIC_SETS:
        if not ev.metric[s]:
            metric[s] = {"n": 0}
            continue
        v = np.concatenate(ev.metric[s])
        metric[s] = _stats_abs(v)
        metric[s]["by_distance"] = _bands(v, np.concatenate(ev.metric_distance[s]), bins_mm)
        metric[s]["region_rms_pct"] = _grid_rms(np.concatenate(ev.metric_points[s]), v, image_size, grid)
    dist = np.array(ev.view_distance_mm)
    plane = {"views": len(dist), "rms_mm_median": float(np.median(ev.plane_rms_mm)),
             "rms_mm_p90": float(np.percentile(ev.plane_rms_mm, 90)),
             "max_mm": float(np.max(ev.plane_max_mm)),
             "rms_over_distance_median": float(np.median(np.array(ev.plane_rms_mm) / dist)),
             "by_distance": _bands(np.array(ev.plane_rms_mm), dist, bins_mm)}
    r = ev.rect
    return {"views": len(ev.epipolar), "epipolar_px": epi, "metric_pct": metric, "plane": plane,
            "rectified": {"f_px": r.f, "baseline_mm": r.baseline_mm, "cx_left": float(r.P1[0, 2]),
                          "cx_right": float(r.P2[0, 2]), "cx_difference": r.cx_difference}}


def disparity_bias(ev_a: Evaluation, ev_b: Evaluation) -> Dict:
    """Disparity bias of calibration b relative to a, on the same corners of one capture.

    delta_d = f_a*B_a * (1/Z_b - 1/Z_a) per corner (rectified px of a). A pure bias is the
    same at every depth; a scale error (f or B) makes it grow with the disparity, so a line
    fB/Z_b = slope * fB/Z_a + offset is fitted too. delta Z/Z at the reference depths uses the
    first-order rule of the spec, -delta_d * Z / (f*B) (Z shrinks when the disparity grows)."""
    if len(ev_a.depth_mm) != len(ev_b.depth_mm):
        raise ValueError("the two evaluations do not cover the same views")
    za, zb = np.concatenate(ev_a.depth_mm), np.concatenate(ev_b.depth_mm)
    if za.shape != zb.shape:
        raise ValueError("the two evaluations do not cover the same corners")
    fb = ev_a.rect.f * ev_a.rect.baseline_mm
    da, db = fb / za, fb / zb
    dd = db - da
    slope, offset = np.polyfit(da, db, 1)
    med = float(np.median(dd))
    return {"delta_d_px": {"median": med, "p10": float(np.percentile(dd, 10)), "p90": float(np.percentile(dd, 90))},
            "fit": {"slope": float(slope), "offset_px": float(offset)},
            "delta_z_over_z_pct": {"%g" % z: float(-100.0 * med * z / fb) for z in DEPTHS_MM},
            "cx_difference_change_px": ev_b.rect.cx_difference - ev_a.rect.cx_difference}
