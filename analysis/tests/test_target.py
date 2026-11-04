import numpy as np
import pytest

from zedcal.target import Target


def kalibr():
    return Target(type="aprilgrid", rows=6, cols=6, tag_size_mm=88.0, tag_spacing_ratio=0.3)


# Expected values of tests/test_geometry.cpp (C++): corners BR, BL, TL, TR
def test_tag0_corners():
    np.testing.assert_allclose(kalibr().tag_corners(0),
                               [[88, 0, 0], [0, 0, 0], [0, 88, 0], [88, 88, 0]], atol=1e-9)


def test_tag5_is_last_of_first_row():
    c = kalibr().tag_corners(5)
    np.testing.assert_allclose(c[1], [572, 0, 0], atol=1e-9)
    np.testing.assert_allclose(c[3], [660, 88, 0], atol=1e-9)


def test_tag30_is_first_of_last_row():
    c = kalibr().tag_corners(30)
    np.testing.assert_allclose(c[1], [0, 572, 0], atol=1e-9)
    np.testing.assert_allclose(c[3], [88, 660, 0], atol=1e-9)


def test_tag35_reaches_board_corner():
    np.testing.assert_allclose(kalibr().tag_corners(35)[3], [660, 660, 0], atol=1e-9)


def test_ids_outside_the_grid_are_refused():
    with pytest.raises(ValueError):
        kalibr().tag_corners(36)
    with pytest.raises(ValueError):
        kalibr().tag_corners(-1)


def test_checkerboard_points_follow_calibrate():
    # calibrate(): rows (v_edges) outer, columns (h_edges) inner, (square*j, square*i, 0)
    t = Target(type="checkerboard", rows=6, cols=9, square_size_mm=23.8)
    p = t.checkerboard_points()
    assert p.shape == (54, 3)
    np.testing.assert_allclose(p[0], [0, 0, 0])
    np.testing.assert_allclose(p[1], [23.8, 0, 0])
    np.testing.assert_allclose(p[9], [0, 23.8, 0])
    np.testing.assert_allclose(p[53], [8 * 23.8, 5 * 23.8, 0])


def test_measured_dimensions_override_the_nominal():
    t = kalibr().with_measured(tag_size_mm=87.9, tag_spacing_mm=26.5)
    np.testing.assert_allclose(t.tag_corners(1)[1], [87.9 + 26.5, 0, 0], atol=1e-9)
