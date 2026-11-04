"""Reading the capture files written by the C++ tools (spec persistencia-captura, Formatos)."""
import json
from dataclasses import dataclass, field
from typing import Dict, List, Tuple

import numpy as np

from .target import Target

SCHEMA_VERSION = 1


class FormatError(ValueError):
    """A capture file that this version cannot read."""


@dataclass
class Frame:
    pose_id: str
    n: int
    file_left: str
    file_right: str
    # AprilGrid: tag id -> 4x2 corners (BR, BL, TL, TR), after / before the refinement
    left: Dict[int, np.ndarray] = field(default_factory=dict)
    right: Dict[int, np.ndarray] = field(default_factory=dict)
    left_unrefined: Dict[int, np.ndarray] = field(default_factory=dict)
    right_unrefined: Dict[int, np.ndarray] = field(default_factory=dict)
    # Checkerboard: inner corners, row-major (empty when not found)
    left_corners: np.ndarray = field(default_factory=lambda: np.zeros((0, 2)))
    right_corners: np.ndarray = field(default_factory=lambda: np.zeros((0, 2)))


@dataclass
class Detections:
    image_size: Tuple[int, int]
    target: Target
    detector: dict
    frames: List[Frame]


def _check_version(data, path):
    version = data.get("schema_version")
    if version != SCHEMA_VERSION:
        raise FormatError("%s has schema_version %r; only %d is supported"
                          % (path, version, SCHEMA_VERSION))


def load_detections(path: str) -> Detections:
    data = json.load(open(path))
    _check_version(data, path)
    t = data["target"]
    if t["type"] == "aprilgrid":
        target = Target(type="aprilgrid", rows=t["rows"], cols=t["cols"],
                        tag_size_mm=t["tag_size_mm"], tag_spacing_ratio=t["tag_spacing_ratio"])
    elif t["type"] == "checkerboard":
        target = Target(type="checkerboard", rows=t["rows"], cols=t["cols"],
                        square_size_mm=t["square_size_mm"])
    else:
        raise FormatError("%s: unknown target type %r" % (path, t["type"]))

    frames = []
    for f in data["frames"]:
        frame = Frame(pose_id=f["pose_id"], n=f["n"], file_left=f["files"]["left"],
                      file_right=f["files"]["right"])
        for side in ("left", "right"):
            cam = f[side]
            if target.type == "aprilgrid":
                for tag in cam.get("tags", []):
                    getattr(frame, side)[tag["id"]] = np.array(tag["corners"], dtype=np.float64)
                    getattr(frame, side + "_unrefined")[tag["id"]] = np.array(
                        tag["corners_unrefined"], dtype=np.float64)
            else:
                corners = np.array(cam.get("corners", []), dtype=np.float64).reshape(-1, 2)
                setattr(frame, side + "_corners", corners)
        frames.append(frame)
    return Detections(image_size=tuple(data["image_size"]), target=target,
                      detector=data.get("detector", {}), frames=frames)


def mono_correspondences(frame: Frame, side: str, target: Target):
    """3D/2D points of one camera with every tag it saw, sorted by tag id."""
    if target.type == "checkerboard":
        corners = getattr(frame, side + "_corners")
        if len(corners) != target.rows * target.cols:
            raise ValueError("the %s checkerboard of %s_%d is incomplete"
                             % (side, frame.pose_id, frame.n))
        return target.checkerboard_points(), corners.copy(), None
    tags = getattr(frame, side)
    ids = sorted(tags)
    obj = np.concatenate([target.tag_corners(i) for i in ids]) if ids else np.zeros((0, 3))
    img = np.concatenate([tags[i] for i in ids]) if ids else np.zeros((0, 2))
    return obj, img, ids


def common_correspondences(frame: Frame, target: Target):
    """3D/2D points of the tags seen by both cameras, sorted by id (C++ matchCommonTags)."""
    if target.type == "checkerboard":
        obj, img_l, _ = mono_correspondences(frame, "left", target)
        _, img_r, _ = mono_correspondences(frame, "right", target)
        return obj, img_l, img_r, None
    ids = sorted(set(frame.left) & set(frame.right))
    if not ids:
        return np.zeros((0, 3)), np.zeros((0, 2)), np.zeros((0, 2)), []
    obj = np.concatenate([target.tag_corners(i) for i in ids])
    img_l = np.concatenate([frame.left[i] for i in ids])
    img_r = np.concatenate([frame.right[i] for i in ids])
    return obj, img_l, img_r, ids


def load_meta(path: str) -> dict:
    """meta.json as a dict; the nanosecond timestamps (text, 19 digits) become ints."""
    data = json.load(open(path))
    _check_version(data, path)
    for f in data.get("frames", []):
        if isinstance(f.get("timestamp_ns"), str):
            f["timestamp_ns"] = int(f["timestamp_ns"])
    return data
