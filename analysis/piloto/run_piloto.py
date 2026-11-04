"""Pilot of the metrics on the existing captures (spec piloto-dados-existentes).

Run in the container, as admin:  cd analysis && python3 piloto/run_piloto.py <calib_runs>/piloto
Expects <piloto>/<capture>/detections.json (from --detections_out) and <piloto>/fabrica/SN<sn>/.
Writes <piloto>/resultados/: calibrations, M1 and M2 outputs, figures and summary.json.
Nothing outside <piloto>/resultados/ is written; it refuses to run if that folder exists.
"""
import glob
import json
import os
import sys
from dataclasses import replace

import numpy as np

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))

from zedcal import calib, io, m1, m2  # noqa: E402
from zedcal.calib_file import read_calibration, write_calibration  # noqa: E402
from zedcal.residuals import left_camera, pose, residuals, right_camera  # noqa: E402

CAPTURES = {
    "xadrez": "2026-10-16_ar_9x6",
    "v2": "2026-10-20_ar_aprilgrid_v2",
    "acrilico": "2026-10-23_acrilico_ar_aprilgrid",
    "controles": "2026-11-03_camera_settings",
}
APRILGRID = ["v2", "acrilico", "controles"]
VARIANTS = {
    "compat": dict(mono_points="common", stereo="joint", rejection="cpp"),
    "all_joint": dict(mono_points="all", stereo="joint", rejection="cpp"),
    "all_fixed": dict(mono_points="all", stereo="fixed", rejection="cpp"),
    "all_joint_norej": dict(mono_points="all", stereo="joint", rejection="none"),
}
# The C++ calibration each compat run should reproduce: from scratch where it exists
CPP_FILES = {
    "xadrez": "recalib/zed_calibration_0.yml",
    "v2": "recalib/zed_calibration_0.yml",
    "acrilico": "zed_calibration_35180697.yml",
    "controles": "zed_calibration_35180697.yml",
}


def rel(a, b):
    a, b = np.asarray(a, float), np.asarray(b, float)
    return float(np.max(np.abs(a - b) / np.maximum(np.abs(b), 1e-9)))


def fy_fx(c):
    return {"left_pct": 100 * (c.K_left[1, 1] / c.K_left[0, 0] - 1),
            "right_pct": 100 * (c.K_right[1, 1] / c.K_right[0, 0] - 1)}


def anisotropy(det, cam, side):
    """Regress the free-pose residuals on the position: dx = a (x - cx) + ..., dy = b (y - cy) + ...
    a - b is the relative x/y scale the data ask for beyond the calibration (a fy/fx signal)."""
    xs, ys, dxs, dys = [], [], [], []
    for f in det.frames:
        try:
            obj, img, _ = io.mono_correspondences(f, side, det.target)
        except ValueError:
            continue
        if len(obj) < 48:
            continue
        rv, tv = pose(obj, img, cam)
        r = residuals(obj, img, rv, tv, cam)
        xs.append(img[:, 0] - cam.K[0, 2]); ys.append(img[:, 1] - cam.K[1, 2])
        dxs.append(r[:, 0]); dys.append(r[:, 1])
    x, y, dx, dy = (np.concatenate(v) for v in (xs, ys, dxs, dys))
    a = np.polyfit(x, dx, 1)[0]
    b = np.polyfit(y, dy, 1)[0]
    return {"scale_x_pct": 100 * a, "scale_y_pct": 100 * b, "y_minus_x_pct": 100 * (b - a)}


def border_corners(det, side, points, cells=(8, 8)):
    """Corners in the outer ring of cells: what each camera brings to the image borders."""
    w, h = det.image_size
    total = 0
    for f in det.frames:
        try:
            if points == "all":
                _, img, _ = io.mono_correspondences(f, side, det.target)
            else:
                _, il, ir, _ = io.common_correspondences(f, det.target)
                img = il if side == "left" else ir
        except ValueError:
            continue
        cx = np.clip((img[:, 0] / w * cells[0]).astype(int), 0, cells[0] - 1)
        cy = np.clip((img[:, 1] / h * cells[1]).astype(int), 0, cells[1] - 1)
        total += int(np.sum((cx == 0) | (cy == 0) | (cx == cells[0] - 1) | (cy == cells[1] - 1)))
    return total


def m1_summary(det, c):
    out = {"rms": m1.rms_report(det, c)["rms_px"]}
    for side, cam in (("left", left_camera(c)), ("right", right_camera(c))):
        fit = m1.camera_fit(det, cam, side, "common")
        b = m1.bias_ratio(fit, cam, det.target, m1.N_INTRINSICS[c.model])
        out["br_" + side] = {k: b.as_dict()[k] for k in ("br", "eps_bias_px", "sigma_d_px",
                                                          "virtual_targets", "virtual_discarded")}
        rmap = m1.residual_map(np.concatenate(fit.image_points), fit.all_residuals, det.image_size)
        out["map_" + side] = rmap
    return out


def main(piloto):
    out = os.path.join(piloto, "resultados")
    if os.path.exists(out):
        sys.exit("refusing: %s exists" % out)
    os.makedirs(out)
    dets = {k: io.load_detections(os.path.join(piloto, v, "detections.json")) for k, v in CAPTURES.items()}
    factory = read_calibration(glob.glob(os.path.join(piloto, "fabrica", "SN*", "calib_factory_*.yml"))[0])
    summary = {"captures": CAPTURES, "variants": VARIANTS, "calibrations": {}, "m1": {}, "fy_fx": {},
               "anisotropy_with_F": {}, "border_corners": {}, "cpp_parity": {}}

    # 1. Calibrations (R2)
    calibs = {}
    for name, det in dets.items():
        os.makedirs(os.path.join(out, name))
        for vname, v in VARIANTS.items():
            r = calib.calibrate(det, calib.CalibrationOptions(**v))
            calibs[(name, vname)] = r.calibration
            calib.save(r, os.path.join(out, name), "calib_%s" % vname)
            summary["calibrations"]["%s/%s" % (name, vname)] = {
                "rms": r.report["rms_px"], "pairs_used": r.report["pairs_used"],
                "pairs_input": r.report["pairs_input"], "baseline_mm": r.report["baseline_mm"],
                "removed": [(e["pose_id"], e["n"], e["removed"]["stage"]) for e in r.report["views"] if e["removed"]]}
            summary["fy_fx"]["%s/%s" % (name, vname)] = fy_fx(r.calibration)
            print("calibrated %s/%s" % (name, vname), flush=True)
        cpp = read_calibration(os.path.join(os.path.dirname(os.path.abspath(piloto)), CAPTURES[name],
                                            CPP_FILES[name]))
        c = calibs[(name, "compat")]
        summary["cpp_parity"][name] = {"file": CPP_FILES[name], "K_rel": max(rel(c.K_left, cpp.K_left), rel(c.K_right, cpp.K_right)),
                                       "T_rel": rel(c.T, cpp.T), "D_abs": float(np.max(np.abs(c.D_left - cpp.D_left)))}
    summary["fy_fx"]["fabrica"] = fy_fx(factory)

    # 2. M1 of every calibration on its own capture, and of F on every capture (R3)
    maps = {}
    for name, det in dets.items():
        for vname in VARIANTS:
            s = m1_summary(det, calibs[(name, vname)])
            maps[(name, vname)] = s.pop("map_left"), s.pop("map_right")
            summary["m1"]["%s/%s" % (name, vname)] = s
        s = m1_summary(det, factory)
        maps[(name, "F")] = s.pop("map_left"), s.pop("map_right")
        summary["m1"]["%s/F" % name] = s
        for side, cam in (("left", left_camera(factory)), ("right", right_camera(factory))):
            summary["anisotropy_with_F"]["%s/%s" % (name, side)] = anisotropy(det, cam, side)
            summary["border_corners"]["%s/%s" % (name, side)] = {
                "common": border_corners(det, side, "common"), "all": border_corners(det, side, "all")}
        print("M1 %s" % name, flush=True)
    for (name, vname), (ml, mr) in maps.items():
        if vname in ("compat", "F"):
            m1.plot_residual_map(ml, "%s, %s, left" % (name, vname), os.path.join(out, name, "m1_%s_left.png" % vname))
        summary.setdefault("residual_rows", {})["%s/%s" % (name, vname)] = {
            "mean_y_by_row_left": np.nanmean(ml["mean_y_px"], axis=1).tolist(),
            "rms_border_vs_centre_left": [float(np.nanmean(np.concatenate([ml["rms_px"][0], ml["rms_px"][-1],
                                                                           ml["rms_px"][:, 0], ml["rms_px"][:, -1]]))),
                                          float(np.nanmean(ml["rms_px"][2:6, 2:6]))]}

    # 3. M2 per variant on the AprilGrid captures, the three references (R3, R4.6)
    grid = m2.pixel_grid(factory.image_size)
    corners = np.concatenate([np.concatenate(list(f.left.values())) for n in APRILGRID for f in dets[n].frames if f.left])
    region = m2.coverage_mask(grid, corners)
    summary["m2"] = {}
    for vname in VARIANTS:
        cs = [calibs[(n, vname)] for n in APRILGRID]
        ref, simple, fitres = m2.reference_calibration(cs)
        entry = {"reference_fit": fitres}
        for rot in (False, True):
            key = "rotation" if rot else "plain"
            eme = m2.empirical_eme(cs, ref, "left", rotation=rot, region=region)
            f_err = m2.mapping_error(left_camera(factory), left_camera(ref), factory.image_size, rotation=rot, region=region)
            entry[key] = {"errors_px": eme["errors_px"], "eme_px": eme["eme_px"],
                          "factory_px": f_err.rms_per_point_px,
                          "simple_mean_vs_reference_px": m2.mapping_error(left_camera(simple), left_camera(ref),
                                                                          factory.image_size, rotation=rot,
                                                                          region=region).rms_per_point_px}
        summary["m2"][vname] = entry
        if vname == "compat":
            write_calibration(os.path.join(out, "calib_reference_compat.yml"), ref)
    # pooled calibration of the three AprilGrid captures, compat variant
    pooled_det = replace(dets["v2"], frames=[f for n in APRILGRID for f in dets[n].frames])
    pooled = calib.calibrate(pooled_det, calib.CalibrationOptions(**VARIANTS["compat"])).calibration
    ref_compat = read_calibration(os.path.join(out, "calib_reference_compat.yml"))
    summary["m2"]["pooled_vs_reference_compat_px"] = m2.mapping_error(
        left_camera(pooled), left_camera(ref_compat), factory.image_size, region=region).rms_per_point_px
    summary["fy_fx"]["pooled_compat"] = fy_fx(pooled)
    # variants against each other, per capture (question 5)
    summary["variant_vs_compat_px"] = {
        "%s/%s" % (n, v): m2.mapping_error(left_camera(calibs[(n, v)]), left_camera(calibs[(n, "compat")]),
                                           factory.image_size, region=region).rms_per_point_px
        for n in CAPTURES for v in VARIANTS if v != "compat"}

    with open(os.path.join(out, "summary.json"), "w") as f:
        json.dump(summary, f, indent=2, default=lambda o: o.tolist() if isinstance(o, np.ndarray) else str(o))
    print("written", os.path.join(out, "summary.json"))


if __name__ == "__main__":
    main(sys.argv[1])
