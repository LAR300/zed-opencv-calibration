import json
import os

import numpy as np
import pytest

from zedcal import coverage, io, synthetic
from zedcal.residuals import Camera, left_camera

FIXTURE = os.path.join(os.path.dirname(__file__), "..", "..", "tests", "fixtures", "pose_geometry.json")


def test_angles_match_the_shared_fixture():
    doc = json.load(open(FIXTURE))
    cam = Camera(np.array(doc["camera"]["K"], float), np.zeros(8))
    for c in doc["cases"]:
        g = coverage.view_geometry(cam, doc["camera"]["image_size"], doc["board_mm"][0], c["rvec"], c["tvec"])
        assert abs(g["tilt_h_deg"] - c["tilt_h_deg"]) < 1e-6, c["name"]
        assert abs(g["tilt_v_deg"] - c["tilt_v_deg"]) < 1e-6, c["name"]
        assert min(abs(g["rotation_deg"] - c["rotation_deg"]), 360 - abs(g["rotation_deg"] - c["rotation_deg"])) < 1e-6
        assert abs(g["distance_m"] - c["distance_m"]) < 1e-9
    frontal = coverage.view_geometry(cam, (1280, 720), 660, doc["cases"][0]["rvec"], doc["cases"][0]["tvec"])
    assert abs(frontal["fraction"] - doc["frontal_fraction"]) < 1e-9


def make(tmp_path, poses, name="c.json"):
    rig = synthetic.zed_like_rig()
    path = str(tmp_path / name)
    synthetic.write(path, synthetic.generate(rig, poses))
    return rig, io.load_detections(path)


def tilted_pose(tilt_h=0.0, tilt_v=0.0, x=0.0, y=0.0, z=900.0):
    """A board pose with the given tilts (fixture convention), its centre at (x, y, z)."""
    import cv2
    a, b = np.radians(-tilt_h), np.radians(-tilt_v)
    Rx = np.array([[1, 0, 0], [0, np.cos(a), -np.sin(a)], [0, np.sin(a), np.cos(a)]])
    Ry = np.array([[np.cos(b), 0, np.sin(b)], [0, 1, 0], [-np.sin(b), 0, np.cos(b)]])
    R = Ry @ Rx @ np.diag([1.0, -1.0, -1.0])
    c = synthetic.board_center(synthetic.KALIBR_6X6)
    t = np.array([x, y, z]) - R @ c
    rv, _ = cv2.Rodrigues(R)
    return rv.ravel(), t


def good_poses():
    poses = []
    # Wide enough for the right camera too, whose centre is 120 mm to the right
    for x in (-1000, 0, 1000):
        for y in (-600, 0, 600):
            poses.append(tilted_pose(x=x, y=y, z=1200))
            poses.append(tilted_pose(x=x * 0.6, y=y * 0.6, z=700))
    poses += [tilted_pose(tilt_h=35), tilted_pose(tilt_h=-35), tilted_pose(tilt_v=35), tilted_pose(tilt_v=-35)]
    return poses


def test_good_capture_passes(tmp_path):
    rig, det = make(tmp_path, good_poses())
    r = coverage.check(det, left_camera(rig))
    assert r["passed"], r["reasons"]


def test_missing_top_right_corner_is_reported(tmp_path):
    poses = [p for p in good_poses()]
    rig, det = make(tmp_path, poses)
    # remove every corner in the top-right cell of the left image
    for f in det.frames:
        for i in list(f.left):
            c = f.left[i]
            if np.any((c[:, 0] >= 1280 * 7 / 8) & (c[:, 1] < 720 / 8)):
                del f.left[i]
    r = coverage.check(det, left_camera(rig))
    assert not r["passed"]
    assert any("image corner cell (0,7)" in m for m in r["reasons"]), r["reasons"]


def test_missing_vertical_tilt_is_reported(tmp_path):
    poses = [p for p in good_poses()][:-2]  # drop the two vertical-axis tilts
    rig, det = make(tmp_path, poses)
    r = coverage.check(det, left_camera(rig))
    assert not r["passed"]
    assert any(m.startswith("vertical tilt insufficient") for m in r["reasons"]), r["reasons"]
    assert not any(m.startswith("horizontal tilt") for m in r["reasons"])


def test_one_sign_only_is_reported_when_both_are_required(tmp_path):
    poses = good_poses()[:-4] + [tilted_pose(tilt_h=35), tilted_pose(tilt_h=40),
                                 tilted_pose(tilt_v=35), tilted_pose(tilt_v=-35)]
    rig, det = make(tmp_path, poses)
    r = coverage.check(det, left_camera(rig))
    assert any("horizontal tilt insufficient" in m and "one side" in m for m in r["reasons"])
    r2 = coverage.check(det, left_camera(rig), coverage.CoverageCriteria(both_signs=False))
    assert r2["passed"], r2["reasons"]


def test_density_counts_exactly():
    det = io.Detections(image_size=(1280, 720), target=synthetic.KALIBR_6X6, detector={}, frames=[
        io.Frame(pose_id="x", n=0, file_left="", file_right="",
                 left={0: np.array([[10.0, 10], [170.0, 10], [10, 100.0], [1279.0, 719.0]])})])
    d = coverage.density(det, "left")
    assert d[0, 0] == 1 and d[0, 1] == 1 and d[1, 0] == 1 and d[7, 7] == 1 and d.sum() == 4


def test_command_exit_codes(tmp_path):
    import yaml
    from zedcal import coverage_command
    from zedcal.calib_file import write_calibration
    rig, det = make(tmp_path, good_poses(), "good.json")
    write_calibration(str(tmp_path / "f.yml"), rig)
    cfg = {"detections": str(tmp_path / "good.json"), "calibration": str(tmp_path / "f.yml"),
           "output_dir": str(tmp_path / "ok")}
    yaml.safe_dump(cfg, open(tmp_path / "ok.yaml", "w"))
    assert coverage_command.main(["--config", str(tmp_path / "ok.yaml")]) == 0
    assert sorted(os.listdir(tmp_path / "ok")) == ["config.yaml", "coverage.json", "coverage.png"]
    make(tmp_path, good_poses()[:-2], "bad.json")
    cfg.update({"detections": str(tmp_path / "bad.json"), "output_dir": str(tmp_path / "bad")})
    yaml.safe_dump(cfg, open(tmp_path / "bad.yaml", "w"))
    assert coverage_command.main(["--config", str(tmp_path / "bad.yaml")]) == 1
    assert coverage_command.main(["--config", str(tmp_path / "bad.yaml")]) == 2  # folder used
