import numpy as np

from zedcal import io, synthetic
from zedcal.residuals import left_camera, residuals


def test_generated_file_reads_back(tmp_path):
    rig = synthetic.zed_like_rig()
    poses = synthetic.random_poses(5, np.random.default_rng(1))
    path = str(tmp_path / "detections.json")
    synthetic.write(path, synthetic.generate(rig, poses))
    d = io.load_detections(path)
    assert len(d.frames) == 5
    assert all(len(f.left) >= 12 for f in d.frames)
    assert all(len(f.right) >= 12 for f in d.frames)


def test_noise_has_the_requested_sigma(tmp_path):
    rig = synthetic.zed_like_rig()
    poses = synthetic.random_poses(40, np.random.default_rng(2))
    path = str(tmp_path / "detections.json")
    synthetic.write(path, synthetic.generate(rig, poses, sigma_px=0.1, rng=np.random.default_rng(3)))
    d = io.load_detections(path)
    res = []
    for f, (rv, tv) in zip(d.frames, poses):
        obj, img, _ = io.mono_correspondences(f, "left", d.target)
        res.append(residuals(obj, img, rv, tv, left_camera(rig)))
    res = np.concatenate(res)
    assert res.size >= 2 * 4000
    assert abs(np.std(res) - 0.1) < 0.005


def test_exact_data_without_noise():
    rig = synthetic.zed_like_rig()
    poses = synthetic.random_poses(3, np.random.default_rng(4))
    d = synthetic.generate(rig, poses)
    f = d["frames"][0]["left"]["tags"][0]
    obj = synthetic.KALIBR_6X6.tag_corners(f["id"])
    rv, tv = poses[0]
    assert np.max(np.abs(residuals(obj, np.array(f["corners"]), rv, tv, left_camera(rig)))) < 1e-9


def test_flex_and_scale_change_the_image():
    rig = synthetic.zed_like_rig()
    poses = synthetic.random_poses(1, np.random.default_rng(5))
    base = synthetic.generate(rig, poses)["frames"][0]["left"]["tags"]
    bent = synthetic.generate(rig, poses, flex_mm=2.0)["frames"][0]["left"]["tags"]
    scaled = synthetic.generate(rig, poses, scale=1.01)["frames"][0]["left"]["tags"]
    common = {t["id"] for t in base} & {t["id"] for t in bent} & {t["id"] for t in scaled}
    assert common
    i = sorted(common)[len(common) // 2]
    b = np.array(next(t["corners"] for t in base if t["id"] == i))
    assert np.max(np.abs(b - np.array(next(t["corners"] for t in bent if t["id"] == i)))) > 0.1
    assert np.max(np.abs(b - np.array(next(t["corners"] for t in scaled if t["id"] == i)))) > 0.1
