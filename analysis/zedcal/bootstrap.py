"""Bootstrap over the views of a capture (spec nucleo-offline R10).

Each sample draws the views with replacement and calibrates again. Hagemann et al. (2022)
show that the standard covariance of the calibration underestimates the uncertainty on real
data, while the bootstrap matches the observed spread (referencias.md A.4).
"""
from dataclasses import dataclass, replace
from typing import List

import numpy as np

from . import calib, io

PARAMETERS = ["fx_left", "fy_left", "cx_left", "cy_left",
              "fx_right", "fy_right", "cx_right", "cy_right", "baseline_mm"]


def parameter_vector(c) -> np.ndarray:
    return np.array([c.K_left[0, 0], c.K_left[1, 1], c.K_left[0, 2], c.K_left[1, 2],
                     c.K_right[0, 0], c.K_right[1, 1], c.K_right[0, 2], c.K_right[1, 2],
                     c.baseline_mm])


@dataclass
class BootstrapResult:
    seed: int
    samples: np.ndarray       # n x len(PARAMETERS)
    calibrations: list        # StereoCalibration of every sample

    @property
    def std(self) -> dict:
        return dict(zip(PARAMETERS, np.std(self.samples, axis=0, ddof=1)))


def bootstrap(det: io.Detections, options: calib.CalibrationOptions, n: int = 50,
              seed: int = 0) -> BootstrapResult:
    rng = np.random.default_rng(seed)
    views = calib.build_views(det)
    valid = [i for i, v in enumerate(views) if v.stereo_ok]
    samples, calibrations = [], []
    for _ in range(n):
        pick = rng.choice(valid, size=len(valid), replace=True)
        resampled = replace(det, frames=[det.frames[i] for i in pick])
        c = calib.calibrate(resampled, options).calibration
        samples.append(parameter_vector(c))
        calibrations.append(c)
    return BootstrapResult(seed=seed, samples=np.array(samples), calibrations=calibrations)
