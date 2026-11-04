"""R7 metric test analysis (spec validacao-metrica)."""
import copy

import cv2
import numpy as np
import pytest

from zedcal import io, metric_test
from zedcal.synthetic import KALIBR_6X6, board_center, generate, write, zed_like_rig

POSITIONS = {"MC": (0.5, 0.5), "ML": (1 / 6, 0.5), "MR": (5 / 6, 0.5), "TC": (0.5, 1 / 6), "BC": (0.5, 5 / 6)}
D0 = 25.0


def metric_session(tmp_path, rig, distances_cm=(50, 100, 200, 300)):
    """Frontal board at known distances: 5 at the centre and one per edge, as the script."""
    poses, ids = [], []
    R, _ = cv2.Rodrigues(np.array([np.pi, 0.0, 0.0]))
    c = board_center(KALIBR_6X6)
    K = rig.K_left
    for cm in distances_cm:
        z = cm * 10.0
        for pos, (u, v) in POSITIONS.items():
            for n in range(1, 6 if pos == "MC" else 2):
                x = (u * 1280 - K[0, 2]) * z / K[0, 0]
                y = (v * 720 - K[1, 2]) * z / K[1, 1]
                poses.append((cv2.Rodrigues(R)[0].ravel(), np.array([x, y, z]) - R @ c))
                ids.append(("MET-%d-%s-%d" % (cm, pos, n), z))
    d = generate(rig, poses)
    for f, (pid, _) in zip(d["frames"], ids):
        f["pose_id"] = pid
    write(str(tmp_path / "m.json"), d)
    det = io.load_detections(str(tmp_path / "m.json"))
    return det, [z - D0 for _, z in ids]      # measured from a point d0 in front of the centre


def test_pose_ids():
    assert metric_test.parse_pose_id("MET-300-MC-1") == (300, "MC")
    assert metric_test.parse_pose_id("B1-P-TL") is None


def test_recovers_d0(tmp_path):
    rig = zed_like_rig()
    det, measured = metric_session(tmp_path, rig)
    r = metric_test.analyse(rig, det, measured)
    assert abs(r["d0_mm"] - D0) < 1.0
    assert [b["label_cm"] for b in r["by_distance"]] == [50, 100, 200, 300]
    for b in r["by_distance"]:
        assert abs(b["residual_after_d0_mm"]) < 1e-3
        assert b["epipolar_px"]["rms"] < 1e-6
        assert b["positions"]["centre"]["pitch_pct"]["rms"] < 1e-6
    assert r["by_distance"][-1]["positions"]["edges"]["views"] >= 2


def test_long_baseline_leaves_a_residual_proportional_to_distance(tmp_path):
    rig = zed_like_rig()
    det, measured = metric_session(tmp_path, rig)
    bad = copy.deepcopy(rig)
    bad.T = rig.T * 1.01
    r = metric_test.analyse(bad, det, measured)
    meas = np.array([b["measured_mm_mean"] for b in r["by_distance"]])
    res = np.array([b["residual_after_d0_mm"] for b in r["by_distance"]])
    slope = np.polyfit(meas, res, 1)[0]
    assert slope == pytest.approx(0.01, rel=0.05)          # 1 % of the distance
    assert r["two_parameter_fit"]["scale_error_pct"] == pytest.approx(1.0, abs=0.02)
    assert r["two_parameter_fit"]["d0_mm"] == pytest.approx(D0 * 1.01, abs=0.5)
    for b in r["by_distance"]:
        assert b["positions"]["centre"]["span_pct"]["mean"] == pytest.approx(1.0, abs=0.02)


def test_measured_distances_from_meta_or_labels(tmp_path):
    rig = zed_like_rig()
    det, _ = metric_session(tmp_path, rig, distances_cm=(50,))
    meta = {"frames": [{"pose_id": "MET-50-MC-1", "n": 0, "measured_distance": {"value_mm": 476.0}}]}
    got = metric_test.measured_distances(det, meta, {"50": 475.0})
    assert got[0] == 476.0 and got[1] == 475.0
    assert metric_test.measured_distances(det, None, None)[0] is None


def test_command(tmp_path):
    import os
    import yaml
    from zedcal import metric_test_command
    from zedcal.calib_file import write_calibration
    rig = zed_like_rig()
    metric_session(tmp_path, rig)
    write_calibration(str(tmp_path / "true.yml"), rig)
    cfg = {"detections": str(tmp_path / "m.json"), "calibrations": {"true": str(tmp_path / "true.yml")},
           "target_measured": {"tag_size_mm": 88.0, "tag_spacing_mm": 26.4},
           "measured_distances_mm": {50: 475.0, 100: 975.0, 200: 1975.0, 300: 2975.0},
           "output_dir": str(tmp_path / "out")}
    yaml.safe_dump(cfg, open(tmp_path / "c.yaml", "w"))
    assert metric_test_command.main(["--config", str(tmp_path / "c.yaml")]) == 0
    assert sorted(os.listdir(tmp_path / "out")) == ["config.yaml", "metric_test.json", "metric_test.png", "report.md"]
    del cfg["measured_distances_mm"]
    cfg["output_dir"] = str(tmp_path / "out2")
    yaml.safe_dump(cfg, open(tmp_path / "c.yaml", "w"))
    assert metric_test_command.main(["--config", str(tmp_path / "c.yaml")]) == 2
