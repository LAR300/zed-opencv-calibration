"""Statistics of analise-estatistica."""
import os
import time

import numpy as np
import pytest
from scipy import stats as st

from zedcal import stats


def test_t_interval_by_hand():
    d = stats.describe(np.arange(1.0, 11.0), "fx_left")
    # mean 5.5, sd 3.0276504, se 0.9574271, t(0.975, 9) = 2.2621572 -> half width 2.1658506
    assert d["ci95"] == pytest.approx([3.3341494, 7.6658506], abs=1e-6)
    assert d["cv"] == pytest.approx(3.0276504 / 5.5, abs=1e-6)
    assert d["cv_informative"]


def test_near_zero_quantity_is_marked():
    d = stats.describe([0.01, -0.02, 0.015, -0.005, 0.0, 0.01, -0.01, 0.02, -0.015, 0.005], "fx_left")
    assert d["cv"] is None and not d["cv_informative"]
    assert stats.describe([1.0, 1.1, 0.9], "ty_mm")["cv"] is None   # by name


def test_factory_position():
    d = stats.describe(np.arange(1.0, 11.0), "fx_left", factory=9.0)
    assert not d["factory_in_ci95"]
    assert d["factory_percentile"] == pytest.approx(85.0)


def test_enumeration_counts():
    assert len(stats._assignments(20, 10, symmetric=False)) == 184756
    assert len(stats._assignments(20, 10, symmetric=True)) == 92378


def _matrix(points):
    return np.linalg.norm(points[:, None] - points[None], axis=2)


def test_permutation_p_is_uniform_under_the_null():
    rng = np.random.default_rng(0)
    labels = ["A"] * 10 + ["B"] * 10
    t0 = time.time()
    reps = 200 if os.environ.get("ZEDCAL_SLOW") else 40   # the criterion asks 200 (~46 s)
    ps = [stats.permutation_test(_matrix(rng.normal(size=(20, 3))), labels, "A", "B")["main"]["p_value"]
          for _ in range(reps)]
    print("%d exact permutation tests of 20 x 20: %.1f s" % (reps, time.time() - t0))
    assert st.kstest(ps, "uniform").pvalue > 0.01


def test_permutation_detects_a_shift():
    rng = np.random.default_rng(1)
    pts = rng.normal(size=(20, 3))
    pts[10:, 0] += 3.0
    r = stats.permutation_test(_matrix(pts), ["A"] * 10 + ["B"] * 10, "A", "B")
    assert r["main"]["p_value"] < 0.01 and r["symmetric"]["p_value"] < 0.01
    assert r["main"]["assignments"] == 184756 and r["symmetric"]["assignments"] == 92378
    assert r["main"]["p_value"] >= 1 / 184756


def test_permutation_statistic_by_hand():
    M = np.array([[0, 1, 5, 5], [1, 0, 5, 5], [5, 5, 0, 3], [5, 5, 3, 0]], float)
    r = stats.permutation_test(M, ["A", "A", "B", "B"], "A", "B")
    assert r["main"]["statistic"] == pytest.approx(5 - 1)
    assert r["symmetric"]["statistic"] == pytest.approx(5 - 2)
    assert r["main"]["p_value"] == pytest.approx(1 / 6)   # the largest of C(4,2) = 6


def test_cross_application():
    rows = cols = ["a1", "a2", "a3", "b1", "b2", "b3"]
    cond = {n: n[0] for n in rows}
    rng = np.random.default_rng(2)
    m = np.full((6, 6), 1.0) + rng.normal(0, 0.01, (6, 6))
    m[:3, 3:] += 0.5            # A calibrations on B captures
    m[3:, :3] += 0.2            # B calibrations on A captures
    r = stats.cross_application(m, rows, cols, cond, cond)
    assert r["a"]["mean_difference"] == pytest.approx(0.5, abs=0.02)
    assert r["b"]["mean_difference"] == pytest.approx(0.2, abs=0.02)
    d = r["a"]["differences"]
    assert r["a"]["ci95"] == pytest.approx(list(stats.t_interval(d)))
    own = [m[0, 1], m[0, 2]]      # the diagonal (training) is left out
    assert d[0] == pytest.approx(np.mean(m[0, 3:]) - np.mean(own))


def test_tost():
    rng = np.random.default_rng(3)
    a = 534.0 + rng.normal(0, 0.3, 10)
    b = 534.0 + rng.normal(0, 0.3, 10)
    r = stats.tost(a, b, margin=2.7)
    assert r["equivalent"] and r["p_value"] < 0.05
    r = stats.tost(a, b + 4.0, margin=2.7)
    assert not r["equivalent"] and r["p_value"] > 0.05
    # by hand: the interval is the Welch 90 % interval
    se = np.sqrt(np.var(a, ddof=1) / 10 + np.var(b, ddof=1) / 10)
    assert r["ci90"][1] - r["ci90"][0] == pytest.approx(2 * st.t.ppf(0.95, r["df"]) * se)


def _normal_sample(n, offset=0.5):
    return st.norm.ppf((np.arange(n) + offset) / n)    # deterministic, sd ~ 1


def test_doubled_sd_is_detected():
    a, b = _normal_sample(10), 2.0 * _normal_sample(10, 0.25) + 5.0
    r = stats.dispersion(a, b)
    assert r["f_ratio"]["p_value"] < 0.05
    # Brown-Forsythe has less power: with n = 10 it misses a doubled sd (p ~ 0.07)
    assert 0.05 < r["brown_forsythe"]["p_value"] < 0.1
    r = stats.dispersion(_normal_sample(20), 2.0 * _normal_sample(20, 0.25))
    assert r["brown_forsythe"]["p_value"] < 0.05 and r["f_ratio"]["p_value"] < 0.01


def test_sd_ratio_interval():
    a, b = _normal_sample(10), 2.0 * _normal_sample(10)
    r = stats.dispersion(a, b)
    assert r["sd_ratio"] == pytest.approx(2.0)
    # F(0.975; 9, 9) = 4.026 -> sqrt(4 / 4.026) .. sqrt(4 * 4.026)
    assert r["sd_ratio_ci95"] == pytest.approx([np.sqrt(4 / 4.0260), np.sqrt(4 * 4.0260)], rel=1e-3)


def test_holm_known_case():
    assert stats.holm([0.01, 0.04, 0.03, 0.005]) == pytest.approx([0.03, 0.06, 0.06, 0.02])


def _perturbed(rng, fx_shift=0.0, sd=0.3):
    import copy
    from zedcal.synthetic import zed_like_rig
    c = copy.deepcopy(zed_like_rig())
    for K in (c.K_left, c.K_right):
        K[0, 0] += fx_shift + rng.normal(0, sd)
        K[1, 1] += fx_shift + rng.normal(0, sd)
        K[0, 2] += rng.normal(0, sd)
        K[1, 2] += rng.normal(0, sd)
    c.T = c.T + rng.normal(0, 0.05, 3)
    return c


def test_report_of_two_synthetic_conditions(tmp_path):
    import yaml
    from zedcal import report_command
    from zedcal.calib_file import write_calibration
    from zedcal.synthetic import zed_like_rig
    rng = np.random.default_rng(5)
    conds = {}
    for c, shift in (("A", 0.0), ("B", 3.0)):
        conds[c] = {"calibrations": {}}
        for i in range(10):
            p = str(tmp_path / ("%s%02d.yml" % (c, i)))
            write_calibration(p, _perturbed(rng, shift))
            conds[c]["calibrations"]["%s%02d" % (c, i)] = p
    write_calibration(str(tmp_path / "F.yml"), zed_like_rig())
    cfg = {"conditions": conds, "factory": str(tmp_path / "F.yml"), "output_dir": str(tmp_path / "out"),
           "margins": {"fx_left": 2.7, "baseline_mm": 0.6}, "mapping_step_px": 40}
    yaml.safe_dump(cfg, open(tmp_path / "c.yaml", "w"), sort_keys=False)
    t0 = time.time()
    assert report_command.main(["--config", str(tmp_path / "c.yaml")]) == 0
    print("report of 2 x 10 calibrations: %.1f s" % (time.time() - t0))
    assert sorted(os.listdir(tmp_path / "out")) == ["config.yaml", "parameters.png", "permutation.png",
                                                     "report.json", "report.md"]
    import json
    r = json.load(open(tmp_path / "out" / "report.json"))["comparison"]
    assert r["permutation"]["left"]["main"]["p_value"] < 0.01     # fx moved 3 px
    assert r["tost"]["baseline_mm"]["equivalent"]
    assert not r["tost"]["fx_left"]["equivalent"]
    md = open(tmp_path / "out" / "report.md").read()
    assert "provisórias" in md and "Holm" in md and "não informativo" in md
