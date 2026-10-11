#!/usr/bin/env python3
"""Prepare an isolated Dolphin 2407 integrated-GBA test profile.

Local-only preflight. Never packages, prints, uploads, or copies the GBA BIOS.
This does NOT start the emulator or claim that any Joybus transfer passed.
"""
import argparse
import configparser
import hashlib
from pathlib import Path
import shutil
import sys


def file_hash(path):
    digest = hashlib.sha256()
    with path.open("rb") as handle:
        for chunk in iter(lambda: handle.read(65536), b""):
            digest.update(chunk)
    return digest.hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--bios", required=True, type=Path)
    parser.add_argument("--dol", required=True, type=Path)
    parser.add_argument("--gba", required=True, type=Path)
    parser.add_argument("--profile", required=True, type=Path)
    parser.add_argument("--expect-dol-sha256", help="Expected DOL SHA-256 from the matching CI artifact")
    parser.add_argument("--expect-gba-sha256", help="Expected GBA SHA-256 from the same CI run")
    args = parser.parse_args()

    for label, path in (("BIOS", args.bios), ("DOL", args.dol), ("GBA", args.gba)):
        if not path.is_file():
            parser.error(f"{label} input does not exist: {path}")
    if args.bios.stat().st_size != 16384:
        parser.error("BIOS must be exactly 16384 bytes")
    if args.dol.stat().st_size == 0 or args.gba.stat().st_size == 0:
        parser.error("DOL/GBA must not be empty")

    # Reject mismatched or mixed-run artifacts BEFORE creating the profile.
    expected = (args.expect_dol_sha256, args.expect_gba_sha256)
    if any(expected) and not all(expected):
        parser.error("Provide both expected artifact SHA-256 values together")
    if all(expected):
        for label, path, digest in (("DOL", args.dol, expected[0]), ("GBA", args.gba, expected[1])):
            if len(digest) != 64 or any(ch not in "0123456789abcdefABCDEF" for ch in digest):
                parser.error(f"{label} expected SHA-256 must contain 64 hexadecimal characters")
            if file_hash(path) != digest.lower():
                parser.error(f"{label} SHA-256 mismatch: refusing to prepare profile")

    # Refuse source, artifact, and input overlap. Only a new disposable profile.
    profile = args.profile.resolve()
    sources = (args.bios.resolve(), args.dol.resolve(), args.gba.resolve())
    if any(profile == source or profile in source.parents for source in sources):
        parser.error("Profile must not contain the supplied BIOS or binaries")
    ini = profile / "Config" / "Dolphin.ini"
    if profile.exists() and any(profile.iterdir()):
        parser.error("Profile must be absent or empty to avoid overwriting saved data")
    ini.parent.mkdir(parents=True, exist_ok=True)

    cfg = configparser.ConfigParser(interpolation=None)
    cfg.optionxform = str
    cfg["Core"] = {
        "SIDevice0": "6", "SIDevice1": "13", "SIDevice2": "0",
        "SIDevice3": "0", "SlotA": "255", "SlotB": "255",
        "SerialPort1": "255", "GFXBackend": "Software", "CPUThread": "False",
    }
    cfg["DSP"] = {"Backend": "NoAudio"}
    cfg["GBA"] = {
        "BIOS": str(sources[0]), "Rom2": str(sources[2]),
        "Threads": "False", "SavesInRomPath": "False",
    }
    with ini.open("w", encoding="utf-8") as output:
        cfg.write(output)
    # Dolphin.ini contains absolute local paths and must stay out of published artifacts.
    ini.chmod(0o600)
    print(f"Isolated profile created: {profile}")
    print(f"DOL SHA256: {file_hash(args.dol)}")
    print(f"GBA SHA256: {file_hash(args.gba)}")
    print("BIOS: local 16384-byte input validated (digest intentionally not logged)")
    dolphin = shutil.which("dolphin-emu")
    if dolphin:
        print(f"Launch manually: {dolphin} -u {profile} -e {sources[1]} -b")
    else:
        print("Dolphin executable absent; emulator execution NOT performed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
