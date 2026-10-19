#!/usr/bin/env python3
"""Run actual-source Viewer/Canvas integration on native panel geometries.

Writes bounded normal-UI PGM samples, per-case logs, and a test-results JSON.
The JSON records observed tests and source hashes; it is not a release receipt.
Use --viewer-source with a git-show copy to reproduce old app regressions.
"""
import argparse
from concurrent.futures import ThreadPoolExecutor, as_completed
import hashlib
import json
import os
from pathlib import Path
import shlex
import subprocess
import tempfile
import time

from build_canvas_apps import ROOT, stage

GEOMETRIES = ["240x240:rgb", "410x502:rgb", "160x160:rgb", "320x180:rgb",
              "502x410:rgb", "480x800:mono", "800x480:mono"]
CASES = ["ui", "pixels", "toolbar", "layout", "transform", "sd", "sd-legacy",
         "submit-retry", "status-unknown", "present-failed", "present-timeout",
         "invalid-surface", "missing-token", "unsubscribe-retained",
         "foreground-retained", "release-retained", "file-close-retained",
         "data-retained"]


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def commit(path):
    return subprocess.check_output(["git", "rev-parse", "HEAD"], cwd=path,
                                   text=True).strip()


def geometry(value):
    try:
        dimensions, fmt = value.split(":")
        w, h = (int(n) for n in dimensions.split("x"))
        if not (160 <= w <= 1200 and 160 <= h <= 1200 and fmt in ("rgb", "mono")):
            raise ValueError
        return value, w, h, fmt
    except ValueError as exc:
        raise argparse.ArgumentTypeError("use WIDTHxHEIGHT:rgb or WIDTHxHEIGHT:mono") from exc


def main():
    p = argparse.ArgumentParser(description=__doc__)
    for name in ("runtime", "system-apps", "output"):
        p.add_argument("--" + name, type=Path, required=True)
    p.add_argument("--geometry", action="append", type=geometry)
    p.add_argument("--case", action="append", choices=CASES + ["header", "status", "message"])
    p.add_argument("--sanitizers", choices=("both", "none", "address"), default="both",
                   help="address combines AddressSanitizer and UndefinedBehaviorSanitizer")
    p.add_argument("--viewer-source", type=Path, default=ROOT / "Apps/model_viewer.c")
    p.add_argument("--jobs", type=int, default=2)
    p.add_argument("--no-samples", action="store_true")
    p.add_argument("--leak-sanitizer", action="store_true",
                   help="enable LSan only where supported; fixture allocation ownership is always checked")
    p.add_argument("--cc", default=os.environ.get("CC", "cc"))
    a = p.parse_args()
    if a.jobs < 1:
        p.error("--jobs must be positive")
    a.output = a.output.resolve()
    a.output.mkdir(parents=True, exist_ok=True)
    app = a.viewer_source.resolve()
    sources = [app, ROOT / "lib/CanvasApp/canvas.c", ROOT / "lib/CanvasApp/entry.c",
               ROOT / "test/canvas/viewer_native_test.c", Path(__file__).resolve(),
               *sorted((ROOT / "Apps").glob("model_viewer_*.h"))]
    before = {str(s): digest(s) for s in sources}
    variants = ["normal", "asan-ubsan"] if a.sanitizers == "both" else [
        "normal" if a.sanitizers == "none" else "asan-ubsan"]
    results = {"schema": 1, "kind": "viewer-native-test-results",
               "repositories": {"utilities": commit(ROOT), "system": commit(a.system_apps),
                                "runtime": commit(a.runtime)},
               "sources_sha256": before, "leak_sanitizer": a.leak_sanitizer,
               "allocation_ownership_checks": True, "builds": [], "tests": []}
    jobs = []
    with tempfile.TemporaryDirectory(prefix="viewer-native-sdk-") as temp:
        include = stage(a.runtime.resolve(), a.system_apps.resolve(), Path(temp))
        results["sdk_headers_sha256"] = {
            f.name: digest(f) for f in sorted(include.glob("*.h"))}
        for variant in variants:
            exe = a.output / ("viewer-native-" + variant)
            command = [*shlex.split(a.cc), "-std=c11", "-g", "-DCANVAS_KIND=1",
                       "-I" + str(include), "-I" + str(ROOT / "Apps"),
                       '-DVIEWER_NATIVE_APP_SOURCE="' + str(app) + '"']
            if "static float mv_view_scale(" in app.read_text():
                command.append("-DFX_NATIVE_HAS_VIEW_SCALE=1")
            command += ["-O2"] if variant == "normal" else [
                "-O1", "-fsanitize=address,undefined", "-fno-omit-frame-pointer",
                "-fno-pie", "-no-pie"]
            command += [str(ROOT / "test/canvas/viewer_native_test.c"), "-o", str(exe)]
            build = subprocess.run(command, text=True, capture_output=True)
            (a.output / (variant + "-build.log")).write_text(build.stdout + build.stderr)
            results["builds"].append({"variant": variant, "command": command,
                                      "returncode": build.returncode,
                                      "executable_sha256": digest(exe) if build.returncode == 0 else None})
            if build.returncode:
                (a.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
                raise SystemExit(build.stderr)
            for value, w, h, fmt in a.geometry or [geometry(s) for s in GEOMETRIES]:
                for case in a.case or CASES:
                    key = f"{variant}-{value.replace(':', '-')}-{case}"
                    cmd = [str(exe), str(w), str(h), fmt, case]
                    if not a.no_samples and variant == "normal" and case == "ui":
                        sample_dir = a.output / "samples" / value.replace(":", "-")
                        sample_dir.mkdir(parents=True, exist_ok=True)
                        cmd.append(str(sample_dir))
                    jobs.append((key, variant, value, case, cmd))

        def run(job):
            key, variant, value, case, cmd = job
            env = dict(os.environ, ASAN_OPTIONS=f"detect_leaks={int(a.leak_sanitizer)}:halt_on_error=1",
                       UBSAN_OPTIONS="halt_on_error=1:print_stacktrace=1")
            started = time.monotonic()
            try:
                process = subprocess.run(cmd, text=True, capture_output=True, env=env, timeout=180)
                output = process.stdout + process.stderr
                code = process.returncode
            except subprocess.TimeoutExpired as exc:
                output = f"TIMEOUT after 180s: {exc}\n"
                code = 124
            (a.output / (key + ".log")).write_text(output)
            return {"key": key, "variant": variant, "geometry": value, "case": case,
                    "returncode": code, "elapsed_seconds": round(time.monotonic() - started, 3),
                    "log": key + ".log", "output": output.strip()}

        with ThreadPoolExecutor(max_workers=a.jobs) as pool:
            for future in as_completed([pool.submit(run, job) for job in jobs]):
                result = future.result()
                results["tests"].append(result)
                print(("PASS " if result["returncode"] == 0 else "FAIL ") + result["key"], flush=True)
                (a.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    results["tests"].sort(key=lambda item: item["key"])
    results["sources_unchanged"] = before == {str(s): digest(s) for s in sources}
    results["passed"] = sum(t["returncode"] == 0 for t in results["tests"])
    results["failed"] = len(results["tests"]) - results["passed"]
    results["samples"] = [str(p.relative_to(a.output)) for p in sorted((a.output / "samples").glob("*/*.pgm"))]
    (a.output / "results.json").write_text(json.dumps(results, indent=2) + "\n")
    print(f"Viewer native: {results['passed']} passed, {results['failed']} failed; "
          f"{len(results['samples'])} bounded samples; sources unchanged={results['sources_unchanged']}")
    return 0 if not results["failed"] and results["sources_unchanged"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
