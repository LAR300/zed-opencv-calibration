# ZED OpenCV Calibration

A camera calibration toolkit for ZED cameras using OpenCV.

## Overview

This project provides five main applications for working with ZED cameras:

1. **Monocular Calibration Tool** - Interactive calibration for single ZED X One cameras.
2. **Monocular Calibration Checker** - Live reprojection error monitor to validate monocular calibration quality.
3. **Stereo Calibration Tool** - Interactive calibration data acquisition and processing for stereo cameras and virtual stereo rigs.
4. **Stereo Calibration Checker** - Live reprojection error monitor to validate stereo calibration quality.
5. **Stereo Reprojection Viewer** - Real-time reprojection tool to visualize calibration results on unrectified images.

## Requirements

### Dependencies

- **ZED SDK** (version 5.1 or higher)
- **OpenCV** (4.x recommended) with the `aruco` contrib module for the AprilGrid target (tested with 4.5.4)
- **CUDA** (compatible with ZED SDK version)
- **OpenGL libraries**:
  - GLEW
  - FreeGLUT
  - OpenGL
- **CMake** (3.5 or higher)
- **C++17** compatible compiler

## Installation and building

### Build Instructions

Open a terminal on your Linux system and execute the following commands:

```bash
git clone https://github.com/stereolabs/zed-opencv-calibration.git
cd zed-opencv-calibration

# Build all tools
mkdir build && cd build
cmake ..
make -j$(nproc)
```

The AprilGrid support shared by the stereo tools lives in `common/aprilgrid/` (OpenCV only). Its unit
tests need no camera and are built on demand:

```bash
cmake .. -DBUILD_TESTS=ON
make -j$(nproc)
ctest --output-on-failure
```

## Usage

### Monocular Calibration

The **Monocular Calibration Tool** computes intrinsic camera parameters (focal length, principal point, distortion coefficients) for a single ZED X One camera using a checkerboard pattern.

#### Run the Monocular Calibration

```bash
cd build/monocular_calibration/
./zed_mono_calibration
```

This command opens the first connected ZED X One camera for live calibration using the default checkerboard settings.

```bash
Usage: ./zed_mono_calibration [options]
  --h_edges <value>      Number of horizontal inner edges of the checkerboard
  --v_edges <value>      Number of vertical inner edges of the checkerboard
  --square_size <value>  Size of a square in the checkerboard (in mm)
  --svo <file>           Path to the SVO file.
  --fisheye              Use fisheye lens model.
  --id <id>              Camera ID of the ZED X One.
  --sn <sn>              Serial number of the ZED X One.
  --help, -h             Show this help message.
```

#### Monocular Calibration Example Commands

- ZED X One using the default (first) camera:

  `./zed_mono_calibration`

- ZED X One selected by serial number:

  `./zed_mono_calibration --sn <serial_number>`

- ZED X One using an SVO file:

  `./zed_mono_calibration --svo <full_path_to_svo_file>`

- ZED X One with fisheye lens model and a custom checkerboard:

  `./zed_mono_calibration --fisheye --h_edges 12 --v_edges 9 --square_size 30.0`

>:pushpin: **Note**: You can obtain the serial number or ID of connected ZED X One cameras by running `ZED_Explorer --all`.

#### Monocular Calibration Process

The process follows the same two-phase approach as stereo calibration:

1. **Data Acquisition**: Move the checkerboard in front of the camera to capture diverse views. Press **Spacebar** or **S** to save a frame when the board is visible and sharp. The tool enforces quality checks (sharpness, sample diversity) and shows real-time X/Y coverage, size range, and skew scores.

2. **Calibration Computation**: Once sufficient data is collected (all metrics at 100% with the minimum number of samples, or the maximum sample count reached), the tool computes the calibration and saves two output files:

   - `mono_calibration_SN<serial_number>.yml`: Intrinsic parameters in OpenCV format (K matrix and distortion coefficients).
   - `SN<serial_number>_mono.conf`: Intrinsic parameters per resolution in ZED-compatible format, covering all supported output resolutions of the camera:
     - **ZED X One GS** (1920×1200): `CAM_FHD1200`, `CAM_FHD`, `CAM_SVGA`
     - **ZED X One 4K** (3840×2160): `CAM_4k`, `CAM_QHDPLUS`, `CAM_FHD`, `CAM_FHD1200`

You can use the OpenCV output file in your ZED SDK applications via the `sl::InitParametersOne::optional_opencv_calibration_file` parameter.

### Monocular Calibration Checker

The **Monocular Calibration Checker** validates an existing monocular calibration by measuring the reprojection error in real time. Point the ZED X One at a checkerboard and the tool continuously detects the corners, projects 3D object points back onto the image using the active calibration, and displays the RMS reprojection error overlaid on the live feed.

The error is color-coded for instant feedback:

- **Green** — below 0.5 px (good)
- **Orange** — 0.5–1.0 px (acceptable)
- **Red** — above 1.0 px (poor, recalibration recommended)

#### Run the Monocular Calibration Checker

```bash
cd build/monocular_checker/
./zed_mono_checker
```

By default the tool uses the camera's internal calibration. Use `--calib_opencv` to evaluate a custom file produced by the monocular calibration tool instead.

```bash
Usage: ./zed_mono_checker [options]
  --calib_opencv <file>  Path to an OpenCV YAML calibration file
                         (if omitted, the camera's internal calibration is used)
  --h_edges <value>      Number of horizontal inner edges of the checkerboard
  --v_edges <value>      Number of vertical inner edges of the checkerboard
  --square_size <value>  Size of a square in the checkerboard (in mm)
  --svo <file>           Path to the SVO file
  --fisheye              Use fisheye lens model
  --id <id>              Camera ID of the ZED X One
  --sn <sn>              Serial number of the ZED X One
  --help, -h             Show this help message
```

#### Monocular Checker Example Commands

- Check the internal calibration of the default camera:

  `./zed_mono_checker`

- Evaluate a calibration file produced by `zed_mono_calibration`:

  `./zed_mono_checker --calib_opencv mono_calibration_SN12345678.yml`

- Select a specific camera by serial number:

  `./zed_mono_checker --sn 12345678`

### Stereo Calibration

The **Stereo Calibration Tool** enables precise calibration of ZED stereo cameras and virtual stereo rigs (e.g., two ZED X One cameras) using a checkerboard pattern. This process computes intrinsic camera parameters (focal length, principal point, distortion coefficients) and extrinsic parameters (relative position and orientation between cameras). For single ZED X One cameras, use the [Monocular Calibration Tool](#monocular-calibration) instead.

#### Checkerboard Pattern Requirements

The calibration requires a printed checkerboard pattern with:

- **Default configuration**: [9x6 checkerboard with 25.4 mm squares](https://github.com/opencv/opencv/blob/4.x/doc/pattern.png/)
- **Custom patterns**: Supported via command-line options (see below)

**Important**: The pattern dimensions refer to the number of **inner corners** (where black and white squares meet), not the number of squares.

#### Prepare the Calibration Target

- Print the checkerboard pattern maximized and attach it on a rigid, flat surface.
- Ensure the pattern is perfectly flat and well-lit.
- Avoid reflections or glare on the checkerboard surface.

#### Run the Calibration

Default command to start calibration:

```bash
cd build/stereo_calibration/
./zed_stereo_calibration
```

This command tries to open the first connected ZED camera for live calibration using the default checkerboard settings.

You can also specify different options to calibrate virtual stereo cameras or use custom checkerboard parameters:

```bash

Usage: ./zed_stereo_calibration [options]
  --h_edges <value>      Number of horizontal inner edges of the checkerboard
  --v_edges <value>      Number of vertical inner edges of the checkerboard
  --square_size <value>  Size of a square in the checkerboard (in mm)
  --target_yaml <file>   Kalibr AprilGrid target file (replaces the checkerboard).
  --detector_params <file> OpenCV YAML file with AprilGrid detector parameters.
  --min_tags <value>     Minimum AprilGrid tags seen in both images (default 12).
  --min_rows <value>     Minimum tag rows seen in both images (default 2).
  --min_cols <value>     Minimum tag columns seen in both images (default 2).
  --min_samples <value>  Samples needed before the collection can end on complete metrics (default 25, at least 20).
  --max_samples <value>  Samples after which the collection pauses if metrics are incomplete (default 35, 0 = no maximum).
  --use_stored_values    Recompute the calibration from the images stored in 'zed-images/'.
  --camera_settings <file> YAML file with the capture settings to apply (exposure, gain, white balance...), restored on exit.
  --svo <file>           Path to the SVO file.
  --fisheye              Use fisheye lens model.
  --virtual              Use ZED X One cameras as a virtual stereo pair.
  --left_id <id>         Id of the left camera if using virtual stereo.
  --right_id <id>        Id of the right camera if using virtual stereo.
  --left_sn <sn>         S/N of the left camera if using virtual stereo.
  --right_sn <sn>        S/N of the right camera if using virtual stereo.
  --help, -h             Show this help message.
```

#### Stereo Calibration Example Commands

- ZED Stereo Camera using an SVO file:

  `./zed_stereo_calibration --svo <full_path_to_svo_file>`

- Virtual Stereo Camera using camera IDs:

  `./zed_stereo_calibration --virtual --left_id 0 --right_id 1`

- Virtual Stereo Camera using camera serial numbers and a custom checkerboard (size 12x9 with 30mm squares):

  `./zed_stereo_calibration --virtual --left_sn <serial_number> --right_sn <serial_number> --h_edges 12 --v_edges 9 --square_size 30.0`

- Virtual Stereo Camera with **fisheye lenses** using camera serial numbers:

  `./zed_stereo_calibration --fisheye --virtual --left_sn <serial_number> --right_sn <serial_number>`

>:pushpin: **Note**: You can easily obtain the serial numbers or the IDs of your connected ZED cameras by running the following command:
>
> ```bash
> ZED_Explorer --all
> ```

#### AprilGrid Target (Kalibr)

Instead of the checkerboard, the stereo tools accept a Kalibr AprilGrid (tag family `tag36h11`)
described by the same `target.yaml` used with Kalibr:

```yaml
target_type: 'aprilgrid' #gridtype
tagCols: 6               #number of apriltags
tagRows: 6               #number of apriltags
tagSize: 0.088           #size of apriltag, edge to edge [m]
tagSpacing: 0.3          #ratio of space between tags to tagSize
```

```bash
./zed_stereo_calibration --target_yaml target.yaml
```

- Each tag carries its own ID, so the target **does not need to be fully visible**: a sample is
  valid when at least 12 tags, spread over at least 2 rows and 2 columns, are seen in both images
  (`--min_tags`, `--min_rows`, `--min_cols`). Only the tags seen by both cameras are used for that
  pair. Push the target partially out of the frame to cover the image borders and corners.
- The detected tags are drawn with their IDs. With the board upright, tag 0 is bottom-left and the
  IDs grow left to right, then bottom to top (Kalibr convention).
- The tool prints the expected target dimensions at start-up. **Measure the printed target** (e.g.
  from the left edge of the first tag to the left edge of the last one in a row): a printer scale
  error does not show in the reprojection error, only in the baseline. Put the measured values in
  the target file if they differ.
- **Tilt the target**: at least half of the samples should be tilted by 30-45 deg, around both
  axes at the same time (one corner of the board closer to the camera). Mostly fronto-parallel
  samples leave the focal length poorly determined even with a low reprojection error. Aim for all
  four metrics at 100% before the maximum sample count is reached.
- Corners are refined on the X-junctions between the tags and the small Kalibr corner squares.
  The detection can be tuned without recompiling with `--detector_params <file.yml>`, an OpenCV
  YAML file using the field names of `cv::aruco::DetectorParameters` (unlisted fields keep the
  defaults; `markerBorderBits` is always 2, as for Kalibr tags):

  ```yaml
  %YAML:1.0
  ---
  adaptiveThreshConstant: 25
  minMarkerPerimeterRate: 0.02
  ```

- `--use_stored_values --target_yaml target.yaml` recomputes the calibration from the images saved
  in `zed-images/`, without the camera.

#### Camera Settings

By default the tools do not change any capture setting: exposure, gain and white balance stay as the
camera has them, usually automatic. Automatic exposure keeps adapting during the collection, and a
long exposure blurs a moving target. To keep the capture conditions constant, pass a settings file:

```bash
./zed_stereo_calibration --target_yaml target.yaml --camera_settings ../../config/camera_settings_zed2i.yaml
./zed_stereo_checker     --target_yaml target.yaml --camera_settings ../../config/camera_settings_zed2i.yaml
```

- The file uses the `sl::VIDEO_SETTINGS` names (`EXPOSURE`, `GAIN`, `AEC_AGC`, `SHARPNESS`,
  `WHITEBALANCE_TEMPERATURE`...), one `NAME: value` per line. Only the listed settings are applied.
  [`config/camera_settings_zed2i.yaml`](config/camera_settings_zed2i.yaml) is a commented starting
  point with the range and the effect of each setting.
- The tool stops **before collecting** if a setting is not supported by the connected camera, is out
  of its range, is read-only, or contradicts another one (e.g. `AEC_AGC: 1` together with `EXPOSURE`).
  Each value is read back after being applied.
- The settings in use are printed at start-up, with or without a file, and again at the end of the
  collection, so `... | tee calib.log` records the capture conditions of the calibration.
- The previous values are **restored on exit**: end of the calibration, `q`, error or Ctrl+C. The next
  program using the camera (ZED Explorer, the ROS wrapper) does not inherit the manual settings.
- Use the same file for the calibration and the checker. It has no effect with `--use_stored_values`,
  which does not open the camera: the stored images do not record the settings, the capture log does.

#### Traceable Capture (`--session`)

For calibrations that must be compared, repeated or reprocessed later (with other models, or with a
refractive model under water), a session file makes every capture self-contained:

```bash
./zed_stereo_calibration --target_yaml target.yaml --camera_settings ../../config/camera_settings_zed2i.yaml \
                         --session my_session.yaml
```

- Start from [`config/session_example.yaml`](config/session_example.yaml). It names the output root,
  the **condition** (e.g. `A_ar_sem_acrilico`) and the **capture number**, and records the lens, the
  target dimensions measured with a caliper, the flat port in front of the camera and the medium.
  Every field is mandatory; write `null` for what is unknown or does not apply. Quote texts that start
  with a digit, such as dates and the lens (`lens: "2.1mm"`).
- Everything goes to `<output_root>/<condition>/captura_<XX>/`:

  | File | Content |
  |---|---|
  | `left/<pose_id>_<n>.png`, `right/...` | the raw pairs (unrectified, lossless) |
  | `detections.json` | per camera, **every** detected tag, with the corners before and after the refinement |
  | `meta.json` | camera, SDK, code commit, host, the session, the capture settings at start and end, and per pair the image timestamp, the host uptime and the sensor temperatures |
  | `session.yaml`, `capture.log` | the session file used and the whole terminal output |
  | `calib_cpp_<model>.yml` | the calibration, in the file the ZED SDK reads, written only if accepted |
  | `calib_cpp_<model>.report.json` | how it was computed and how well the saved parameters fit (see below) |

- **Nothing is ever deleted or overwritten.** The tool refuses to start if the capture folder already
  exists and is not empty: use the next capture number. `zed-images/` is not used.
- The session mode requires the AprilGrid (`--target_yaml`) and records a new live capture, so it
  cannot be combined with `--use_stored_values`.
- `detections.json` and `meta.json` are plain JSON (`schema_version: 1`), read by the offline analysis.
  The temperatures are `null` when the camera sensors cannot be reached (see the permissions in
  `.ai/docs/bench.md`).
- **Calibration of a session capture.** The optional `calibration` section of the session file sets
  the model (`rational8` or `fisheye4`), the acceptance RMS, the outlier rejection factor and floor,
  and the prior. The calibration starts **from scratch** unless `prior: factory` is set, so that it
  can be compared with the factory calibration as an independent one. The steps are the same as
  without `--session` (mono, per-view rejection, stereo with the intrinsics refined jointly,
  per-view rejection).
- **The report tells which RMS is which.** All values are OpenCV's per-corner 2D RMS
  (`rms_convention: per_corner_2d`), with the per-coordinate MSE (RMS²/2) next to them:

  | Field | Meaning |
  |---|---|
  | `stereo` | the RMS of the final stereo optimisation |
  | `stereo_fit_left/right` | each camera with the **saved** parameters and the poses of the stereo fit; used for the acceptance |
  | `free_pose_left/right` | each camera with the saved intrinsics and one free pose per view |
  | `mono_stage_left/right` | the mono calibration before the stereo fit: what the tool prints as Left/Right without `--session` |

  It also lists, per view, the corners used, the stereo fit error and whether the view was removed
  (and at which stage), plus the standard deviations of the last mono calibration. OpenCV gives none
  for the stereo fit.
- `--recalibrate <captura_XX>` calibrates an existing session capture again without the camera, with
  the `calibration` section of its `session.yaml` (with `prior: factory`, the exported factory
  calibration of `zed_export_factory` is the prior). Existing results are never overwritten.
- Older captures can be brought to the same format without the camera:
  `--use_stored_values --detections_out detections.json` writes the detections of `zed-images/`
  (checkerboard or AprilGrid) with the detector the calibration uses; an existing file is never
  overwritten.

#### Pose Script (`roteiro`)

With `roteiro: <file>` in the session (e.g. [`config/roteiro_v1.yaml`](config/roteiro_v1.yaml),
63 poses), the capture follows a fixed list of poses with ids such as `B1-P-TL`, `B2-M-MC-Hp35` or
`B3-P-TL-R090` (block, distance `P`/`M`/`L`, position on the 3×3 image grid, variation). The same
file and ids serve every capture and condition.

- The expected target is drawn on the left image; a panel on the right one shows the pose, the
  measured quantities (green inside the tolerance, red outside), the stillness and the target
  sharpness.
- The quantities come from a PnP pose with the exported factory calibration (or `pose_calibration`
  in the session). They only guide the operator; they never enter the calibration. The angles are
  defined in [`common/pose_script/pose_script.hpp`](common/pose_script/pose_script.hpp): the tilt
  about the horizontal axis is positive when the top edge is farther, the tilt about the vertical
  axis when the right edge is farther, and the rotation is the image angle of the target X axis,
  clockwise, in [0, 360). The width fraction is that of the **whole** target, projected.
- Keys: `s` saves only when every quantity is in tolerance, the minimum of tags is met and the
  target is still and sharp (the `static_capture` section); `o` saves a pose that is not feasible,
  flagged `out_of_tolerance`; `k` skips the pose; `b` goes back one pose without deleting
  anything (the new pair gets the next `n`). At the end the tool lists the saved, out-of-tolerance
  and skipped poses, and calibrates if there are enough pairs.
- The dissimilarity filter is off in this mode, and the X/Y/Size/Skew metrics are not updated: the
  coverage is checked offline (`zedcal.coverage_command`).
- `meta.json` records, per pair, the measured pose, the flag and the still window with its
  thresholds, and every operator action of the script.
- [`config/roteiro_metrico_v1.yaml`](config/roteiro_metrico_v1.yaml) is the metric test script
  (36 poses at 0.5, 1, 2 and 3 m). Both files are generated by
  [`config/make_roteiro_v1.py`](config/make_roteiro_v1.py): edit the generator, not the files.

#### Other Session Modes

| Option | What it does |
|---|---|
| `--measure_static_noise <s>` | with the target still, records the corner displacement between frames and the target sharpness for `<s>` seconds into `noise.json`, with the `static_capture` thresholds they suggest; saves no pair |
| `--timelapse <interval_s> <duration_min>` | warm-up record: one pair per interval with `pose_id` `thermal`, without `s`, then stops without calibrating (analysis: `zedcal.thermal_command`) |

`--svo <file>` reads a recorded SVO instead of the camera, with the same open parameters; it cannot
be combined with `--session`, `--timelapse` or `--camera_settings`, which describe a live camera.

#### Offline Analysis (`analysis/`)

The Python package `zedcal` reads the captures (`detections.json`, `meta.json`) and the calibration
files. Run it in the container, from `analysis/`, as `python3 -m zedcal.<command> --config
<file.yaml>`; each command documents its configuration in its docstring (`--help`), copies it to
its output folder and never writes over an existing one. Tests: `python3 -m pytest` (the slow
statistical checks with `ZEDCAL_SLOW=1`).

| Command | Purpose |
|---|---|
| `calibrate` | calibration of a capture in Python (same steps as the C++ tool, more variants) |
| `coverage_command` | corners per cell of an 8×8 grid and tilt histograms; exit 0 when the capture passes, 1 when it must be captured again |
| `m1_command` | fit metrics: RMS conventions, residual maps, Bias Ratio |
| `m2_command` | mapping error between calibrations, the image-space reference and the empirical EME |
| `m3_command` | leave-one-capture-out matrix and cross application between conditions |
| `m4_command` | task metrics: epipolar error after rectification, triangulated metric error, plane residual, disparity bias between calibrations |
| `report_command` | statistics of one condition, or the comparison of two (permutation, TOST, dispersion), in Markdown |
| `metric_test_command` | metric test at known distances, with the common offset d0 |
| `thermal_command` | warm-up curves and the proposed warm-up time |

Specs, results and decisions are in `.ai/specs/`; the bench notes in `.ai/docs/bench.md`.

#### Calibration Process

The calibration process consists of two main phases:

1. **Data Acquisition**: Move the checkerboard in front of the camera(s) to capture diverse views. The tool provides real-time feedback on the quality of the captured data.
2. **Calibration Computation**: Once sufficient data is collected, the tool computes the calibration parameters and saves them to two files.

The **Data Acquisition** phase consists of moving the checkerboard in front of the camera(s) to capture diverse views. The tool provides real-time feedback on the quality of the captured data regarding *XY coverage*, *distance variation*, and *skewness*.

When the checkerboard is placed in a position that you want to capture, press the **Spacebar** or the **S** key to capture the images.

- If the checkerboard is detected in the image(s), and the captured data are different enough from the previously captured images, the data is accepted, and the quality indicators are updated.
- If the data is not accepted, a message is displayed in the GUI output indicating the reason (e.g., checkerboard not detected, not enough variation, etc.).

The blue dots that appear on the left image indicate the center of each checkerboard that has been detected and accepted so far. The size of the dots indicates the relative size of the checkerboard in the image (bigger dots mean closer to the camera).

In order to collect good calibration data, ensure that:

- The checkerboard is always fully visible in both left and right images. Corners detected in both images are highlighted with colored visual markers.
- The checkerboard moves over a wide area of the image frame. "Green" polygons appear on the left image to indicate the covered areas. When one of the 4 zones of the left image becomes fully green, the coverage requirement is met for that part of the image.
- Red areas on the side of the left frame indicate zones that are not yet covered by the checkerboard. Try to make them as small as possible.
- The checkerboard is moved closer and farther from the camera to ensure depth variation. At least one image covering almost the full left frame is required.
- The checkerboard is tilted and rotated to provide different angles.

The "X", "Y", "Size", and "Skew" percentages indicate the quality of the collected data for each criterion.

For **X** and **Y**, the minimum and maximum values correspond to the minimum and maximum position of the corner of the checkerboard closest to the image border. The COVERAGE indicates the size of the horizontal and vertical area covered by the checkerboard corners in the left image. The higher the coverage, the more the image is covered.

For **Size**, the minimum and maximum values correspond to the smallest and largest size of the checkerboard in the left image. The COVERAGE indicates the range of sizes of the checkerboard in the collected samples. A higher coverage means that the checkerboard was captured at a wider range of distances from the camera.

For **Skew**, the minimum and maximum values correspond to the minimum and maximum skewness angle of the checkerboard in the left image. The COVERAGE indicates the range of skew angles of the checkerboard in the collected samples. A higher coverage means that the checkerboard was captured at a wider range of angles. A value of 0° means the checkerboard is perfectly fronto-parallel to the camera, a theoretical maximum of 90° means the checkerboard is seen edge-on. Normally, the maximum achievable skew is around 40°.

If you cannot reach 100% for one of the metrics, be sure that it's as high as possible, and move the checkerboard in different positions to maximize the coverage by acquiring the maximum number of samples.

Here are some tips to improve each metric:

- To raise the "X" and "Y" metrics move the checkerboard to the edges and corners of the left image while keeping it fully visible in the right frame.
- To raise the "Size" metric, move the checkerboard closer and farther from the camera. You must acquire at least one image where the checkerboard is covering almost the full left image and one where it's smaller and corners are barely detected [see the image below].
- To raise the "Skew" metric, rotate the checkerboard in different angles. It's easier to obtain different skew values if the checkerboard is closer to the camera and rotated around the vertical and horizontal axes simultaneously.

The "Calibrate" process will automatically start when all metrics reach 100% and the minimum number
of samples (`--min_samples`, default 25) is collected.

If the maximum number of samples (`--max_samples`, default 35) is reached while some metrics are still
below 100%, the collection **pauses** instead of calibrating. The live view keeps running and a banner
lists the incomplete metrics:

- **`s`** or **spacebar**: continue collecting, without a maximum. The current frame is not saved:
  move the target and press `s` again to capture.
- **`f`**: calibrate now with the samples collected so far.
- **`q`** or **ESC**: abort.

The **`f`** key also ends the collection at any time, as soon as at least 20 samples (the fewest the
calibration accepts) are saved. Ending with incomplete metrics is allowed but reported: incomplete
metrics usually mean a poorly determined calibration (for instance, too few tilted samples leave the
focal length uncertain even with a low reprojection error).

To keep collecting until all metrics are complete, without a maximum:

```bash
./zed_stereo_calibration --max_samples 0
```

The terminal reports how the collection ended, the number of samples and the final score of each
metric (`>>> Collection ended: ...`), so that `./zed_stereo_calibration ... | tee calib.log` keeps a
record of it. `--min_samples` and `--max_samples` have no effect with `--use_stored_values`.

![Calibration GUI](./images/calibration_gui.png)

For each metric, the GUI shows the following information in a table:

- **MIN_VAL**: Minimum value stored in all the samples collected so far.
- **MAX_VAL**: Maximum value stored in all the samples collected so far.
- **COVERAGE**: The difference between the MIN_VAL and MAX_VAL, representing the range of variation in the collected samples.
- **REQUIRED**: The minimum required value for the COVERAGE to consider the metric as satisfied.
- **SCORE**: The percentage score for the metric, calculated as (COVERAGE / REQUIRED) * 100%.

You can follow the steps of the calibration process in the terminal output:

1. The left camera is calibrated first, followed by the right camera to obtain the intrinsic parameters.
2. Finally, the stereo calibration is performed to compute the extrinsic parameters (rotation and translation) between the two cameras.

Good calibration results typically yield a reprojection error below 0.5 pixels for each calibration step.

If any reprojection error is too high, the calibration is not accurate enough and should be redone. Before recalibrating, verify the following:

- The checkerboard is perfectly flat and securely mounted.
- The checkerboard is well-lit with even, stable lighting.
- Camera lenses are clean and free of smudges or dust.
- No reflections or glare appear on the checkerboard surface.

After a good calibration is complete, two files are generated:

- `zed_calibration_<serial_number>.yml`: Contains intrinsic and extrinsic parameters for the stereo camera setup in OpenCV format.
- `SN<serial_number>.conf`: Contains the calibration parameters in ZED SDK format.

You can use these files in your ZED SDK applications:

- [Use the `sl::InitParameters::optional_opencv_calibration_file` parameter to load the calibration from the OpenCV file](https://www.stereolabs.com/docs/api/structsl_1_1InitParameters.html#a9eab2753374ef3baec1d31960859ba19).
- Manually copy the `SN<serial_number>.conf` file to the ZED SDK calibration folder to make the ZED SDK automatically use it:

  - Linux: `/usr/local/zed/settings/`
  - Windows: `C:\ProgramData\Stereolabs\settings`
- [Use the `sl::InitParameters::optional_settings_path` to indicate to the ZED SDK where to find the custom `SN<serial_number>.conf` calibration file](https://www.stereolabs.com/docs/api/structsl_1_1InitParameters.html#aa8262e36d2d4872410f982a735b92294).

>:pushpin: **Note**: When calibrating a virtual ZED X One stereo rig, the serial number of the Virtual Stereo Camera is generated by the ZED SDK using the serial numbers of the two individual cameras. Make sure to use this generated serial number when loading the calibration in your application to have a unique identifier for the virtual stereo setup.

### Factory Calibration Export

`zed_export_factory` writes the factory calibration of the connected camera ("F") in the same
OpenCV file the calibration tool writes, so that it can be evaluated with exactly the same tools
and metrics as any other calibration. F is a reference to compare with, not the truth.

```bash
./zed_export_factory --output_root /workspaces/isaac_ros-dev/dev/calib_runs/dados --verify --expect_resolution 1280x720
```

- The camera is opened as `zed_stereo_calibration` opens it (same resolution and frame rate, SDK
  self-calibration disabled), and `calibration_parameters_raw` (unrectified) is converted with the
  same code that writes the tool's own calibrations.
- Output in `<output_root>/fabrica/SN<serial>/`: `calib_factory_rational8.yml`, `meta.json`, a copy of
  the SDK file `SN<serial>.conf` and `export.log`. An existing folder is never overwritten.
- `--verify` reopens the camera with the written file as `optional_opencv_calibration_file` and checks
  that the SDK reads back the same parameters (1e-4 relative). `--verify_file <file.yml>` runs only
  that check, for any calibration file.
- The tool stops if the factory distortion has thin prism terms, which the 8-coefficient file
  cannot hold, or if `--expect_resolution` does not match the opened resolution.

### Stereo Calibration Checker

The **Stereo Calibration Checker** validates an existing stereo calibration by measuring three reprojection errors in real time. Point the stereo camera at a checkerboard and the tool continuously detects corners in both images, then reports:

- **Left reproj.** — RMS reprojection error on the left image (tests left camera intrinsics).
- **Right reproj.** — RMS reprojection error on the right image (tests right camera intrinsics).
- **Stereo reproj.** — Combined RMS error using the stereo constraint: the board pose from the left image is propagated via the calibrated rotation and translation to predict right-image corners. This specifically tests the extrinsic calibration (R, T).

All three errors are color-coded and displayed live on the images:

- **Green** — below 0.5 px (good)
- **Orange** — 0.5–1.0 px (acceptable)
- **Red** — above 1.0 px (poor, recalibration recommended)

#### Run the Stereo Calibration Checker

```bash
cd build/stereo_checker/
./zed_stereo_checker
```

By default the tool uses the camera's internal calibration. Use `--calib_opencv` to evaluate a custom file produced by the stereo calibration tool instead.

```bash
Usage: ./zed_stereo_checker [options]
  --calib_opencv <file>  Path to an OpenCV YAML calibration file
                         (if omitted, the camera's internal calibration is used)
  --h_edges <value>      Number of horizontal inner edges of the checkerboard
  --v_edges <value>      Number of vertical inner edges of the checkerboard
  --square_size <value>  Size of a square in the checkerboard (in mm)
  --target_yaml <file>   Kalibr AprilGrid target file (replaces the checkerboard)
  --detector_params <file> OpenCV YAML file with AprilGrid detector parameters
  --camera_settings <file> YAML file with the capture settings to apply, restored on exit (see Camera Settings)
  --min_tags <value>     Minimum number of AprilGrid tags for a valid view (default 12)
  --min_rows <value>     Minimum number of tag rows for a valid view (default 2)
  --min_cols <value>     Minimum number of tag columns for a valid view (default 2)
  --svo <file>           Path to the SVO file
  --fisheye              Use fisheye lens model
  --virtual              Use ZED X One cameras as a virtual stereo pair
  --left_id <id>         Id of the left camera if using virtual stereo
  --right_id <id>        Id of the right camera if using virtual stereo
  --left_sn <sn>         S/N of the left camera if using virtual stereo
  --right_sn <sn>        S/N of the right camera if using virtual stereo
  --help, -h             Show this help message
```

With an AprilGrid (`--target_yaml`), the Left and Right errors use all the tags seen by each camera,
and the Stereo error needs the minimum number of tags in both images. Views below the minimum show
`N/A`.

#### Stereo Checker Example Commands

- Check the internal calibration of the default stereo camera:

  `./zed_stereo_checker`

- Evaluate a calibration file produced by `zed_stereo_calibration`:

  `./zed_stereo_checker --calib_opencv zed_calibration_SN12345678.yml`

- Check the internal calibration with a Kalibr AprilGrid:

  `./zed_stereo_checker --target_yaml target.yaml`

- Virtual stereo rig using camera serial numbers:

  `./zed_stereo_checker --virtual --left_sn <serial_number> --right_sn <serial_number> --calib_opencv SN<virtual_sn>.conf`

### Stereo Reprojection Viewer

The **Stereo Reprojection Viewer** is a diagnostic tool that visualizes the effects of camera calibration in real-time. It helps validate calibration quality by displaying how 3D point cloud data reprojects onto unrectified images.

The tool opens the stereo camera and loads calibration parameters either from a specified file or from the default ZED SDK calibration location.

#### What the Viewer Displays

The application provides three synchronized views:

1. **3D Point Cloud** - The computed depth data in 3D space
2. **Rectified Left Image** - The corrected, distortion-free left camera image
3. **Unrectified Left Image with Reprojection Overlay** - The raw left camera image overlaid with reprojected 3D points color-coded by depth (blue for close, red for far)

#### Evaluating Calibration Quality

By comparing the rectified and unrectified views with the reprojection overlay, you can:

- **Assess distortion correction**: Verify how well the calibration parameters remove lens distortions
- **Compare field of view**: Evaluate differences in effective FOV between rectified and unrectified images
- **Validate depth accuracy**: Ensure 3D points align correctly with their corresponding image features

#### Virtual Stereo Camera Alignment

For virtual stereo camera setups (e.g., two ZED X One cameras), the reprojection view provides critical alignment feedback:

- **Centered reprojection zone**: Indicates proper optical axis alignment between the two cameras with minimal vertical offset
- **Minimal uncovered areas**: Suggests good lens matching and overlapping fields of view between cameras

#### Run the Reprojection Viewer

Default command to start the reprojection viewer:

```bash
cd build/stereo_reprojection_viewer
./zed_reprojection_viewer [options]
```

This command tries to open the first connected ZED stereo camera for live reprojection viewing.

You can also specify different options to use virtual stereo cameras, fisheye lenses, or an SVO file:

```bash
Usage: ./zed_reprojection_viewer [options]
  --svo <file>          Path to the SVO file.
  --calib_path <file>   Path to the optional calibration file
  --ocv <file>          Path to an optional OpenCV calibration file
  --fisheye             Use fisheye lens model.
  --virtual             Use ZED X One cameras as a virtual stereo pair.
  --left_id <id>        Id of the left camera if using virtual stereo.
  --right_id <id>       Id of the right camera if using virtual stereo.
  --left_sn <sn>        S/N of the left camera if using virtual stereo.
  --right_sn <sn>       S/N of the right camera if using virtual stereo.
  --help, -h            Show this help message.
```

#### Stereo Reprojection Viewer Example Commands

- ZED Stereo Camera using an SVO file:

  `./zed_reprojection_viewer --svo <full_path_to_svo_file>`

- Virtual Stereo Camera using camera IDs:

  `./zed_reprojection_viewer --virtual --left_id 0 --right_id 1`

- Virtual Stereo Camera with fisheye lenses using camera serial numbers:

  `./zed_reprojection_viewer --fisheye --virtual --left_sn <serial_number> --right_sn <serial_number>`
