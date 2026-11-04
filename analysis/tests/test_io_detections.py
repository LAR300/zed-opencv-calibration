import json

import numpy as np
import pytest

from zedcal import io


def write(tmp_path, data, name="detections.json"):
    path = tmp_path / name
    path.write_text(json.dumps(data))
    return str(path)


def sample():
    def tag(i, x):
        return {"id": i, "corners": [[x + 10, 0], [x, 0], [x, 10], [x + 10, 10]],
                "corners_unrefined": [[x + 10.5, 0], [x, 0], [x, 10], [x + 10, 10]]}
    return {
        "schema_version": 1,
        "image_size": [1280, 720],
        "target": {"type": "aprilgrid", "rows": 6, "cols": 6, "tag_size_mm": 88.0,
                   "tag_spacing_ratio": 0.3, "family": "tag36h11", "id_convention": "kalibr",
                   "corner_order": ["BR", "BL", "TL", "TR"], "source": "target.yaml"},
        "detector": {"refinement": "xjunction_cornersubpix", "params": {"adaptiveThreshConstant": 25.0}},
        "frames": [{"pose_id": "free", "n": 0,
                    "files": {"left": "left/free_0.png", "right": "right/free_0.png"},
                    "left": {"tags": [tag(3, 100), tag(1, 0), tag(7, 200)]},
                    "right": {"tags": [tag(1, 5), tag(7, 205)]}}],
    }


def test_reads_frames_and_target(tmp_path):
    d = io.load_detections(write(tmp_path, sample()))
    assert d.image_size == (1280, 720)
    assert d.target.type == "aprilgrid" and d.target.cols == 6
    assert len(d.frames) == 1
    f = d.frames[0]
    assert (f.pose_id, f.n) == ("free", 0)
    assert sorted(f.left) == [1, 3, 7] and sorted(f.right) == [1, 7]
    assert f.left[3].shape == (4, 2)
    assert f.left_unrefined[3][0, 0] == 110.5


def test_mono_correspondences_use_every_tag_of_the_camera(tmp_path):
    d = io.load_detections(write(tmp_path, sample()))
    obj, img, ids = io.mono_correspondences(d.frames[0], "left", d.target)
    assert ids == [1, 3, 7]  # sorted, as the C++ std::map
    assert obj.shape == (12, 3) and img.shape == (12, 2)
    np.testing.assert_allclose(obj[:4], d.target.tag_corners(1))


def test_common_correspondences_keep_tags_seen_by_both(tmp_path):
    d = io.load_detections(write(tmp_path, sample()))
    obj, img_l, img_r, ids = io.common_correspondences(d.frames[0], d.target)
    assert ids == [1, 7]
    assert obj.shape == (8, 3) and img_l.shape == (8, 2) and img_r.shape == (8, 2)
    np.testing.assert_allclose(img_r[0], [15, 0])


def test_refuses_another_schema_version(tmp_path):
    data = sample()
    data["schema_version"] = 2
    with pytest.raises(io.FormatError, match="schema_version"):
        io.load_detections(write(tmp_path, data))


def test_reads_checkerboard_detections(tmp_path):
    data = sample()
    data["target"] = {"type": "checkerboard", "rows": 6, "cols": 9, "square_size_mm": 23.8,
                      "corner_order": "row_major", "source": "command line"}
    corners = [[float(i), float(i) / 2] for i in range(54)]
    data["frames"] = [{"pose_id": "legacy", "n": 2,
                       "files": {"left": "zed-images/image_left_2.png", "right": "x.png"},
                       "left": {"corners": corners}, "right": {"corners": []}}]
    d = io.load_detections(write(tmp_path, data))
    f = d.frames[0]
    assert f.left_corners.shape == (54, 2) and f.right_corners.shape == (0, 2)
    obj, img, _ = io.mono_correspondences(f, "left", d.target)
    assert obj.shape == (54, 3)
    with pytest.raises(ValueError):
        io.mono_correspondences(f, "right", d.target)  # an incomplete board is not usable


def test_reads_meta_with_nulls(tmp_path):
    meta = {"schema_version": 1, "camera": {"serial": 35180697, "resolution": [1280, 720]},
            "session": None,
            "frames": [{"pose_id": "free", "n": 0, "timestamp_ns": "1793740637958548975",
                        "temperature_c": {"onboard_left": None, "onboard_right": 30.1, "imu": None}}]}
    m = io.load_meta(write(tmp_path, meta, "meta.json"))
    assert m["camera"]["serial"] == 35180697
    assert m["frames"][0]["timestamp_ns"] == 1793740637958548975  # text -> exact int
    assert m["frames"][0]["temperature_c"]["onboard_left"] is None
