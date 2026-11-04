import json
import os

import numpy as np
import pytest
import yaml

from zedcal import calibrate, synthetic


def make_config(tmp_path, out_name):
    rig = synthetic.zed_like_rig()
    det = str(tmp_path / "detections.json")
    if not os.path.exists(det):
        synthetic.write(det, synthetic.generate(rig, synthetic.random_poses(22, np.random.default_rng(5)),
                                                sigma_px=0.1, rng=np.random.default_rng(6)))
    cfg = {"detections": det, "output_dir": str(tmp_path / out_name), "model": "rational8",
           "variants": [{"mono_points": "common", "stereo": "joint", "rejection": "cpp"}]}
    path = str(tmp_path / (out_name + ".yaml"))
    yaml.safe_dump(cfg, open(path, "w"))
    return path


def test_command_writes_calibration_report_and_config(tmp_path):
    assert calibrate.main(["--config", make_config(tmp_path, "out")]) == 0
    files = sorted(os.listdir(tmp_path / "out"))
    assert files == ["calib_rational8_common_joint_cpp.report.json",
                     "calib_rational8_common_joint_cpp.yml", "config.yaml"]


def test_two_runs_give_identical_files(tmp_path):
    calibrate.main(["--config", make_config(tmp_path, "a")])
    calibrate.main(["--config", make_config(tmp_path, "b")])
    for name in ("calib_rational8_common_joint_cpp.yml",):
        assert open(tmp_path / "a" / name).read() == open(tmp_path / "b" / name).read()
    ra = json.load(open(tmp_path / "a" / "calib_rational8_common_joint_cpp.report.json"))
    rb = json.load(open(tmp_path / "b" / "calib_rational8_common_joint_cpp.report.json"))
    ra.pop("config"); rb.pop("config")  # the config paths differ by construction
    assert ra == rb
    assert "versions" in ra and "commit" in ra["code"]


def test_output_folder_is_never_overwritten(tmp_path):
    path = make_config(tmp_path, "c")
    calibrate.main(["--config", path])
    with pytest.raises(FileExistsError):
        calibrate.main(["--config", path])
