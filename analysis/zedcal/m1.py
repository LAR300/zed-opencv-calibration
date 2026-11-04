"""M1, quality of the fit: RMS, residual map per image region and the Bias Ratio
(spec metrica-ajuste; Hagemann et al., IJCV 2022, referencias.md A).

Conventions (Hagemann; audit 3.4): N counts image COORDINATES (2 per corner), N_P the free
parameters (intrinsics + 6 per view), MSE values are per coordinate. OpenCV's per-corner RMS
squared is twice the per-coordinate MSE.

Bias Ratio, per camera, for a given calibration:
  MSE_calib  per-coordinate MSE of the calibration's residuals with one free pose per view,
             robust (see robust_mse)
  sigma_d^2  = 4 x robust MSE over the residuals of the virtual targets (each tag on its own,
             4 corners, pose re-estimated with the intrinsics fixed: 8 obs, 6 params)

robust_mse: the mean square after discarding |r - median| > 5 sigma_MAD. The brief's
(1.4826 MAD)^2 was measured biased on the virtual targets: the 4 corners of a tag have
different leverage in its pose, their residuals have different variances, and the MAD of that
mixture is ~4% below the root mean square the factor 4 assumes. On unbiased synthetic data that
gave BR = 0.08 instead of ~0; the trimmed mean square gives sigma_d within 0.5% and stays robust
to gross outliers (spec metrica-ajuste, R3).
  s_d^2      = MSE_calib / (1 - N_P / N)
  eps_bias^2 = max(s_d^2 - sigma_d^2, 0)
  BR         = eps_bias^2 (1 - N_P / N) / MSE_calib
"""
from dataclasses import dataclass, field
from typing import Dict, List, Optional

import numpy as np

from . import io
from .residuals import Camera, left_camera, pose, residuals, right_camera, right_pose, rms_per_corner

MAD_TO_SIGMA = 1.4826
N_INTRINSICS = {"rational8": 12, "radtan5": 9, "fisheye4": 8, "pinhole": 4}


def robust_sigma(values: np.ndarray) -> float:
    """1.4826 x median absolute deviation of the signed values (x and y pooled)."""
    v = np.asarray(values, np.float64).ravel()
    return float(MAD_TO_SIGMA * np.median(np.abs(v - np.median(v))))


def robust_mse(values: np.ndarray, k: float = 5.0) -> float:
    """Mean square of the values within k robust sigmas of the median (x and y pooled)."""
    v = np.asarray(values, np.float64).ravel()
    if v.size == 0:
        return float("nan")
    s = robust_sigma(v)
    kept = v[np.abs(v - np.median(v)) <= k * s] if s > 0 else v
    return float(np.mean(kept ** 2))


def mse_plain(values: np.ndarray) -> float:
    v = np.asarray(values, np.float64).ravel()
    return float(np.mean(v ** 2))


def dof_factor(n_obs: int, n_params: int) -> float:
    """1 - N_P / N, with N the number of coordinates."""
    if n_obs <= n_params:
        raise ValueError("%d observations cannot fit %d parameters" % (n_obs, n_params))
    return 1.0 - n_params / n_obs


@dataclass
class CameraFit:
    """Residuals of one camera with one free pose per view."""
    residuals: List[np.ndarray] = field(default_factory=list)   # per view, N x 2
    image_points: List[np.ndarray] = field(default_factory=list)
    tags: List[List[int]] = field(default_factory=list)
    object_points: List[np.ndarray] = field(default_factory=list)

    @property
    def all_residuals(self) -> np.ndarray:
        return np.concatenate(self.residuals) if self.residuals else np.zeros((0, 2))


def correspondences(frame, side, target, points):
    if points == "common":
        obj, il, ir, ids = io.common_correspondences(frame, target)
        return obj, (il if side == "left" else ir), ids
    return io.mono_correspondences(frame, side, target)


def camera_fit(det: io.Detections, cam: Camera, side: str, points: str = "common",
               min_corners: int = 48) -> CameraFit:
    fit = CameraFit()
    for f in det.frames:
        try:
            obj, img, ids = correspondences(f, side, det.target, points)
        except ValueError:
            continue
        if len(obj) < min_corners:
            continue
        rv, tv = pose(obj, img, cam)
        fit.residuals.append(residuals(obj, img, rv, tv, cam))
        fit.image_points.append(img)
        fit.object_points.append(obj)
        fit.tags.append(ids)
    return fit


def virtual_target_residuals(fit: CameraFit, cam: Camera, target):
    """Residuals of every tag (AprilGrid) or exclusive 2x2 corner square (checkerboard) with its
    own pose and the intrinsics fixed. Returns the residuals and the number discarded."""
    out, discarded = [], 0
    for obj, img, ids in zip(fit.object_points, fit.image_points, fit.tags):
        groups = []
        if ids is not None:  # AprilGrid: 4 corners per tag, in order
            groups = [np.arange(4 * k, 4 * k + 4) for k in range(len(ids))]
        else:                # checkerboard: exclusive squares of 2x2 inner corners
            for r in range(0, target.rows - 1, 2):
                for c in range(0, target.cols - 1, 2):
                    a = r * target.cols + c
                    groups.append(np.array([a, a + 1, a + target.cols, a + target.cols + 1]))
        for g in groups:
            try:
                rv, tv = pose(obj[g], img[g], cam)
                res = residuals(obj[g], img[g], rv, tv, cam)
            except (ValueError, Exception):  # cv2 errors on degenerate quads
                discarded += 1
                continue
            if not np.all(np.isfinite(res)):
                discarded += 1
                continue
            out.append(res)
    return (np.concatenate(out) if out else np.zeros((0, 2))), discarded


@dataclass
class BiasRatio:
    br: float
    sqrt_br: float
    eps_bias_px: float
    sigma_d_px: float
    mse_calib_robust_px2: float
    mse_calib_plain_px2: float
    n_obs: int
    n_params: int
    virtual_targets: int
    virtual_discarded: int

    def as_dict(self) -> Dict[str, float]:
        return dict(self.__dict__)


def bias_ratio(fit: CameraFit, cam: Camera, target, n_intrinsics: int,
               mse_calib: Optional[float] = None) -> BiasRatio:
    """Bias Ratio of one camera. 'mse_calib' overrides the robust per-coordinate MSE (used only
    to show what a wrong convention does)."""
    res = fit.all_residuals
    n_obs = res.size                                  # coordinates
    n_params = n_intrinsics + 6 * len(fit.residuals)
    factor = dof_factor(n_obs, n_params)
    robust = robust_mse(res)
    mse = robust if mse_calib is None else mse_calib
    v_res, discarded = virtual_target_residuals(fit, cam, target)
    sigma_d2 = 4.0 * robust_mse(v_res)               # 8 obs / (8 - 6) = 4
    s_d2 = mse / factor
    eps2 = max(s_d2 - sigma_d2, 0.0)
    br = eps2 * factor / mse if mse > 0 else 0.0
    return BiasRatio(br=br, sqrt_br=float(np.sqrt(br)), eps_bias_px=float(np.sqrt(eps2)),
                     sigma_d_px=float(np.sqrt(sigma_d2)), mse_calib_robust_px2=robust,
                     mse_calib_plain_px2=mse_plain(res), n_obs=n_obs, n_params=n_params,
                     virtual_targets=len(v_res) // 4, virtual_discarded=discarded)


# --- RMS of a calibration (spec metrica-ajuste R1) -----------------------------------------

def stereo_fit_residuals(det: io.Detections, calib, min_corners: int = 48):
    """Residuals of both cameras with one pose per view fitted to both images through R, T
    (intrinsics and R, T fixed): the poses the stereo optimisation ends with."""
    from scipy.optimize import least_squares
    cam_l, cam_r = left_camera(calib), right_camera(calib)
    out_l, out_r = [], []
    for f in det.frames:
        obj, il, ir, _ = io.common_correspondences(f, det.target)
        if len(obj) < min_corners:
            continue
        rv0, tv0 = pose(obj, il, cam_l)

        def fun(x):
            rvr, tvr = right_pose(x[:3], x[3:], calib.R, calib.T)
            return np.concatenate([residuals(obj, il, x[:3], x[3:], cam_l).ravel(),
                                   residuals(obj, ir, rvr, tvr, cam_r).ravel()])

        x = least_squares(fun, np.concatenate([rv0, tv0]), xtol=1e-12, ftol=1e-12, gtol=1e-12).x
        r = fun(x).reshape(2, -1, 2)
        out_l.append(r[0])
        out_r.append(r[1])
    return out_l, out_r


def rms_report(det: io.Detections, calib, max_rms_px: float = 0.5, points: str = "common") -> dict:
    """Per-corner RMS (OpenCV convention) of a calibration on a capture, with the filter."""
    fit_l, fit_r = stereo_fit_residuals(det, calib)
    rl = np.concatenate(fit_l) if fit_l else np.zeros((0, 2))
    rr = np.concatenate(fit_r) if fit_r else np.zeros((0, 2))
    free_l = camera_fit(det, left_camera(calib), "left", points).all_residuals
    free_r = camera_fit(det, right_camera(calib), "right", points).all_residuals
    rms = {
        "stereo": float(np.sqrt((np.sum(rl ** 2) + np.sum(rr ** 2)) / (len(rl) + len(rr)))),
        "stereo_fit_left": rms_per_corner(rl), "stereo_fit_right": rms_per_corner(rr),
        "free_pose_left": rms_per_corner(free_l), "free_pose_right": rms_per_corner(free_r),
    }
    checked = ("stereo", "stereo_fit_left", "stereo_fit_right")
    return {
        "rms_convention": "per_corner_2d",
        "rms_px": rms,
        "mse_per_coordinate_px2": {k: v * v / 2 for k, v in rms.items()},
        "filter": {"max_rms_px": max_rms_px, "checked": list(checked),
                   "passed": all(rms[k] <= max_rms_px for k in checked)},
        "views": len(fit_l),
    }


# --- Residual map per image region (spec metrica-ajuste R2) --------------------------------

def residual_map(points: np.ndarray, res: np.ndarray, image_size, grid=(8, 8)) -> dict:
    """Per cell of a cols x rows grid: corner count, RMS and the mean signed residual in x and y.
    Empty cells are NaN (not 0). A corner on a cell border goes to the cell on its right/below,
    the last row and column included."""
    w, h = image_size
    cols, rows = grid
    pts = np.asarray(points, np.float64).reshape(-1, 2)
    res = np.asarray(res, np.float64).reshape(-1, 2)
    cx = np.clip((pts[:, 0] / w * cols).astype(int), 0, cols - 1)
    cy = np.clip((pts[:, 1] / h * rows).astype(int), 0, rows - 1)
    count = np.zeros((rows, cols), int)
    rms = np.full((rows, cols), np.nan)
    mean_x = np.full((rows, cols), np.nan)
    mean_y = np.full((rows, cols), np.nan)
    for r in range(rows):
        for c in range(cols):
            sel = (cy == r) & (cx == c)
            count[r, c] = int(np.sum(sel))
            if count[r, c]:
                rms[r, c] = rms_per_corner(res[sel])
                mean_x[r, c] = float(np.mean(res[sel, 0]))
                mean_y[r, c] = float(np.mean(res[sel, 1]))
    return {"grid": [cols, rows], "count": count, "rms_px": rms, "mean_x_px": mean_x, "mean_y_px": mean_y}


def plot_residual_map(m: dict, title: str, path: str) -> None:
    import matplotlib
    matplotlib.use("Agg")
    import matplotlib.pyplot as plt
    fig, axes = plt.subplots(1, 2, figsize=(12, 4))
    im = axes[0].imshow(m["rms_px"], cmap="viridis")
    axes[0].set_title("RMS per corner [px]")
    fig.colorbar(im, ax=axes[0])
    rows, cols = m["rms_px"].shape
    yy, xx = np.mgrid[0:rows, 0:cols]
    axes[1].quiver(xx, yy, np.nan_to_num(m["mean_x_px"]), -np.nan_to_num(m["mean_y_px"]),
                   angles="xy", scale_units="xy")
    axes[1].set_xlim(-0.5, cols - 0.5)
    axes[1].set_ylim(rows - 0.5, -0.5)
    axes[1].set_title("mean signed residual (image y down)")
    for ax in axes:
        ax.set_xlabel("cell column")
        ax.set_ylabel("cell row")
    fig.suptitle(title)
    fig.tight_layout()
    fig.savefig(path, dpi=100)
    plt.close(fig)
