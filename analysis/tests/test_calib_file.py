import cv2
import numpy as np
import pytest

from zedcal.calib_file import StereoCalibration, read_calibration, write_calibration


def sample(model="rational8"):
    n = {"rational8": 8, "radtan5": 5, "fisheye4": 4}[model]
    K = np.array([[533.82, 0, 632.115], [0, 533.845, 369.065], [0, 0, 1.0]])
    D = np.linspace(-0.4, 0.3, n)
    R, _ = cv2.Rodrigues(np.array([0.00134, -0.00025, 0.0021]))
    return StereoCalibration(image_size=(1280, 720), model=model, K_left=K, D_left=D,
                             K_right=K * 0.999 + np.diag([0, 0, 0.001]), D_right=D * 1.01,
                             R=R, T=np.array([-119.982, -0.234, 0.078]))


@pytest.mark.parametrize("model", ["rational8", "radtan5", "fisheye4"])
def test_round_trip(tmp_path, model):
    c = sample(model)
    path = str(tmp_path / "calib.yml")
    write_calibration(path, c)
    r = read_calibration(path)
    assert r.model == model and r.image_size == (1280, 720)
    for a, b in ((r.K_left, c.K_left), (r.D_right, c.D_right), (r.R, c.R), (r.T, c.T)):
        np.testing.assert_allclose(a, b, rtol=0, atol=1e-12)


def test_written_keys_are_the_sdk_ones(tmp_path):
    path = str(tmp_path / "calib.yml")
    write_calibration(path, sample())
    fs = cv2.FileStorage(path, cv2.FILE_STORAGE_READ)
    for key in ("Size", "K_LEFT", "K_RIGHT", "D_LEFT", "D_RIGHT", "R", "T"):
        assert not fs.getNode(key).empty(), key
    assert fs.getNode("R").mat().shape == (3, 1)  # Rodrigues vector
    assert fs.getNode("D_LEFT").mat().shape == (1, 8)
    assert fs.getNode("Size").isSeq()  # the SDK refuses a 2x1 matrix
    assert open(path).read().count("Size: [ 1280, 720 ]") == 1


def test_fisheye_uses_the_fe_keys(tmp_path):
    path = str(tmp_path / "calib.yml")
    write_calibration(path, sample("fisheye4"))
    fs = cv2.FileStorage(path, cv2.FILE_STORAGE_READ)
    assert not fs.getNode("D_LEFT_FE").empty() and fs.getNode("D_LEFT").empty()


def test_reads_14_coefficients_as_rational8(tmp_path):
    path = str(tmp_path / "calib14.yml")
    fs = cv2.FileStorage(path, cv2.FILE_STORAGE_WRITE)
    fs.write("Size", (1280, 720))
    fs.write("K_LEFT", np.eye(3)); fs.write("K_RIGHT", np.eye(3))
    D = np.zeros((1, 14)); D[0, 0] = 0.1
    fs.write("D_LEFT", D); fs.write("D_RIGHT", D)
    fs.write("R", np.zeros((3, 1))); fs.write("T", np.array([[-120.0], [0], [0]]))
    fs.release()
    c = read_calibration(path)
    assert c.model == "rational8" and c.D_left.shape == (8,)


def test_refuses_terms_beyond_the_eighth(tmp_path):
    path = str(tmp_path / "prism.yml")
    fs = cv2.FileStorage(path, cv2.FILE_STORAGE_WRITE)
    fs.write("Size", (1280, 720))
    fs.write("K_LEFT", np.eye(3)); fs.write("K_RIGHT", np.eye(3))
    D = np.zeros((1, 14)); D[0, 9] = 0.01
    fs.write("D_LEFT", D); fs.write("D_RIGHT", D)
    fs.write("R", np.zeros((3, 1))); fs.write("T", np.array([[-120.0], [0], [0]]))
    fs.release()
    with pytest.raises(ValueError, match="beyond the 8th"):
        read_calibration(path)


def test_reads_the_cpp_file():
    # Written by zed_stereo_calibration --recalibrate (bench data, if present)
    path = ("/workspaces/isaac_ros-dev/dev/calib_runs/testes_persistencia/recalib_cs/captura_01/"
            "calib_cpp_rational8.yml")
    try:
        c = read_calibration(path)
    except OSError:
        pytest.skip("bench data not available")
    assert c.model == "rational8" and abs(c.T[0] + 120) < 2
