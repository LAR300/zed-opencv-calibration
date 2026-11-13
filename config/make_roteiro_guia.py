"""Writes the pose data of config/roteiro_v1_guia.html from the pose script and the target file.

Run: python3 config/make_roteiro_guia.py [roteiro.yaml] [target.yaml] [guia.html]
(defaults: config/roteiro_v1.yaml, config/aprilgrid_6x6_88mm.yaml, config/roteiro_v1_guia.html).

The page is a printable checklist with a drawing of every pose. It guides a capture in a session
without the script mode (roteiro: null), where the tool does not check the pose, and helps
repeat the same poses in every capture. Only the JSON block of the page is rewritten.
No PyYAML: the script file has one flow mapping per pose, as config/make_roteiro_v1.py writes it.
"""
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
NUM = r"(-?[\d.]+)"
POSE = re.compile(
    r"^\s*-\s*\{id:\s*([\w-]+),\s*block:\s*(\d+),\s*fraction:\s*\[%s,\s*%s\],\s*"
    r"center:\s*\[%s,\s*%s\],\s*tilt_h_deg:\s*%s,\s*tilt_v_deg:\s*%s,\s*rotation_deg:\s*%s\}\s*$"
    % ((NUM,) * 7))
DATA_BLOCK = re.compile(r'(<script id="roteiro-data" type="application/json">)(.*?)(</script>)', re.S)


def read_script(path):
    name, tolerances, poses, entries = None, {}, [], 0
    with open(path) as f:
        for line in f:
            if line.startswith("name:"):
                name = line.split(":", 1)[1].strip()
            elif line.startswith("tolerances:"):
                for key, value in re.findall(r"(\w+):\s*([\d.]+)", line.split(":", 1)[1]):
                    tolerances[key] = float(value)
            elif line.lstrip().startswith("- {"):
                entries += 1
                m = POSE.match(line)
                if not m:
                    raise ValueError("%s: pose line not understood: %s" % (path, line.strip()))
                g = m.groups()
                poses.append({"id": g[0], "block": int(g[1]),
                              "fraction": [float(g[2]), float(g[3])],
                              "center": [float(g[4]), float(g[5])],
                              "tilt_h_deg": float(g[6]), "tilt_v_deg": float(g[7]),
                              "rotation_deg": float(g[8])})
    if name is None or not poses or len(poses) != entries:
        raise ValueError("%s: no name or no poses" % path)
    return name, tolerances, poses


def read_target(path):
    values = {}
    with open(path) as f:
        for line in f:
            m = re.match(r"^(\w+):\s*'?([\w.]+)'?\s*$", line)
            if m:
                values[m.group(1)] = m.group(2)
    if values.get("target_type") != "aprilgrid":
        raise ValueError("%s: only an AprilGrid target is drawn" % path)
    return {"tagCols": int(values["tagCols"]), "tagRows": int(values["tagRows"]),
            "tagSize": float(values["tagSize"]), "tagSpacing": float(values["tagSpacing"]),
            "file": os.path.basename(path)}


def main(argv):
    script = argv[1] if len(argv) > 1 else os.path.join(HERE, "roteiro_v1.yaml")
    target = argv[2] if len(argv) > 2 else os.path.join(HERE, "aprilgrid_6x6_88mm.yaml")
    guide = argv[3] if len(argv) > 3 else os.path.join(HERE, "roteiro_v1_guia.html")
    name, tolerances, poses = read_script(script)
    data = {"name": name, "source": os.path.basename(script), "tolerances": tolerances,
            "image": [1280, 720], "target": read_target(target), "poses": poses}
    with open(guide) as f:
        html = f.read()
    if len(DATA_BLOCK.findall(html)) != 1:
        raise ValueError("%s: expected one roteiro-data block" % guide)
    payload = "\n" + json.dumps(data, ensure_ascii=False, separators=(",", ":")) + "\n"
    html = DATA_BLOCK.sub(lambda m: m.group(1) + payload + m.group(3), html)
    with open(guide, "w") as f:
        f.write(html)
    print("%s: %d poses from %s, target %s" % (guide, len(poses), script, target))
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
