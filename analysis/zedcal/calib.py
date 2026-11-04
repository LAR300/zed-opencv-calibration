"""Offline calibration of a capture, with other models and variants (spec nucleo-offline R4-R8).

Variants:
- mono_points: "all" (every tag of each camera) or "common" (only the tags seen by both, as the
  C++ tool does);
- stereo: "joint" (intrinsics refined with R, T, as the C++) or "fixed" (CALIB_FIX_INTRINSIC);
- rejection: "none" or "cpp" (per view, threshold max(factor x median, floor), twice, as the
  C++ calibrate()).
cpp_compatible() gives the variant that reproduces calib_cpp_<model>.yml from the same
detections. Points are float32, as in the C++ (cv::Point3f / cv::Point2f).
"""
import json
import os
import subprocess
from dataclasses import asdict, dataclass, field, replace
from typing import Dict, List, Optional, Tuple

import cv2
import numpy as np

from . import io
from .calib_file import StereoCalibration, write_calibration
from .residuals import Camera, pose, residuals, right_pose, rms_per_corner

MIN_VIEWS = 20        # MIN_IMAGE of the C++ tool
MIN_TAGS = 12         # DEFAULT_MIN_TAGS, DEFAULT_MIN_TAG_ROWS / COLS of the C++ tool
MIN_ROWS = 2
MIN_COLS = 2
STD_DEV_NAMES = ["fx", "fy", "cx", "cy", "k1", "k2", "p1", "p2", "k3", "k4", "k5", "k6",
                 "s1", "s2", "s3", "s4", "tau_x", "tau_y"]


@dataclass
class CalibrationOptions:
    model: str = "rational8"          # rational8 | radtan5 | fisheye4
    mono_points: str = "all"          # all | common
    stereo: str = "joint"             # joint | fixed
    rejection: str = "cpp"            # none | cpp
    outlier_factor: float = 2.5
    outlier_floor_px: float = 0.5
    prior: Optional[StereoCalibration] = None

    def variant(self) -> str:
        return "%s_%s_%s" % (self.mono_points, self.stereo, self.rejection)


def cpp_compatible(model: str = "rational8", prior: Optional[StereoCalibration] = None):
    return CalibrationOptions(model=model, mono_points="common", stereo="joint",
                              rejection="cpp", prior=prior)


@dataclass
class View:
    """One input pair: correspondences used for each camera and for the stereo fit."""
    pose_id: str
    n: int
    obj_left: np.ndarray
    img_left: np.ndarray
    obj_right: np.ndarray
    img_right: np.ndarray
    obj_common: np.ndarray
    img_common_left: np.ndarray
    img_common_right: np.ndarray
    mono_ok: Tuple[bool, bool]
    stereo_ok: bool


@dataclass
class CalibrationResult:
    calibration: StereoCalibration
    report: dict = field(default_factory=dict)


def meets_minimum(ids, target, min_tags=MIN_TAGS, min_rows=MIN_ROWS, min_cols=MIN_COLS) -> bool:
    if ids is None:  # checkerboard: a complete board
        return True
    rows = {i // target.cols for i in ids}
    cols = {i % target.cols for i in ids}
    return len(ids) >= min_tags and len(rows) >= min_rows and len(cols) >= min_cols


def _f32(a, cols):
    return np.ascontiguousarray(np.asarray(a, np.float32).reshape(-1, 1, cols))


def build_views(det: io.Detections) -> List[View]:
    views = []
    for f in det.frames:
        try:
            ol, il, ids_l = io.mono_correspondences(f, "left", det.target)
            ok_l = meets_minimum(ids_l, det.target)
        except ValueError:
            ol, il, ok_l = np.zeros((0, 3)), np.zeros((0, 2)), False
        try:
            orr, ir, ids_r = io.mono_correspondences(f, "right", det.target)
            ok_r = meets_minimum(ids_r, det.target)
        except ValueError:
            orr, ir, ok_r = np.zeros((0, 3)), np.zeros((0, 2)), False
        try:
            oc, cl, cr, ids_c = io.common_correspondences(f, det.target)
            ok_c = meets_minimum(ids_c, det.target) and len(oc) > 0
        except ValueError:
            oc, cl, cr, ok_c = np.zeros((0, 3)), np.zeros((0, 2)), np.zeros((0, 2)), False
        views.append(View(f.pose_id, f.n, ol, il, orr, ir, oc, cl, cr, (ok_l, ok_r), ok_c))
    return views


def _trim(D, model):
    """OpenCV's Python binding may return 14 coefficients; the model keeps its own, the rest
    must be zero (the prism and tilt terms are not enabled)."""
    D = np.asarray(D, np.float64).ravel()
    n = {"rational8": 8, "radtan5": 5, "fisheye4": 4}[model]
    if np.any(D[n:] != 0):
        raise RuntimeError("unexpected non-zero distortion terms beyond the model")
    return D[:n].copy()


def _dist_init(model):
    return np.zeros(4 if model == "fisheye4" else 5 if model == "radtan5" else 8)


def _mono(obj, img, size, K, D, model, guess):
    """One camera. Returns rms, K, D, std devs (None for fisheye)."""
    K = np.array(K, np.float64)
    D = np.array(D, np.float64).reshape(1, -1)
    if model == "fisheye4":
        flags = cv2.fisheye.CALIB_RECOMPUTE_EXTRINSIC + cv2.fisheye.CALIB_FIX_SKEW
        if guess:
            flags += cv2.fisheye.CALIB_USE_INTRINSIC_GUESS
        rms, K, D, _, _ = cv2.fisheye.calibrate([_f32(o, 3) for o in obj], [_f32(i, 2) for i in img],
                                                size, K, D, flags=flags)
        return rms, K, D.ravel(), None
    flags = cv2.CALIB_USE_INTRINSIC_GUESS if guess else 0
    if model == "rational8":
        flags += cv2.CALIB_RATIONAL_MODEL
    rms, K, D, _, _, std_int, _, _ = cv2.calibrateCameraExtended(
        [_f32(o, 3) for o in obj], [_f32(i, 2) for i in img], size, K, D, flags=flags)
    return rms, K, _trim(D, model), std_int.ravel()


def _stereo(obj, img_l, img_r, size, c: StereoCalibration, mode):
    """Stereo fit. Returns rms, updated calibration, per-view errors (None for fisheye)."""
    O = [_f32(o, 3) for o in obj]
    L = [_f32(i, 2) for i in img_l]
    Rr = [_f32(i, 2) for i in img_r]
    if c.fisheye:
        # cv2.fisheye.stereoCalibrate fails in the Python binding of OpenCV 4.5.4 (assertion
        # "!fixedSize()" for every argument layout tried). The ZED 2i uses the rational model;
        # the fisheye model is the ZED X One's, calibrated by the C++ tool.
        raise NotImplementedError("the fisheye stereo fit is not available in the Python "
                                  "binding of OpenCV %s; use the C++ tool" % cv2.__version__)
        flags = cv2.fisheye.CALIB_CHECK_COND
        flags += cv2.fisheye.CALIB_FIX_INTRINSIC if mode == "fixed" else cv2.fisheye.CALIB_USE_INTRINSIC_GUESS
        out = cv2.fisheye.stereoCalibrate(O, L, Rr, c.K_left, c.D_left.reshape(1, -1), c.K_right,
                                          c.D_right.reshape(1, -1), size, flags=flags)
        rms, K1, D1, K2, D2, R, T = out[:7]
        per_view = None
    else:
        flags = cv2.CALIB_FIX_INTRINSIC if mode == "fixed" else cv2.CALIB_USE_INTRINSIC_GUESS
        if c.model == "rational8":
            flags += cv2.CALIB_RATIONAL_MODEL
        rms, K1, D1, K2, D2, R, T, _, _, per_view = cv2.stereoCalibrateExtended(
            O, L, Rr, c.K_left, c.D_left.reshape(1, -1), c.K_right, c.D_right.reshape(1, -1), size,
            np.eye(3), np.zeros((3, 1)), flags=flags)
    return rms, replace(c, K_left=K1, D_left=_trim(D1, c.model), K_right=K2, D_right=_trim(D2, c.model), R=R,
                        T=np.asarray(T, np.float64).ravel()), per_view


def _frame_error(obj, img_l, img_r, cam_l, cam_r, R=None, T=None):
    """Per-view RMS of both cameras together, as the C++ rejection computes it."""
    obj32 = _f32(obj, 3)
    ok, rv_l, tv_l = cv2.solvePnP(obj32, _f32(img_l, 2), cam_l.K, cam_l.D)
    if not ok:
        return None
    if R is None:
        ok, rv_r, tv_r = cv2.solvePnP(obj32, _f32(img_r, 2), cam_r.K, cam_r.D)
        if not ok:
            return None
    else:
        rv_r, tv_r = right_pose(rv_l, tv_l, R, T)
    pl, _ = cv2.projectPoints(obj32, rv_l, tv_l, cam_l.K, cam_l.D)
    pr, _ = cv2.projectPoints(obj32, np.asarray(rv_r).reshape(3, 1), np.asarray(tv_r).reshape(3, 1),
                              cam_r.K, cam_r.D)
    el = pl.reshape(-1, 2) - np.asarray(img_l, np.float32).reshape(-1, 2)
    er = pr.reshape(-1, 2) - np.asarray(img_r, np.float32).reshape(-1, 2)
    n = len(el)
    return float(np.sqrt((np.sum(el.astype(np.float64) ** 2) + np.sum(er.astype(np.float64) ** 2)) / (2 * n)))


def _outliers(errors: List[Tuple[float, int]], factor, floor):
    if not errors:
        return [], 0.0
    ordered = sorted(errors)
    threshold = max(factor * ordered[len(ordered) // 2][0], floor)
    return [i for e, i in errors if e > threshold], threshold


def _git_commit() -> str:
    try:
        return subprocess.check_output(["git", "-C", os.path.dirname(__file__), "rev-parse", "HEAD"],
                                       stderr=subprocess.DEVNULL, text=True).strip()
    except (OSError, subprocess.CalledProcessError):
        return "unknown"


def calibrate(det: io.Detections, options: CalibrationOptions = CalibrationOptions()) -> CalibrationResult:
    o = options
    if o.model not in ("rational8", "radtan5", "fisheye4"):
        raise ValueError("unknown model %r" % o.model)
    size = tuple(det.image_size)
    views = build_views(det)
    stereo_idx = [i for i, v in enumerate(views) if v.stereo_ok]
    if len(stereo_idx) < MIN_VIEWS:
        raise ValueError("%d views with enough common tags, at least %d needed" % (len(stereo_idx), MIN_VIEWS))
    removed: Dict[int, Tuple[str, float]] = {}

    def mono_views(side):
        if o.mono_points == "common":
            idx = [i for i in stereo_idx if i not in removed]
            obj = [views[i].obj_common for i in idx]
            img = [views[i].img_common_left if side == "left" else views[i].img_common_right for i in idx]
        else:
            k = 0 if side == "left" else 1
            idx = [i for i, v in enumerate(views) if v.mono_ok[k] and i not in removed]
            obj = [views[i].obj_left if side == "left" else views[i].obj_right for i in idx]
            img = [views[i].img_left if side == "left" else views[i].img_right for i in idx]
        return idx, obj, img

    prior = o.prior
    guess = prior is not None
    K0l, D0l = (prior.K_left, prior.D_left) if guess else (np.eye(3), _dist_init(o.model))
    K0r, D0r = (prior.K_right, prior.D_right) if guess else (np.eye(3), _dist_init(o.model))

    def run_mono(guess_now, Kl, Dl, Kr, Dr):
        _, ol, il = mono_views("left")
        _, orr, ir = mono_views("right")
        rl, Kl, Dl, sl = _mono(ol, il, size, Kl, Dl, o.model, guess_now)
        rr, Kr, Dr, sr = _mono(orr, ir, size, Kr, Dr, o.model, guess_now)
        return (rl, Kl, Dl, sl), (rr, Kr, Dr, sr)

    (rms_l, Kl, Dl, std_l), (rms_r, Kr, Dr, std_r) = run_mono(guess, K0l, D0l, K0r, D0r)
    calib = StereoCalibration(image_size=size, model=o.model, K_left=Kl, D_left=Dl, K_right=Kr,
                              D_right=Dr, R=np.eye(3), T=np.zeros(3))
    radtan = o.model != "fisheye4"

    def active():
        return [i for i in stereo_idx if i not in removed]

    def reject(stage, use_stereo):
        nonlocal calib, rms_l, rms_r, std_l, std_r
        idx = active()
        errors = []
        cl = Camera(calib.K_left, calib.D_left)
        cr = Camera(calib.K_right, calib.D_right)
        for i in idx:
            v = views[i]
            e = _frame_error(v.obj_common, v.img_common_left, v.img_common_right, cl, cr,
                             calib.R if use_stereo else None, calib.T if use_stereo else None)
            if e is not None:
                errors.append((e, i))
        out, _ = _outliers(errors, o.outlier_factor, o.outlier_floor_px)
        if out and len(idx) - len(out) >= MIN_VIEWS:
            for e, i in errors:
                if i in out:
                    removed[i] = (stage, e)
            (rms_l, Kl2, Dl2, std_l), (rms_r, Kr2, Dr2, std_r) = run_mono(
                True, calib.K_left, calib.D_left, calib.K_right, calib.D_right)
            calib = replace(calib, K_left=Kl2, D_left=Dl2, K_right=Kr2, D_right=Dr2)
            return True
        return False

    if o.rejection == "cpp" and radtan:
        reject("mono", False)

    def stereo_fit():
        idx = active()
        return _stereo([views[i].obj_common for i in idx], [views[i].img_common_left for i in idx],
                       [views[i].img_common_right for i in idx], size, calib, o.stereo)

    stereo_rms, calib, per_view = stereo_fit()
    if o.rejection == "cpp" and radtan and reject("stereo", True):
        stereo_rms, calib, per_view = stereo_fit()

    report = _report(o, det, views, stereo_idx, active(), removed, calib, stereo_rms, per_view,
                     (rms_l, rms_r), (std_l, std_r), guess)
    return CalibrationResult(calibration=calib, report=report)


def _report(o, det, views, stereo_idx, used, removed, calib, stereo_rms, per_view, mono_rms, std,
            guess) -> dict:
    cam_l = Camera(calib.K_left, calib.D_left, calib.fisheye)
    cam_r = Camera(calib.K_right, calib.D_right, calib.fisheye)
    view_reports, free_l, free_r, fit_l, fit_r, corners = [], [], [], [], [], []
    for k, i in enumerate(used):
        v = views[i]
        rv, tv = pose(v.obj_common, v.img_common_left, cam_l)
        el = rms_per_corner(residuals(v.obj_common, v.img_common_left, rv, tv, cam_l))
        rv2, tv2 = pose(v.obj_common, v.img_common_right, cam_r)
        er = rms_per_corner(residuals(v.obj_common, v.img_common_right, rv2, tv2, cam_r))
        free_l.append(el)
        free_r.append(er)
        corners.append(len(v.obj_common))
        if per_view is not None:
            fit_l.append(float(per_view[k, 0]))
            fit_r.append(float(per_view[k, 1]))

    def agg(values):
        if not values:
            return None
        c = np.array(corners[:len(values)], float)
        return float(np.sqrt(np.sum(c * np.square(values)) / np.sum(c)))

    for i, v in enumerate(views):
        entry = {"pose_id": v.pose_id, "n": v.n, "corners_left": len(v.obj_left),
                 "corners_right": len(v.obj_right), "corners_common": len(v.obj_common),
                 "used_in_stereo": i in used, "removed": None}
        if i in removed:
            entry["removed"] = {"stage": removed[i][0], "error": removed[i][1]}
        if per_view is not None and i in used:
            k = used.index(i)
            entry["stereo_error_left"] = float(per_view[k, 0])
            entry["stereo_error_right"] = float(per_view[k, 1])
        view_reports.append(entry)

    rms = {"stereo": float(stereo_rms), "stereo_fit_left": agg(fit_l), "stereo_fit_right": agg(fit_r),
           "free_pose_left": agg(free_l), "free_pose_right": agg(free_r),
           "mono_stage_left": float(mono_rms[0]), "mono_stage_right": float(mono_rms[1])}
    opts = asdict(o)
    opts["prior"] = "given" if guess else None
    return {
        "schema_version": 1,
        "model": o.model,
        "variant": o.variant(),
        "options": opts,
        "image_size": list(det.image_size),
        "pairs_input": len(views),
        "pairs_stereo_valid": len(stereo_idx),
        "pairs_used": len(used),
        "rms_convention": "per_corner_2d",
        "rms_px": rms,
        "mse_per_coordinate_px2": {k: (None if v is None else v * v / 2.0) for k, v in rms.items()},
        "std_dev_mono_last": {
            "names": STD_DEV_NAMES,
            "left": None if std[0] is None else [float(x) for x in std[0]],
            "right": None if std[1] is None else [float(x) for x in std[1]],
        },
        "std_dev_stereo": None,
        "baseline_mm": calib.baseline_mm,
        "versions": {"numpy": np.__version__, "opencv": cv2.__version__},
        "code": {"commit": _git_commit()},
        "views": view_reports,
    }


def save(result: CalibrationResult, output_dir: str, name: Optional[str] = None) -> Tuple[str, str]:
    """calib_<model>_<variant>.yml and .report.json in output_dir; never overwrites."""
    name = name or "calib_%s_%s" % (result.calibration.model, result.report["variant"])
    yml = os.path.join(output_dir, name + ".yml")
    rep = os.path.join(output_dir, name + ".report.json")
    if os.path.exists(rep):
        raise FileExistsError("%s already exists and is never overwritten" % rep)
    write_calibration(yml, result.calibration)
    with open(rep, "w") as f:
        json.dump(result.report, f, indent=2)
    return yml, rep
