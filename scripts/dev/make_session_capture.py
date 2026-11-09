"""Bench helper: turn an old run (zed-images/) into a session capture folder.

Copies the pairs to left/free_<n>.png and right/free_<n>.png, rewrites the legacy
detections.json (from --detections_out) for the new file names, and writes session.yaml
and a minimal meta.json (serial only). The original run is only read.

Usage: python3 -I make_session_capture.py <run_dir> <legacy_detections.json> <output_root>
           <condition> <capture> <prior> <serial> <session_example.yaml>
"""
import json
import os
import re
import shutil
import sys

run, legacy, root, condition, capture, prior, serial, example = sys.argv[1:9]
capture = int(capture)
out = os.path.join(root, condition, "captura_%02d" % capture)
if os.path.exists(out) and os.listdir(out):
    sys.exit("refusing: %s is not empty" % out)
os.makedirs(os.path.join(out, "left"))
os.makedirs(os.path.join(out, "right"))

detections = json.load(open(legacy))
for frame in detections["frames"]:
    n = frame["n"]
    for side in ("left", "right"):
        src = os.path.join(run, "zed-images", "image_%s_%d.png" % (side, n))
        rel = "%s/free_%d.png" % (side, n)
        shutil.copyfile(src, os.path.join(out, rel))
        frame["files"][side] = rel
    frame["pose_id"] = "free"
json.dump(detections, open(os.path.join(out, "detections.json"), "w"), indent=2)

session = open(example).read()
session = re.sub(r"(?m)^output_root: .*$", "output_root: " + root, session)
session = re.sub(r"(?m)^condition: .*$", "condition: " + condition, session)
session = re.sub(r"(?m)^capture: .*$", "capture: %d" % capture, session)
session = re.sub(r"(?m)^  prior: .*$", "  prior: " + prior, session)
open(os.path.join(out, "session.yaml"), "w").write(session)
json.dump({"schema_version": 1, "camera": {"serial": int(serial)}},
          open(os.path.join(out, "meta.json"), "w"))
print(out, len(detections["frames"]), "pairs")
