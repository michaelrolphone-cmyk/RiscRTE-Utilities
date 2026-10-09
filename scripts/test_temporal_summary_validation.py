#!/usr/bin/env python3
"""Frozen-baseline temporal summary/validator equivalence, normal and sanitized."""
import os
from pathlib import Path
import shlex
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "tests/temporal_summary_validation_test.c"


def main():
    compiler = shlex.split(os.environ.get("CC", "cc"))
    with tempfile.TemporaryDirectory(prefix="temporal-summary-validation-") as tmp:
        for sanitized in (False, True):
            name = "asan-ubsan" if sanitized else "normal"
            executable = Path(tmp) / name
            flags = ["-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror"]
            if sanitized:
                flags += ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                          "-fno-omit-frame-pointer", "-no-pie"]
            subprocess.run(compiler + flags + [str(SOURCE), "-o", str(executable)],
                           check=True, timeout=120)
            environment = dict(os.environ)
            # LeakSanitizer is unsupported in some test sandboxes; retain ASan
            # and UBSan's bounds, use-after-free, and undefined-behavior checks.
            options = environment.get("ASAN_OPTIONS", "")
            environment["ASAN_OPTIONS"] = options + (":" if options else "") + "detect_leaks=0"
            print(f"Running temporal summary differential ({name})", flush=True)
            subprocess.run([str(executable)], check=True, env=environment, timeout=120)


if __name__ == "__main__":
    main()
