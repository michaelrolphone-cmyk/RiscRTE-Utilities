#!/usr/bin/env python3
"""Check selected and historical Points/editor ABI dispatch; no service/device claim."""
import os
from pathlib import Path
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]

def main():
    with tempfile.TemporaryDirectory(prefix="points-editor-abi-") as directory:
        for selected in (False, True):
            for sanitized in (False, True):
                flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
                if selected:
                    flags.append("-DALARM_SERVICE_CLOCK_EDIT_V2")
                if sanitized:
                    flags.extend(["-fsanitize=address,undefined", "-fno-sanitize-recover=all"])
                target = Path(directory) / f"descriptor-{selected}-{sanitized}"
                command = [os.environ.get("CC", "cc"), *flags,
                           "-I" + str(ROOT / "lib/Alarm/include"),
                           str(ROOT / "tests/points_clock_edit_descriptor_test.c"), "-o", str(target)]
                print(" ".join(command), flush=True)
                subprocess.run(command, check=True)
                subprocess.run([str(target)], check=True,
                               env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0"})

if __name__ == "__main__":
    main()
