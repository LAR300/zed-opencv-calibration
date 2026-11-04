"""The package must never detect corners: they come from the C++ detector (risk 6 of the
audit; Hagemann 2022, pitfall 4: the detector noise must be the calibration's)."""
import os
import re

PACKAGE = os.path.join(os.path.dirname(__file__), "..", "zedcal")
FORBIDDEN = [r"\baruco\b", r"findChessboardCorners", r"detectMarkers", r"findCirclesGrid"]


def test_package_has_no_corner_detection():
    offenders = []
    for root, _, files in os.walk(PACKAGE):
        for name in files:
            if not name.endswith(".py"):
                continue
            path = os.path.join(root, name)
            text = open(path).read()
            for pattern in FORBIDDEN:
                if re.search(pattern, text):
                    offenders.append("%s: %s" % (os.path.relpath(path, PACKAGE), pattern))
    assert offenders == [], offenders


def test_the_check_itself_finds_a_forbidden_call(tmp_path):
    sample = "import cv2\ncv2.aruco.detectMarkers(img, d)\n"
    assert any(re.search(p, sample) for p in FORBIDDEN)
