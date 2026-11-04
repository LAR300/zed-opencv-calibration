"""The calibration file read by the ZED SDK (optional_opencv_calibration_file), as the C++
tools write it (common/calib_io/opencv_calib_file.cpp): Size, K_LEFT, K_RIGHT, D_LEFT,
D_RIGHT (or D_LEFT_FE/D_RIGHT_FE), R as a Rodrigues vector and T in mm.

Distortion models: rational8 (k1, k2, p1, p2, k3, k4, k5, k6), radtan5 (k1, k2, p1, p2, k3)
and fisheye4 (k1, k2, k3, k4). The SDK was verified with rational8 (1x8) and with 1x14 files;
radtan5 is written 1x5 for the offline analysis.
"""
import os
from dataclasses import dataclass
from typing import Tuple

import cv2
import numpy as np

MODELS = ("rational8", "radtan5", "fisheye4")


@dataclass
class StereoCalibration:
    image_size: Tuple[int, int]
    model: str
    K_left: np.ndarray
    D_left: np.ndarray
    K_right: np.ndarray
    D_right: np.ndarray
    R: np.ndarray  # 3x3, left -> right
    T: np.ndarray  # (3,), mm, negative Tx

    @property
    def fisheye(self) -> bool:
        return self.model == "fisheye4"

    @property
    def baseline_mm(self) -> float:
        return float(np.linalg.norm(self.T))


def _model_from(d: np.ndarray, fisheye: bool, key: str) -> Tuple[str, np.ndarray]:
    d = np.asarray(d, dtype=np.float64).ravel()
    if fisheye:
        if d.size != 4:
            raise ValueError("%s must have 4 coefficients" % key)
        return "fisheye4", d
    if d.size == 5:
        return "radtan5", d
    if d.size in (8, 14):
        if np.any(d[8:] != 0):
            raise ValueError("%s has non-zero terms beyond the 8th (thin prism or tilt)" % key)
        return "rational8", d[:8].copy()
    raise ValueError("%s must have 5, 8 or 14 coefficients" % key)


def read_calibration(path: str) -> StereoCalibration:
    if not os.path.exists(path):
        raise FileNotFoundError(path)
    fs = cv2.FileStorage(path, cv2.FILE_STORAGE_READ)
    if not fs.isOpened():
        raise OSError("cannot open %s" % path)
    fisheye = not fs.getNode("D_LEFT_FE").empty()
    keys = ("Size", "K_LEFT", "K_RIGHT", "D_LEFT_FE" if fisheye else "D_LEFT",
            "D_RIGHT_FE" if fisheye else "D_RIGHT", "R", "T")
    for key in keys:
        if fs.getNode(key).empty():
            raise ValueError("%s has no %s" % (path, key))
    size_node = fs.getNode("Size")
    if size_node.isSeq():  # "[ 1280, 720 ]", as the C++ tools and the SDK write it
        image_size = (int(size_node.at(0).real()), int(size_node.at(1).real()))
    else:                  # a 2x1 matrix, as cv2 writes a tuple
        w, h = size_node.mat().ravel()[:2]
        image_size = (int(w), int(h))
    model_l, D_left = _model_from(fs.getNode(keys[3]).mat(), fisheye, keys[3])
    model_r, D_right = _model_from(fs.getNode(keys[4]).mat(), fisheye, keys[4])
    if model_l != model_r:
        raise ValueError("%s: the two cameras use different distortion models" % path)
    R, _ = cv2.Rodrigues(fs.getNode("R").mat().astype(np.float64).reshape(3, 1))
    return StereoCalibration(image_size=image_size, model=model_l,
                             K_left=fs.getNode("K_LEFT").mat().astype(np.float64),
                             D_left=D_left,
                             K_right=fs.getNode("K_RIGHT").mat().astype(np.float64),
                             D_right=D_right, R=R,
                             T=fs.getNode("T").mat().astype(np.float64).ravel())


def write_calibration(path: str, c: StereoCalibration, overwrite: bool = False) -> None:
    if c.model not in MODELS:
        raise ValueError("unknown model %r" % c.model)
    if not overwrite and os.path.exists(path):
        raise FileExistsError("%s already exists and is never overwritten" % path)
    fs = cv2.FileStorage(path, cv2.FILE_STORAGE_WRITE)
    # A flow sequence, as cv::FileStorage writes a cv::Size: the SDK refuses the 2x1 matrix
    # that fs.write("Size", tuple) would produce
    fs.startWriteStruct("Size", cv2.FileNode_SEQ | cv2.FileNode_FLOW)
    fs.write("", int(c.image_size[0]))
    fs.write("", int(c.image_size[1]))
    fs.endWriteStruct()
    fs.write("K_LEFT", np.asarray(c.K_left, np.float64))
    fs.write("K_RIGHT", np.asarray(c.K_right, np.float64))
    suffix = "_FE" if c.fisheye else ""
    fs.write("D_LEFT" + suffix, np.asarray(c.D_left, np.float64).reshape(1, -1))
    fs.write("D_RIGHT" + suffix, np.asarray(c.D_right, np.float64).reshape(1, -1))
    rvec, _ = cv2.Rodrigues(np.asarray(c.R, np.float64))
    fs.write("R", rvec.reshape(3, 1))
    fs.write("T", np.asarray(c.T, np.float64).reshape(3, 1))
    fs.release()
