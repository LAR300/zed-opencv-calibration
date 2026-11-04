"""M2, mapping error between calibrations and the empirical EME (spec erro-mapeamento;
Hagemann et al. 2022, referencias.md A.4).

Mapping error A -> B for one camera: a pixel grid is unprojected with A (undistortPointsIter,
tight criteria), each unprojection is checked by projecting back with A, and the rays are
projected with B. The rotation variant also minimises over a 3-parameter rotation of the rays
(no translation), which removes what the extrinsics would compensate (Hagemann's K).
Conventions: 'rms_per_point_px' = sqrt(mean |du|^2); 'k_per_coord_px2' = mean |du|^2 / 2.
"""
from dataclasses import dataclass, replace
from typing import Dict, List, Optional, Sequence

import cv2
import numpy as np
from scipy.optimize import least_squares

from .calib_file import StereoCalibration
from .residuals import Camera, left_camera, project, right_camera

CRITERIA = (cv2.TERM_CRITERIA_COUNT | cv2.TERM_CRITERIA_EPS, 100, 1e-12)


def pixel_grid(image_size, step: float = 20.0) -> np.ndarray:
    """Grid every 'step' px covering the whole image, the last row and column included."""
    w, h = image_size
    xs = np.unique(np.append(np.arange(0.0, w, step), w - 1.0))
    ys = np.unique(np.append(np.arange(0.0, h, step), h - 1.0))
    xx, yy = np.meshgrid(xs, ys)
    return np.column_stack([xx.ravel(), yy.ravel()])


def unproject(pixels: np.ndarray, cam: Camera, criteria=CRITERIA) -> np.ndarray:
    """Normalised rays (x, y) of the pixels."""
    src = np.asarray(pixels, np.float64).reshape(-1, 1, 2)
    if cam.fisheye:
        und = cv2.fisheye.undistortPoints(src, cam.K, cam.D)
    else:
        und = cv2.undistortPointsIter(src, cam.K, cam.D, None, None, criteria)
    return und.reshape(-1, 2)


def project_rays(rays: np.ndarray, cam: Camera, rvec=np.zeros(3)) -> np.ndarray:
    obj = np.column_stack([rays, np.ones(len(rays))])
    return project(obj, rvec, np.zeros(3), cam)


@dataclass
class MappingError:
    rms_per_point_px: float
    max_px: float
    k_per_coord_px2: float
    errors_px: np.ndarray        # per grid point (NaN where excluded)
    grid: np.ndarray
    excluded: int
    rotation_deg: Optional[np.ndarray] = None

    def summary(self) -> Dict:
        return {"rms_per_point_px": self.rms_per_point_px, "max_px": self.max_px,
                "k_per_coord_px2": self.k_per_coord_px2, "excluded_points": self.excluded,
                "rotation_deg": None if self.rotation_deg is None else self.rotation_deg.tolist()}


def coverage_mask(grid: np.ndarray, corners: np.ndarray, max_distance_px: float = 40.0) -> np.ndarray:
    """Grid points with a detected corner within 'max_distance_px' (default: two grid steps).
    Elsewhere a calibration only extrapolates, and the mapping error there measures the
    extrapolation rather than the calibration (measured: spec erro-mapeamento, R1)."""
    from scipy.spatial import cKDTree
    tree = cKDTree(np.asarray(corners, float).reshape(-1, 2))
    distance, _ = tree.query(np.asarray(grid, float), k=1)
    return distance <= max_distance_px


def mapping_error(a: Camera, b: Camera, image_size, step: float = 20.0, rotation: bool = False,
                  roundtrip_tol_px: float = 1e-3, region: Optional[np.ndarray] = None) -> MappingError:
    """'region': optional boolean mask over pixel_grid(image_size, step), e.g. coverage_mask."""
    grid = pixel_grid(image_size, step)
    rays = unproject(grid, a)
    back = project_rays(rays, a)
    ok = np.all(np.isfinite(back), axis=1) & (np.linalg.norm(back - grid, axis=1) <= roundtrip_tol_px)
    if region is not None:
        ok &= region
    rvec = np.zeros(3)
    if rotation:
        def fun(r):
            return (project_rays(rays[ok], b, r) - grid[ok]).ravel()
        rvec = least_squares(fun, np.zeros(3), xtol=1e-14, ftol=1e-14, gtol=1e-14).x
    du = project_rays(rays[ok], b, rvec) - grid[ok]
    sq = np.sum(du ** 2, axis=1)
    errors = np.full(len(grid), np.nan)
    errors[ok] = np.sqrt(sq)
    return MappingError(rms_per_point_px=float(np.sqrt(np.mean(sq))), max_px=float(np.sqrt(np.max(sq))),
                        k_per_coord_px2=float(np.mean(sq) / 2.0), errors_px=errors, grid=grid,
                        excluded=int(np.sum(~ok)),
                        rotation_deg=np.degrees(rvec) if rotation else None)


# --- The reference ("mean") calibration of a condition (R5) ---------------------------------

def _rational_from(params) -> Camera:
    fx, fy, cx, cy = params[:4]
    return Camera(np.array([[fx, 0, cx], [0, fy, cy], [0, 0, 1.0]]), np.array(params[4:]))


def _params(cam: Camera) -> np.ndarray:
    return np.concatenate([[cam.K[0, 0], cam.K[1, 1], cam.K[0, 2], cam.K[1, 2]], cam.D])


def _mean_camera_in_image_space(cams: Sequence[Camera], image_size, step) -> (Camera, float):
    """Fit a camera of the same model to the mean of the pixels every camera projects a common
    ray grid to. Returns the camera and the RMS of the fit (px)."""
    start = np.mean([_params(c) for c in cams], axis=0)
    rays = unproject(pixel_grid(image_size, step), _rational_from(start))
    target = np.mean([project_rays(rays, c) for c in cams], axis=0)
    ok = np.all(np.isfinite(target), axis=1)
    def fun(p):
        return (project_rays(rays[ok], _rational_from(p)) - target[ok]).ravel()
    sol = least_squares(fun, start, xtol=1e-14, ftol=1e-14, gtol=1e-14)
    res = fun(sol.x).reshape(-1, 2)
    return _rational_from(sol.x), float(np.sqrt(np.mean(np.sum(res ** 2, axis=1))))


def _mean_rotation(Rs) -> np.ndarray:
    """Chordal mean of rotation matrices (SVD projection of the mean matrix)."""
    U, _, Vt = np.linalg.svd(np.mean(Rs, axis=0))
    R = U @ Vt
    if np.linalg.det(R) < 0:
        U[:, -1] *= -1
        R = U @ Vt
    return R


def reference_calibration(calibs: Sequence[StereoCalibration], step: float = 20.0):
    """The reference of a condition, in image space; also returns the fit residuals and the
    simple mean of the parameters, for comparison."""
    if any(c.fisheye for c in calibs):
        raise NotImplementedError("the image-space reference is implemented for rational/radtan models")
    size = calibs[0].image_size
    left, fit_l = _mean_camera_in_image_space([left_camera(c) for c in calibs], size, step)
    right, fit_r = _mean_camera_in_image_space([right_camera(c) for c in calibs], size, step)
    R = _mean_rotation([c.R for c in calibs])
    T = np.mean([c.T for c in calibs], axis=0)
    ref = StereoCalibration(image_size=size, model=calibs[0].model, K_left=left.K, D_left=left.D,
                            K_right=right.K, D_right=right.D, R=R, T=T)
    simple = replace(ref, K_left=np.mean([c.K_left for c in calibs], axis=0),
                     D_left=np.mean([c.D_left for c in calibs], axis=0),
                     K_right=np.mean([c.K_right for c in calibs], axis=0),
                     D_right=np.mean([c.D_right for c in calibs], axis=0))
    return ref, simple, {"fit_rms_left_px": fit_l, "fit_rms_right_px": fit_r}


# --- Empirical EME and the pair matrix (R6, R7) -------------------------------------------

def empirical_eme(calibs: Sequence[StereoCalibration], reference: StereoCalibration, side: str,
                  step: float = 20.0, rotation: bool = False,
                  region: Optional[np.ndarray] = None) -> Dict:
    """Mapping error of every calibration to the reference, and the empirical EME: the RMS of
    those errors."""
    pick = left_camera if side == "left" else right_camera
    errors = [mapping_error(pick(c), pick(reference), c.image_size, step, rotation,
                            region=region).rms_per_point_px for c in calibs]
    return {"errors_px": errors, "eme_px": float(np.sqrt(np.mean(np.square(errors))))}


def pair_matrix(calibs: Sequence[StereoCalibration], side: str, step: float = 20.0,
                rotation: bool = False, region: Optional[np.ndarray] = None) -> np.ndarray:
    """Mapping errors between every pair (A -> B), n x n; the diagonal is 0."""
    pick = left_camera if side == "left" else right_camera
    n = len(calibs)
    m = np.zeros((n, n))
    for i in range(n):
        for j in range(n):
            if i != j:
                m[i, j] = mapping_error(pick(calibs[i]), pick(calibs[j]), calibs[i].image_size,
                                        step, rotation, region=region).rms_per_point_px
    return m


def percentile_of(value: float, distribution: Sequence[float]) -> float:
    """Percentage of the distribution strictly below the value (ties count half)."""
    d = np.asarray(distribution, float)
    return float(100.0 * (np.sum(d < value) + 0.5 * np.sum(d == value)) / len(d))
