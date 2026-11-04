"""R7, metric test at known distances (spec validacao-metrica, R5).

The session follows config/roteiro_metrico_v1.yaml: pose ids MET-<distance cm>-<position>-<n>,
position MC (centre) or ML, MR, TC, BC (edges). For every calibration:
  (a) the M4 metric error (tag sides, pitch, spans) per distance and position;
  (b) the distance of the target plane, triangulated, against the measured one. The measuring
      reference point sits an unknown d0 in front of the optical centre, the same at every
      distance: measured = Z_plane - d0. d0 is the mean of Z_plane - measured; what is left per
      distance is the error after d0. A scale error of the calibration shows up as a residual
      proportional to the distance, and as the slope of the two-parameter fit (information);
  (c) the M4 epipolar error per distance.
"""
import re
from typing import Dict, Optional

import numpy as np

from . import io, m4
from .target import Target

POSE_ID = re.compile(r"MET-(\d+)-(MC|ML|MR|TC|BC)-(\d+)$")


def parse_pose_id(pose_id: str):
    m = POSE_ID.match(pose_id)
    if not m:
        return None
    return int(m.group(1)), m.group(2)


def plane_distance(X: np.ndarray) -> float:
    """Perpendicular distance from the optical centre to the plane fitted to the corners."""
    c = X.mean(axis=0)
    normal = np.linalg.svd(X - c, full_matrices=False)[2][-1]
    return float(abs(normal @ c))


def measured_distances(det: io.Detections, meta: Optional[dict], by_label: Optional[Dict] = None) -> list:
    """The measured distance (mm) of every frame: the frame's measured_distance.value_mm in
    meta.json (matched by pose_id and n), else the value given per distance label (cm)."""
    from_meta = {}
    for f in (meta or {}).get("frames", []):
        md = f.get("measured_distance")
        if md and md.get("value_mm") is not None:
            from_meta[(f["pose_id"], f["n"])] = float(md["value_mm"])
    by_label = {int(k): float(v) for k, v in (by_label or {}).items()}
    out = []
    for f in det.frames:
        v = from_meta.get((f.pose_id, f.n))
        if v is None:
            parsed = parse_pose_id(f.pose_id)
            v = by_label.get(parsed[0]) if parsed else None
        out.append(v)
    return out


def analyse(calib, det: io.Detections, distances_mm: list, target: Optional[Target] = None,
            alpha: float = 0.0) -> Dict:
    ev = m4.evaluate(calib, det, target=target, alpha=alpha)
    rows = []
    for v, k in enumerate(ev.frame_index):
        parsed = parse_pose_id(det.frames[k].pose_id)
        if parsed is None or distances_mm[k] is None:
            continue
        rows.append({"view": v, "label_cm": parsed[0], "position": parsed[1],
                     "measured_mm": distances_mm[k], "plane_mm": plane_distance(ev.points_3d[v])})
    if not rows:
        raise ValueError("no MET-... view with a measured distance")
    plane = np.array([r["plane_mm"] for r in rows])
    meas = np.array([r["measured_mm"] for r in rows])
    d0 = float(np.mean(plane - meas))
    slope, intercept = np.polyfit(meas, plane, 1)
    out = {"d0_mm": d0, "two_parameter_fit": {"scale_error_pct": float(100 * (slope - 1)), "d0_mm": float(intercept)},
           "views": len(rows), "by_distance": []}
    for label in sorted({r["label_cm"] for r in rows}):
        sel = [r for r in rows if r["label_cm"] == label]
        res = np.array([r["plane_mm"] - d0 - r["measured_mm"] for r in sel])
        mm = np.array([r["measured_mm"] for r in sel])
        entry = {"label_cm": label, "views": len(sel), "measured_mm_mean": float(mm.mean()),
                 "residual_after_d0_mm": float(res.mean()), "residual_sd_mm": float(res.std(ddof=1)) if len(res) > 1 else None,
                 "relative_error_after_d0_pct": float(100 * np.mean(res / mm)), "positions": {}}
        views = [r["view"] for r in sel]
        e = np.concatenate([ev.epipolar[v] for v in views])
        entry["epipolar_px"] = {"rms": float(np.sqrt(np.mean(e ** 2))), "p90_abs": float(np.percentile(np.abs(e), 90))}
        for pos_group, names in (("centre", ("MC",)), ("edges", ("ML", "MR", "TC", "BC"))):
            pv = [r["view"] for r in sel if r["position"] in names]
            if not pv:
                continue
            g = {"views": len(pv)}
            for s in m4.METRIC_SETS:
                vals = [m for m, v in zip(ev.metric[s], ev.metric_view[s]) if v in pv]
                if vals:
                    v = np.concatenate(vals)
                    g[s + "_pct"] = {"mean": float(v.mean()), "rms": float(np.sqrt(np.mean(v ** 2))), "n": int(v.size)}
            entry["positions"][pos_group] = g
        out["by_distance"].append(entry)
    return out
