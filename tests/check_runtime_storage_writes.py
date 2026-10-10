#!/usr/bin/env python3
"""Fail CI if the runtime source adds direct persistent-storage access.

This is a narrowly scoped source audit, NOT proof that Swiss/firmware won't
create files. It deliberately scans only the source files linked into the two
console binaries, and should be revised if legitimate storage is added.
"""
from pathlib import Path
import re
import sys

paths = (sorted(Path("src").glob("*.c")) +
         sorted(Path("gba-diagnostic").glob("*.c")) +
         sorted(Path("protocol").glob("*.c")))
patterns = {
    "GameCube memory card API": r"\bCARD_(?:Init|Mount|Create|Write|Format|Delete|Open|Read|Close)\s*\(",
    "filesystem write API": r"\b(?:fwrite|fprintf|fputs|fputc|remove|rename|mkdir|unlink)\s*\(",
    "filesystem write mode": r"\bfopen\s*\([^;]*,\s*\"[wax+]",
    "GBA save hardware register write": r"\b(?:FLASH|SRAM|EEPROM)_(?:Write|Erase|Program)\s*\(",
}
errors = []
for path in paths:
    content = path.read_text(encoding="utf-8")
    for name, pattern in patterns.items():
        for match in re.finditer(pattern, content, flags=re.MULTILINE):
            line = content.count("\n", 0, match.start()) + 1
            errors.append(f"{path}:{line}: prohibited direct storage operation ({name})")
if errors:
    print("\n".join(errors), file=sys.stderr)
    sys.exit(1)
print(f"Runtime write API guard passed ({len(paths)} source files audited).")
