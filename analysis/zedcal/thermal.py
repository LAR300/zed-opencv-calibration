"""Warm-up until the thermal steady state (spec aquecimento-termico, R2 and R3).

A fixed target in front of a fixed camera, one pair every interval from a cold start. With a
FIXED calibration (F) every pair gives:
- the target pose in the left camera (PnP): translation (mm) and the rotation relative to the
  first pair (Rodrigues vector, deg);
- the stereo transfer: the left pose with R, T projected on the right camera, against the right
  corners: its RMS and its mean signed residual in x and y (an x drift is a disparity drift);
- the mean signed residual in x and y of the left camera in its own pose.
A drift of the lens or of the sensor mount moves these series; in the steady state they are flat
within the noise.

Steady-state criterion: sigma is the noise of one pair, from the consecutive differences. A window (default 5 min) is settled when its mean is within
factor * sigma of the mean of the LAST window, taken as the steady value. The steady instant
is the centre of the first window from which every later window is settled, provided the
line fitted to the end of the record (its last third, at least two windows) changes by no more
than factor * sigma: else a drift that lasts until the end would look settled against the last
window, and the record is reported as not settled. With ~10 pairs per
window the mean of a window has a noise of ~0.3 sigma, so the default factor 2 rarely fails by
noise alone.
"""
from dataclasses import dataclass
from typing import Dict, List, Optional

import cv2
import numpy as np

from . import io
from .residuals import left_camera, pose, residuals, right_camera, right_pose, rms_per_corner

SERIES = ("tx_mm", "ty_mm", "tz_mm", "rx_deg", "ry_deg", "rz_deg",
          "transfer_rms_px", "transfer_dx_px", "transfer_dy_px", "left_dx_px", "left_dy_px")


def series(calib, det: io.Detections, meta: Optional[dict], min_corners: int = 24) -> Dict:
    """Per pair (in time order): time (min since the first), temperatures and the SERIES."""
    cam_l, cam_r = left_camera(calib), right_camera(calib)
    info = {(f["pose_id"], f["n"]): f for f in (meta or {}).get("frames", [])}
    rows = []
    for f in det.frames:
        m = info.get((f.pose_id, f.n), {})
        ts = m.get("timestamp_ns")
        obj_l, img_l, _ = io.mono_correspondences(f, "left", det.target)
        obj_r, img_r, _ = io.mono_correspondences(f, "right", det.target)
        if len(obj_l) < min_corners or len(obj_r) < min_corners:
            continue
        rv, tv = pose(obj_l, img_l, cam_l)
        res_l = residuals(obj_l, img_l, rv, tv, cam_l)
        rvr, tvr = right_pose(rv, tv, calib.R, calib.T)
        res_r = residuals(obj_r, img_r, rvr, tvr, cam_r)
        Rm = cv2.Rodrigues(np.asarray(rv, float).reshape(3, 1))[0]
        temps = m.get("temperature_c") or {}
        rows.append({"n": f.n, "timestamp_ns": int(ts) if ts is not None else None,
                     "temperature_c": {k: temps.get(k) for k in ("onboard_left", "onboard_right", "imu")},
                     "tx_mm": float(tv[0]), "ty_mm": float(tv[1]), "tz_mm": float(tv[2]),
                     "R": Rm,
                     "transfer_rms_px": rms_per_corner(res_r),
                     "transfer_dx_px": float(np.mean(res_r[:, 0])), "transfer_dy_px": float(np.mean(res_r[:, 1])),
                     "left_dx_px": float(np.mean(res_l[:, 0])), "left_dy_px": float(np.mean(res_l[:, 1]))})
    if not rows:
        raise ValueError("no pair with the target seen by both cameras")
    if all(r["timestamp_ns"] is not None for r in rows):
        rows.sort(key=lambda r: r["timestamp_ns"])
        t0 = rows[0]["timestamp_ns"]
        t = [(r["timestamp_ns"] - t0) / 6e10 for r in rows]
        timing = "timestamp"
    else:
        rows.sort(key=lambda r: r["n"])
        t = [float(r["n"]) for r in rows]
        timing = "pair index (no timestamps)"
    # angles: rotation of every pair relative to the first, as a Rodrigues vector in degrees
    # (the absolute Rodrigues vector of a board facing the camera is near pi and flips sign)
    R0 = rows[0]["R"]
    for r in rows:
        ang = np.degrees(cv2.Rodrigues(r.pop("R") @ R0.T)[0].ravel())
        r.update({"rx_deg": float(ang[0]), "ry_deg": float(ang[1]), "rz_deg": float(ang[2])})
    has_temp = any(v is not None for r in rows for v in r["temperature_c"].values())
    return {"time_min": t, "timing": timing, "has_temperature": has_temp, "rows": rows}


def noise_sigma(q: np.ndarray) -> float:
    """Noise of one pair: RMS of the consecutive differences, trimmed at 5 MAD-sigma (as the
    robust MSE of the M1), over sqrt 2. More efficient than the MAD alone, still robust to a jump."""
    d = np.diff(np.asarray(q, float))
    d = d - np.median(d)
    mad = 1.4826 * np.median(np.abs(d))
    keep = np.abs(d) <= 5 * mad if mad > 0 else np.ones(d.size, bool)
    return float(np.sqrt(np.mean(d[keep] ** 2)) / np.sqrt(2.0))


@dataclass
class Steady:
    time_min: Optional[float]      # None: not settled before the end
    sigma: float
    final_value: float
    windows: List[Dict]


def steady_time(t, q, window_min: float = 5.0, factor: float = 2.0) -> Steady:
    t, q = np.asarray(t, float), np.asarray(q, float)
    sigma = noise_sigma(q)
    end = t[-1]
    last = q[t >= end - window_min]
    final = float(np.mean(last))
    starts = t[t <= end - window_min]
    wins = []
    for s in starts:
        sel = (t >= s) & (t < s + window_min)
        dev = float(np.mean(q[sel]) - final)
        wins.append({"start_min": float(s), "deviation": dev, "settled": bool(abs(dev) <= factor * sigma)})
    time = None
    for i in range(len(wins)):
        if all(w["settled"] for w in wins[i:]):
            time = wins[i]["start_min"] + window_min / 2
            break
    if time is not None:
        # a drift that goes on until the end also ends near the last window: the line fitted to
        # the end of the record (its last third, at least two windows) must change by no more
        # than factor * sigma
        sel = t >= end - max(2 * window_min, (end - t[0]) / 3)
        if sel.sum() >= 3:
            slope = np.polyfit(t[sel], q[sel], 1)[0]
            if abs(slope * (t[sel][-1] - t[sel][0])) > factor * sigma:
                time = None
    return Steady(time, sigma, final, wins)


def analyse(s: Dict, window_min: float = 5.0, factor: float = 2.0) -> Dict:
    out = {"timing": s["timing"], "has_temperature": s["has_temperature"], "series": {}}
    times = []
    for name in SERIES:
        q = [r[name] for r in s["rows"]]
        st = steady_time(s["time_min"], q, window_min, factor)
        out["series"][name] = {"steady_min": st.time_min, "sigma": st.sigma, "final": st.final_value,
                               "total_change": float(q[0] - st.final_value)}
        times.append(st.time_min)
    settled = [x for x in times if x is not None]
    out["not_settled"] = [n for n, x in zip(SERIES, times) if x is None]
    out["steady_min"] = None if out["not_settled"] else float(max(settled))
    if out["steady_min"] is not None and s["has_temperature"]:
        k = int(np.searchsorted(s["time_min"], out["steady_min"]))
        k = min(k, len(s["rows"]) - 1)
        out["temperature_at_steady_c"] = s["rows"][k]["temperature_c"]
    return out


def proposed_x(steady_minutes: List[Optional[float]]) -> Optional[int]:
    """R4: X is the largest steady time of the cold starts, rounded up to the minute."""
    if not steady_minutes or any(m is None for m in steady_minutes):
        return None
    return int(np.ceil(max(steady_minutes)))
