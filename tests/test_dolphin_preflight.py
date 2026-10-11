#!/usr/bin/env python3
"""Smoke-test the Dolphin profile preflight with wholly synthetic inputs."""
from pathlib import Path
import configparser
import subprocess
import sys
import tempfile

SCRIPT = Path(__file__).resolve().parents[1] / "experiments/v2-probe/prepare_dolphin_profile.py"

def invoke(bios, dol, gba, profile):
    return subprocess.run(
        [sys.executable, str(SCRIPT), "--bios", str(bios), "--dol", str(dol),
         "--gba", str(gba), "--profile", str(profile)],
        capture_output=True, text=True, check=False,
    )

with tempfile.TemporaryDirectory(prefix="cubedeck-preflight-") as root:
    base = Path(root)
    bios = base / "synthetic-bios.bin"
    dol = base / "synthetic.dol"
    gba = base / "synthetic.gba"
    bios.write_bytes(b"X" * 16384)  # NOT a Nintendo BIOS
    dol.write_bytes(b"dummy dol")
    gba.write_bytes(b"dummy gba")
    profile = base / "isolated"
    ok = invoke(bios, dol, gba, profile)
    assert ok.returncode == 0, ok.stderr
    assert "BIOS SHA256" not in ok.stdout and "DOL SHA256" in ok.stdout
    ini = profile / "Config/Dolphin.ini"
    cfg = configparser.ConfigParser()
    cfg.read(ini)
    assert cfg["Core"]["SIDevice1"] == "13"
    assert cfg["Core"]["SlotA"] == "255"
    assert cfg["GBA"]["Rom2"] == str(gba.resolve())
    assert cfg["GBA"]["BIOS"] == str(bios.resolve())
    assert not (ini.stat().st_mode & 0o077)
    assert invoke(bios, dol, gba, profile).returncode != 0  # no overwrite

    short_bios = base / "invalid-bios.bin"
    short_bios.write_bytes(b"X" * 1024)
    assert invoke(short_bios, dol, gba, base / "bad").returncode != 0
    assert not (base / "bad").exists()
    assert invoke(bios, dol, gba, base).returncode != 0
    assert invoke(bios, dol, gba, base / "missing").returncode == 0

print("Synthetic Dolphin preflight: config, isolation and refusal cases passed.")
