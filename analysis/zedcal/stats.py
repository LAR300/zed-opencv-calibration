"""Statistics per condition and between two conditions (spec analise-estatistica, brief R6).

- describe: mean, Student-t 95 % interval, CV (not variance), and where F falls.
- permutation_test: exact permutation of the calibration LABELS on a matrix of pairwise
  mapping errors (the 145 pairwise errors are not independent; permuting the calibrations keeps
  that dependence, as in Mantel / PERMANOVA tests).
- cross_application: a calibration on the captures of its own condition vs the other one.
- tost: two one-sided Welch t-tests, equivalence when the 90 % interval is inside +-margin.
- dispersion: Brown-Forsythe (Levene centred on the median) and the one-sided F ratio test.
- holm: Holm-adjusted p values, reported for information.
"""
from itertools import combinations
from math import comb
from typing import Dict, Optional, Sequence

import cv2
import numpy as np
from scipy import stats as st

# Quantities whose true value is near zero: their CV says nothing, the SD is reported instead.
NEAR_ZERO = ("ty_mm", "tz_mm", "rx_deg", "ry_deg", "rz_deg")


def calibration_quantities(c) -> Dict[str, float]:
    """The parameters of a stereo calibration that R1 tabulates. R as its Rodrigues vector in
    degrees (for these small rotations, about the three axes)."""
    r = np.degrees(cv2.Rodrigues(np.asarray(c.R, np.float64))[0].ravel())
    q = {}
    for side, K in (("left", c.K_left), ("right", c.K_right)):
        q.update({"fx_" + side: K[0, 0], "fy_" + side: K[1, 1], "cx_" + side: K[0, 2],
                  "cy_" + side: K[1, 2], "fy_over_fx_" + side: K[1, 1] / K[0, 0]})
    T = np.asarray(c.T, np.float64).ravel()
    q.update({"baseline_mm": float(np.linalg.norm(T)), "tx_mm": T[0], "ty_mm": T[1], "tz_mm": T[2],
              "rx_deg": r[0], "ry_deg": r[1], "rz_deg": r[2]})
    return {k: float(v) for k, v in q.items()}


def t_interval(values, level: float = 0.95):
    v = np.asarray(values, float)
    n = len(v)
    m = float(np.mean(v))
    half = float(st.t.ppf(0.5 + level / 2, n - 1) * np.std(v, ddof=1) / np.sqrt(n))
    return m - half, m + half


def describe(values: Sequence[float], name: str = "", factory: Optional[float] = None,
             near_zero: Optional[bool] = None) -> Dict:
    """R1/R2. near_zero: None marks the CV as not informative for the NEAR_ZERO names and
    whenever the 95 % interval contains 0."""
    v = np.asarray(values, float)
    lo, hi = t_interval(v)
    mean, sd = float(np.mean(v)), float(np.std(v, ddof=1))
    if near_zero is None:
        near_zero = name in NEAR_ZERO or lo <= 0.0 <= hi
    out = {"n": int(len(v)), "mean": mean, "sd": sd, "ci95": [lo, hi],
           "cv": None if near_zero else sd / abs(mean), "cv_informative": not near_zero}
    if factory is not None:
        out["factory"] = float(factory)
        out["factory_in_ci95"] = bool(lo <= factory <= hi)
        out["factory_percentile"] = float(100.0 * (np.sum(v < factory) + 0.5 * np.sum(v == factory)) / len(v))
    return out


def _assignments(n: int, n_a: int, symmetric: bool) -> np.ndarray:
    """Indicator rows of every choice of the n_a members of A. With symmetric and equal sizes,
    each division appears once (member 0 always in A)."""
    if symmetric and 2 * n_a == n:
        idx = [(0,) + c for c in combinations(range(1, n), n_a - 1)]
    else:
        idx = list(combinations(range(n), n_a))
    X = np.zeros((len(idx), n))
    rows = np.repeat(np.arange(len(idx)), n_a)
    X[rows, np.array(idx).ravel()] = 1.0
    return X


def _group_stats(X: np.ndarray, M: np.ndarray, n_a: int, n_b: int):
    total = np.sum(np.triu(M, 1))
    XM = X @ M
    within_a = np.sum(XM * X, axis=1) / 2.0
    cross = np.sum(XM * (1.0 - X), axis=1)
    within_b = total - within_a - cross
    return cross / (n_a * n_b), within_a / comb(n_a, 2), within_b / comb(n_b, 2)


def permutation_test(M: np.ndarray, labels: Sequence[str], a: str, b: str) -> Dict:
    """R3 on a matrix of pairwise mapping errors (symmetrised as (M + M^T) / 2).

    main: mean(A-B pairs) - mean(A-A pairs); symmetric: mean(A-B) - mean(A-A and B-B pairs).
    One-sided p (the observed statistic counts as one of the enumerated ones)."""
    M = np.asarray(M, float)
    M = (M + M.T) / 2.0
    np.fill_diagonal(M, 0.0)
    labels = list(labels)
    order = [i for i, l in enumerate(labels) if l == a] + [i for i, l in enumerate(labels) if l == b]
    if len(order) != len(labels):
        raise ValueError("labels other than %s and %s" % (a, b))
    M = M[np.ix_(order, order)]
    n_a, n_b = labels.count(a), labels.count(b)
    if n_a < 2 or n_b < 2:
        raise ValueError("each condition needs at least 2 calibrations")
    obs = np.zeros((1, n_a + n_b))
    obs[0, :n_a] = 1.0
    out = {}
    for variant, symmetric in (("main", False), ("symmetric", True)):
        X = _assignments(n_a + n_b, n_a, symmetric)
        cross, wa, wb = _group_stats(X, M, n_a, n_b)
        c0, wa0, wb0 = (v[0] for v in _group_stats(obs, M, n_a, n_b))
        if symmetric:
            pairs = comb(n_a, 2) + comb(n_b, 2)
            null = cross - (wa * comb(n_a, 2) + wb * comb(n_b, 2)) / pairs
            observed = c0 - (wa0 * comb(n_a, 2) + wb0 * comb(n_b, 2)) / pairs
        else:
            null, observed = cross - wa, c0 - wa0
        tol = 1e-12 * max(1.0, abs(observed))
        out[variant] = {"statistic": float(observed), "p_value": float(np.mean(null >= observed - tol)),
                        "assignments": int(len(null)), "null": null}
    out["mean_between_px"] = float(c0)
    out["mean_within_a_px"] = float(wa0)
    out["mean_within_b_px"] = float(wb0)
    return out


def cross_application(matrix: np.ndarray, rows: Sequence[str], cols: Sequence[str],
                      row_condition: Dict[str, str], col_condition: Dict[str, str]) -> Dict:
    """R4. For each calibration (row): mean over the captures of the OTHER condition minus the
    mean over the captures of its own (its own capture, the training one, left out). The
    calibrations are the units of the t interval."""
    matrix = np.asarray(matrix, float)
    per_cond = {}
    for i, r in enumerate(rows):
        c = row_condition.get(r)
        if c is None:
            continue
        own = [matrix[i, j] for j, k in enumerate(cols) if col_condition.get(k) == c and k != r]
        other = [matrix[i, j] for j, k in enumerate(cols) if col_condition.get(k) not in (None, c)]
        if own and other:
            per_cond.setdefault(c, []).append(float(np.mean(other) - np.mean(own)))
    out = {}
    for c, d in per_cond.items():
        e = {"n": len(d), "mean_difference": float(np.mean(d)), "differences": d}
        if len(d) > 1:
            e["ci95"] = list(t_interval(d))
        out[c] = e
    return out


def tost(a: Sequence[float], b: Sequence[float], margin: float, alpha: float = 0.05) -> Dict:
    """R5: Welch TOST on mu_b - mu_a with the margin +-margin."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    na, nb = len(a), len(b)
    va, vb = np.var(a, ddof=1) / na, np.var(b, ddof=1) / nb
    diff = float(np.mean(b) - np.mean(a))
    se = float(np.sqrt(va + vb))
    df = float((va + vb) ** 2 / (va ** 2 / (na - 1) + vb ** 2 / (nb - 1)))
    p_lower = float(st.t.sf((diff + margin) / se, df))   # H0: diff <= -margin
    p_upper = float(st.t.cdf((diff - margin) / se, df))  # H0: diff >= +margin
    half = float(st.t.ppf(1 - alpha, df) * se)
    ci = [diff - half, diff + half]
    return {"difference": diff, "se": se, "df": df, "margin": float(margin),
            "ci90" if alpha == 0.05 else "ci": ci, "p_value": max(p_lower, p_upper),
            "equivalent": bool(-margin < ci[0] and ci[1] < margin)}


def dispersion(a: Sequence[float], b: Sequence[float]) -> Dict:
    """R6: Brown-Forsythe and the one-sided F test (B more dispersed than A)."""
    a, b = np.asarray(a, float), np.asarray(b, float)
    bf = st.levene(a, b, center="median")
    f = float(np.var(b, ddof=1) / np.var(a, ddof=1))
    return {"sd_a": float(np.std(a, ddof=1)), "sd_b": float(np.std(b, ddof=1)),
            "brown_forsythe": {"statistic": float(bf.statistic), "p_value": float(bf.pvalue)},
            "f_ratio": {"statistic": f, "p_value": float(st.f.sf(f, len(b) - 1, len(a) - 1))}}


def holm(p_values: Sequence[float]) -> np.ndarray:
    """Holm step-down adjusted p values, in the input order."""
    p = np.asarray(p_values, float)
    m = len(p)
    order = np.argsort(p)
    adj = np.empty(m)
    running = 0.0
    for k, i in enumerate(order):
        running = max(running, min(1.0, (m - k) * p[i]))
        adj[i] = running
    return adj
