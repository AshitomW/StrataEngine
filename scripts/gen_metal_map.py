import json
import subprocess
import sys

spirv_cross, spv_file, out_map = sys.argv[1], sys.argv[2], sys.argv[3]
raw = subprocess.check_output([spirv_cross, spv_file, "--reflect"])
data = json.loads(raw)
lines = []
for item in data.get("textures", []) + data.get("images", []):
    lines.append(f"{item['name']} {item['binding']}\n")
with open(out_map, "w") as f:
    f.writelines(lines)

