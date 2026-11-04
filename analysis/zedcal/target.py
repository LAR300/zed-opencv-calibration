"""Calibration target geometry, in the convention of the C++ tools.

AprilGrid (Kalibr, decisions D5 of aprilgrid-target): IDs grow left to right along a row,
then rows bottom to top; board frame X right, Y up, origin at the outer corner of tag 0, mm.
Corners of a tag in the order OpenCV returns them: BR, BL, TL, TR
(common/aprilgrid/aprilgrid.cpp tagCorners3D). Checkerboard: inner corners row-major,
(square * col, square * row, 0), as calibrate() builds them.
"""
from dataclasses import dataclass, replace
from typing import Optional

import numpy as np


@dataclass(frozen=True)
class Target:
    type: str                       # "aprilgrid" or "checkerboard"
    rows: int
    cols: int
    tag_size_mm: float = 0.0        # aprilgrid
    tag_spacing_ratio: float = 0.0  # aprilgrid: space / tag size
    square_size_mm: float = 0.0     # checkerboard

    @property
    def pitch_mm(self) -> float:
        return self.tag_size_mm * (1.0 + self.tag_spacing_ratio)

    def tag_corners(self, tag_id: int) -> np.ndarray:
        """3D corners (4x3, mm) of an AprilGrid tag, in the order BR, BL, TL, TR."""
        if self.type != "aprilgrid":
            raise ValueError("tag_corners needs an AprilGrid target")
        if not 0 <= tag_id < self.rows * self.cols:
            raise ValueError("tag id %d is not part of a %dx%d grid" % (tag_id, self.rows, self.cols))
        x0 = (tag_id % self.cols) * self.pitch_mm
        y0 = (tag_id // self.cols) * self.pitch_mm
        s = self.tag_size_mm
        return np.array([[x0 + s, y0, 0.0], [x0, y0, 0.0], [x0, y0 + s, 0.0], [x0 + s, y0 + s, 0.0]])

    def checkerboard_points(self) -> np.ndarray:
        """3D inner corners (Nx3, mm) of a checkerboard, row-major."""
        if self.type != "checkerboard":
            raise ValueError("checkerboard_points needs a checkerboard target")
        rows, cols = np.mgrid[0:self.rows, 0:self.cols]
        return np.column_stack([cols.ravel() * self.square_size_mm,
                                rows.ravel() * self.square_size_mm,
                                np.zeros(self.rows * self.cols)])

    def with_measured(self, tag_size_mm: Optional[float] = None,
                      tag_spacing_mm: Optional[float] = None) -> "Target":
        """The same grid with the dimensions measured on the printed target."""
        size = self.tag_size_mm if tag_size_mm is None else tag_size_mm
        spacing = (self.tag_spacing_ratio * self.tag_size_mm if tag_spacing_mm is None
                   else tag_spacing_mm)
        return replace(self, tag_size_mm=size, tag_spacing_ratio=spacing / size)
