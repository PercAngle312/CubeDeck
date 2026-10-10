"""Guard the hardware-tested Joybus RESET -> STATUS startup contract.

The optional packet ACK experiment must not get stranded in RESET. This
test intentionally checks the callback-driven transition at source level;
cross-compilation alone cannot catch a missing state-machine edge.
"""
from pathlib import Path
import re

source = Path("experiments/v2-probe/gamecube-ack.c").read_text()
match = re.search(
    r"else\s+if\s*\(joybus_stage\s*==\s*1\)\s*\{(?P<body>.*?)\}\s*else\s+if\s*\(joybus_stage\s*==\s*2\)",
    source,
    re.S,
)
assert match is not None, "RESET callback dispatch edge is missing"
body = match.group("body")
assert re.search(r"joybus_stage\s*=\s*2\s*;", body), "RESET does not advance to STATUS"
assert re.search(r"start_joybus_transfer\s*\(\s*0x00\s*,\s*3\s*\)\s*;", body), (
    "RESET does not issue STATUS Joybus transfer"
)
assert re.search(r"if\s*\(joybus_stage\s*==\s*1\)", source)
assert "joybus_stage = 1;" in source, "RESET stage must be reachable"
print("ACK-probe Joybus RESET -> STATUS callback edge is present.")
