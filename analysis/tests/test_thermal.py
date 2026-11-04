"""Warm-up analysis (spec aquecimento-termico)."""
import numpy as np
import pytest

from zedcal import io, thermal
from zedcal.synthetic import generate, random_poses, write, zed_like_rig


def test_exponential_drift_steady_time():
    rng = np.random.default_rng(0)
    t = np.arange(0, 60, 0.5)                      # one pair every 30 s for 60 min
    sigma, amplitude, tau = 0.05, 0.5, 8.0
    q = amplitude * np.exp(-t / tau) + rng.normal(0, sigma, t.size)
    s = thermal.steady_time(t, q, window_min=5.0, factor=2.0)
    truth = tau * np.log(amplitude / (2 * sigma))   # the drift left equals factor * sigma: 12.9 min
    assert s.sigma == pytest.approx(sigma, rel=0.25)
    assert abs(s.time_min - truth) < 5.0, (s.time_min, truth)


def test_steady_time_is_within_one_window_in_most_runs():
    t = np.arange(0, 60, 0.5)
    truth = 8.0 * np.log(0.5 / 0.1)
    times = [thermal.steady_time(t, 0.5 * np.exp(-t / 8.0) + np.random.default_rng(s).normal(0, 0.05, t.size)).time_min
             for s in range(100)]
    times = np.array([np.inf if x is None else x for x in times])
    assert np.mean(np.abs(times - truth) < 5.0) >= 0.9


def test_flat_series_is_steady_from_the_start():
    rng = np.random.default_rng(1)
    t = np.arange(0, 60, 0.5)
    s = thermal.steady_time(t, rng.normal(0, 0.05, t.size))
    assert s.time_min == pytest.approx(2.5)        # centre of the first window


def test_drift_until_the_end_is_not_settled():
    t = np.arange(0, 60, 0.5)
    s = thermal.steady_time(t, 0.01 * t + np.random.default_rng(2).normal(0, 0.05, t.size))
    assert s.time_min is None


def _thermal_capture(tmp_path, rig, n=40, sigma=0.1):
    pose = random_poses(1, np.random.default_rng(4), distance_mm=(900.0, 900.0), max_tilt_deg=10.0,
                        max_offset=0.05)[0]
    d = generate(rig, [pose] * n, sigma_px=sigma, rng=np.random.default_rng(5))
    meta = {"schema_version": 1, "frames": []}
    for k, f in enumerate(d["frames"]):
        f["pose_id"] = "thermal"
        meta["frames"].append({"pose_id": "thermal", "n": k, "timestamp_ns": str(10 ** 18 + k * 30 * 10 ** 9),
                               "temperature_c": {"onboard_left": 30.0 + 0.2 * k, "onboard_right": 30.5, "imu": 31.0}})
    write(str(tmp_path / "t.json"), d)
    return io.load_detections(str(tmp_path / "t.json")), meta


def test_series_without_drift_are_flat(tmp_path):
    rig = zed_like_rig()
    det, meta = _thermal_capture(tmp_path, rig)
    s = thermal.series(rig, det, meta)
    assert s["timing"] == "timestamp" and s["has_temperature"]
    assert s["time_min"][1] == pytest.approx(0.5)
    for name in thermal.SERIES:
        q = np.array([r[name] for r in s["rows"]])
        if name == "transfer_rms_px":
            assert np.all(q < 0.2)
            continue
        # flat: no trend beyond the noise
        slope = np.polyfit(s["time_min"], q, 1)[0] * (s["time_min"][-1])
        assert abs(slope) < 4 * max(thermal.noise_sigma(q), 1e-9), name
    r = thermal.analyse(s)
    assert r["steady_min"] is not None and r["steady_min"] <= 5.0
    assert r["temperature_at_steady_c"]["onboard_left"] >= 30.0


def test_proposed_x():
    assert thermal.proposed_x([12.3, 17.01, 9.0]) == 18
    assert thermal.proposed_x([12.0, None]) is None


def test_command(tmp_path):
    import json
    import os
    import yaml
    from zedcal import thermal_command
    from zedcal.calib_file import write_calibration
    rig = zed_like_rig()
    det, meta = _thermal_capture(tmp_path, rig)
    json.dump(meta, open(tmp_path / "meta.json", "w"))
    write_calibration(str(tmp_path / "F.yml"), rig)
    cfg = {"records": [{"name": "dia1", "detections": str(tmp_path / "t.json")}],
           "calibration": str(tmp_path / "F.yml"), "output_dir": str(tmp_path / "out")}
    yaml.safe_dump(cfg, open(tmp_path / "c.yaml", "w"))
    assert thermal_command.main(["--config", str(tmp_path / "c.yaml")]) == 0
    assert sorted(os.listdir(tmp_path / "out")) == ["config.yaml", "report.md", "thermal.json",
                                                     "thermal_vs_temperature.png", "thermal_vs_time.png"]
    r = json.load(open(tmp_path / "out" / "thermal.json"))
    assert r["proposed_x_min"] is not None and "warning" in r
