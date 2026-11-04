# zedcal — offline analysis of the calibration captures

Python package for the offline part of the calibration methodology: it reads the captures written
by `zed_stereo_calibration --session` (or exported with `--detections_out`), calibrates them again
with other models and variants, and computes the metrics (M1–M4) and the statistics.
Specs: `.ai/specs/nucleo-offline/` and the metric specs next to it.

- **No corner detection here.** The corners always come from the C++ detector, through
  `detections.json`: the Bias Ratio needs the noise of the detector the calibration used.
  `tests/test_no_detection.py` enforces it.
- Runs in the `isaac-ros-dev` container (Python 3, NumPy 1.26, SciPy 1.15, OpenCV 4.5.4,
  Matplotlib 3.5, PyYAML, pytest 6.2): no new dependency.
- Units: mm for the target and T, px for the image. Error fields name their convention,
  e.g. `rms_per_corner_px`, `mse_per_coord_px2`.

```bash
cd analysis && python3 -m pytest tests        # tests
python3 -m zedcal.<command> --config <file>    # commands, see configs/
```
