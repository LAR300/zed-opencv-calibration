import numpy as np
import pytest

from zedcal import calib, io, m2, synthetic
from zedcal.residuals import Camera, left_camera

SIZE = (1280, 720)
K = np.array([[534.0, 0, 632.0], [0, 534.5, 369.0], [0, 0, 1]])
D = np.array([-0.05, 0.03, 0.0003, -0.0002, -0.01, 0.0, 0.0, 0.0])


def test_grid_covers_the_borders():
    g = m2.pixel_grid(SIZE, 20)
    assert g[:, 0].min() == 0 and g[:, 0].max() == 1279 and g[:, 1].max() == 719


def test_identical_calibrations_give_zero():
    for rotation in (False, True):
        e = m2.mapping_error(Camera(K, D), Camera(K, D), SIZE, rotation=rotation)
        assert e.rms_per_point_px < 1e-9 and e.excluded == 0


def test_fx_perturbation_gives_the_expected_shift():
    delta = 1e-3
    kb = K.copy()
    kb[0, 0] *= 1 + delta
    e = m2.mapping_error(Camera(K, np.zeros(8)), Camera(kb, np.zeros(8)), SIZE)
    expected = np.abs(e.grid[:, 0] - K[0, 2]) * delta
    assert np.max(np.abs(e.errors_px - expected)) < 1e-6
    assert abs(e.rms_per_point_px - np.sqrt(np.mean(expected ** 2))) < 1e-6
    assert abs(e.k_per_coord_px2 - np.mean(expected ** 2) / 2) < 1e-9


def test_cx_shift_and_the_rotation_variant():
    kb = K.copy()
    kb[0, 2] += 1.0
    plain = m2.mapping_error(Camera(K, D), Camera(kb, D), SIZE)
    assert abs(plain.rms_per_point_px - 1.0) < 1e-6
    rot = m2.mapping_error(Camera(K, D), Camera(kb, D), SIZE, rotation=True)
    assert rot.rms_per_point_px < plain.rms_per_point_px
    r = rot.rotation_deg
    # Mostly about the vertical axis, with the order of magnitude of atan(1/fx)
    assert abs(r[1]) > 10 * max(abs(r[0]), abs(r[2]))
    assert 0.3 < abs(np.radians(r[1])) / np.arctan(1 / K[0, 0]) < 3


def test_diverging_unprojection_is_excluded_and_counted():
    wild = Camera(K, np.array([0.9, 0.9, 0, 0, 0.9, 0, 0, 0]))
    e = m2.mapping_error(wild, wild, SIZE)
    assert e.excluded > 0
    assert np.isnan(e.errors_px).sum() == e.excluded
    # the excluded points are at the image border
    border = e.grid[np.isnan(e.errors_px)]
    assert np.all((np.abs(border[:, 0] - 640) > 300) | (np.abs(border[:, 1] - 360) > 200))


def test_percentile():
    assert m2.percentile_of(5.0, [1, 2, 3, 4, 6, 7, 8, 9, 10, 11]) == 40.0
    assert m2.percentile_of(0.5, [1, 2]) == 0.0


@pytest.fixture(scope="module")
def ten_noisy(tmp_path_factory):
    tmp = tmp_path_factory.mktemp("m2")
    rig = synthetic.zed_like_rig()
    out, corners = [], []
    for s in range(10):
        path = str(tmp / ("d%d.json" % s))
        synthetic.write(path, synthetic.generate(rig, synthetic.random_poses(22, np.random.default_rng(300 + s)),
                                                 sigma_px=0.3, rng=np.random.default_rng(400 + s)))
        det = io.load_detections(path)
        out.append(calib.calibrate(det, calib.CalibrationOptions(rejection="none")).calibration)
        corners += [np.concatenate(list(f.left.values())) for f in det.frames]
    region = m2.coverage_mask(m2.pixel_grid(SIZE), np.concatenate(corners))
    return rig, out, region


def test_reference_is_closer_to_the_truth_than_each_where_there_is_data(ten_noisy):
    # Measured on 2026-11-03: covered region, reference 0.144 px vs 0.222-0.726 px for each;
    # full image, the uncovered side strips dominate with extrapolation (ref 1.52, each 0.83-2.12)
    rig, calibs, region = ten_noisy
    ref, simple, fit = m2.reference_calibration(calibs)
    to_truth = lambda c: m2.mapping_error(left_camera(c), left_camera(rig), SIZE,
                                          region=region).rms_per_point_px
    each = [to_truth(c) for c in calibs]
    assert to_truth(ref) < min(each), (to_truth(ref), each)
    assert fit["fit_rms_left_px"] < 0.05
    assert region.mean() < 1.0  # the synthetic poses leave part of the image uncovered


def test_coverage_mask():
    grid = m2.pixel_grid(SIZE, 20)
    mask = m2.coverage_mask(grid, np.array([[10.0, 10.0]]))
    assert mask[0] and mask.sum() == np.sum(np.linalg.norm(grid - [10, 10], axis=1) <= 40)


def test_reference_of_copies_is_the_copy(ten_noisy):
    _, calibs, _ = ten_noisy
    ref, _, _ = m2.reference_calibration([calibs[0]] * 5)
    e = m2.mapping_error(left_camera(ref), left_camera(calibs[0]), SIZE)
    assert e.rms_per_point_px < 1e-6


def test_empirical_eme_and_pair_matrix(ten_noisy):
    _, calibs, _ = ten_noisy
    ref, _, _ = m2.reference_calibration(calibs)
    eme = m2.empirical_eme(calibs, ref, "left")
    assert len(eme["errors_px"]) == 10
    assert abs(eme["eme_px"] - np.sqrt(np.mean(np.square(eme["errors_px"])))) < 1e-12
    m = m2.pair_matrix(calibs[:4], "left")
    assert m.shape == (4, 4) and np.all(np.diag(m) == 0) and np.all(m[~np.eye(4, dtype=bool)] > 0)
    # 10 calibrations give 45 distinct pairs
    assert len(np.triu_indices(10, 1)[0]) == 45


def test_m2_command(tmp_path, ten_noisy):
    import os
    import yaml
    from zedcal import m2_command
    from zedcal.calib_file import write_calibration
    rig, calibs, _ = ten_noisy
    paths = []
    for i, c in enumerate(calibs[:4]):
        os.makedirs(tmp_path / ("captura_%02d" % i))
        p = str(tmp_path / ("captura_%02d" % i) / "calib.yml")
        write_calibration(p, c)
        paths.append(p)
    write_calibration(str(tmp_path / "truth.yml"), rig)
    cfg = {"calibrations": paths, "factory": str(tmp_path / "truth.yml"), "output_dir": str(tmp_path / "m2")}
    yaml.safe_dump(cfg, open(tmp_path / "m2.yaml", "w"))
    assert m2_command.main(["--config", str(tmp_path / "m2.yaml")]) == 0
    out = sorted(os.listdir(tmp_path / "m2"))
    assert "m2.json" in out and "calib_reference.yml" in out and "m2_factory_left_full_image.png" in out
