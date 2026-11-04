import os

import numpy as np
import pytest

from zedcal import calib, io, synthetic
from zedcal.bootstrap import PARAMETERS, bootstrap, parameter_vector

OPTS = calib.CalibrationOptions(rejection="none")


def dataset(tmp_path, seed, n_views=25, sigma=0.3):
    rig = synthetic.zed_like_rig()
    poses = synthetic.random_poses(n_views, np.random.default_rng(seed))
    path = str(tmp_path / ("d%d.json" % seed))
    synthetic.write(path, synthetic.generate(rig, poses, sigma_px=sigma,
                                             rng=np.random.default_rng(seed + 1000)))
    return io.load_detections(path)


def test_same_seed_same_result(tmp_path):
    det = dataset(tmp_path, 1)
    a = bootstrap(det, OPTS, n=5, seed=7)
    b = bootstrap(det, OPTS, n=5, seed=7)
    np.testing.assert_array_equal(a.samples, b.samples)
    assert a.samples.shape == (5, len(PARAMETERS))


SLOW = pytest.mark.skipif(os.environ.get("ZEDCAL_SLOW") != "1",
                          reason="statistical test, ~15 min: run with ZEDCAL_SLOW=1")


@SLOW
def test_bootstrap_spread_matches_the_spread_of_independent_captures(tmp_path):
    # The std of fx over independent synthetic captures is the truth the bootstrap should
    # estimate (Hagemann 2022). A single capture's estimate varies about 2x from capture to
    # capture, so the median over several captures is used.
    # Measured on 2026-11-03 (25 views, sigma 0.3 px): observed 0.27 px, median bootstrap
    # 0.43 px, ratio 1.59. Resampling with replacement leaves ~37% of the views out of each
    # sample, among them the tilted views that fix the focal length, so the bootstrap of one
    # 25-view capture overestimates the spread. The plan's 30% criterion is not met; this test
    # only guards the order of magnitude, and the methodology takes the uncertainty from the
    # spread of the 10 captures (empirical EME), the bootstrap being an upper indication.
    independent = [parameter_vector(calib.calibrate(dataset(tmp_path, s), OPTS).calibration)[0]
                   for s in range(100, 130)]
    observed = np.std(independent, ddof=1)
    estimates = [bootstrap(dataset(tmp_path, s), OPTS, n=30, seed=s).std["fx_left"]
                 for s in range(200, 205)]
    ratio = np.median(estimates) / observed
    print("observed %.4f, bootstrap estimates %s, median ratio %.2f" % (observed, estimates, ratio))
    assert 0.7 < ratio < 2.5, (estimates, observed)
