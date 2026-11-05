#!/usr/bin/env python3
"""Full RF controller through the shipping adapter and real logical canvas.

Requires build_shared_sdr.py output in the same checkout. Reuses its exact
defines, staged headers, materialized adapter and portable source selection.
Only the Xtensa compiler/math shim and native sleep peripheral are replaced.
This is host integration evidence, not hardware or physical e-ink qualification.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path
import random
import re
import subprocess

ROOT = Path(__file__).resolve().parents[1]


def sha(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def once(source, old, new):
    if source.count(old) != 1:
        raise ValueError("Ambiguous host fixture anchor: " + old[:100])
    return source.replace(old, new, 1)


def function(source, start, next_start, replacement):
    begin = source.index(start)
    end = source.index(next_start, begin)
    return source[:begin] + replacement + "\n" + source[end:]



def run_logged(cmd, log, stage, evidence, timeout=180, env=None):
    """Persist failed/timeout stage evidence before propagating the failure."""
    def decoded(value):
        return value.decode(errors="replace") if isinstance(value, bytes) else value or ""
    try:
        result = subprocess.run(cmd, text=True, capture_output=True,
                                timeout=timeout, env=env)
        stdout, stderr = result.stdout, result.stderr
        failure = None if result.returncode == 0 else {
            "kind": "nonzero_exit", "returncode": result.returncode}
    except subprocess.TimeoutExpired as error:
        stdout, stderr = decoded(error.stdout), decoded(error.stderr)
        failure = {"kind": "timeout", "timeout_seconds": timeout}
        result = None
    log.write_text(stdout + stderr)
    if failure:
        prior = json.loads(evidence.read_text()) if evidence.exists() else {"hardware_verified": False}
        prior["failure"] = {**failure, "stage": stage, "command": list(map(str, cmd)),
                            "stdout": stdout, "stderr": stderr, "log": str(log)}
        evidence.write_text(json.dumps(prior, indent=2) + "\n")
        print(stdout + stderr, flush=True)
        if result is None:
            raise subprocess.TimeoutExpired(cmd, timeout, output=stdout, stderr=stderr)
        result.check_returncode()
    return result


def fixture():
    original = ROOT / "test/native_apps/rf_renderer_test.c"
    s = '#include "canvas_provider.h"\n' + original.read_text()
    s = once(s, '#include "../../Apps/waterfall.c"',
             '#include "' + str(ROOT / "Apps/waterfall.c") + '"')
    s = once(s, 'V("key_page",key_page);V("key_choice",key_choice);',
             '\n#ifndef PORTABLE_RF_SHARED_TEXT_INPUT\n'
             'V("key_page",key_page);V("key_choice",key_choice);\n#endif\n')
    s = once(s, "static jmp_buf fx_retained_jump;",
             'static jmp_buf fx_retained_jump;\n#include "rf_shared_canvas_declarations.inc"')
    s = function(s, "static void fx_guards(void)", "static unsigned fx_pixel(",
                 "static void fx_guards(void){sc_guards();}")
    s = function(s, "static unsigned fx_pixel(", "static void fx_snapshot(",
                 "static unsigned fx_pixel(unsigned x,unsigned y){return sc_pixel(x,y);}")
    s = function(s, "static void fx_corners(void)", "static uint32_t fx_region_hash(",
                 "static void fx_corners(void){sc_corners();}")
    s = once(s, '  if(op[0]==\'#\'||!op[0]){fx_line[0]=0;continue;}',
             '  if(op[0]==\'#\'||!op[0]){fx_line[0]=0;continue;}\n'
             '  if(sc_command(op)){fx_line[0]=0;continue;}')
    s = function(s, "static bool fx_info(", "static void fx_frame_release(",
                 "static bool fx_info(void*c,risc_display_info_v1*s){return sc_info(c,s);}\n"
                 "static bool fx_frame(void*c,uint32_t f,risc_display_surface_v1*s){return sc_frame(c,f,s);}")
    s = once(s, "(void)c;(void)o;fx_io();assert(fx_frames&&f==1);",
             "(void)c;(void)o;fx_io();assert(fx_frames&&f==1);sc_verify_raster();")
    s = function(s, "static void fx_touch_report(", "#ifdef RF_RENDER_WATCH_TOUCH\nvoid hid_renderer_watch_report",
                 "static void fx_touch_report(risc_touch_snapshot_v1*s){sc_report(s);}")
    s = once(s, "(void)c;fx_io();assert(n==1);",
             "(void)c;fx_io();assert(n<=RISC_TOUCH_QUEUE_LENGTH);")
    s = function(s, "static bool fx_nav(", "static bool fx_foreground(",
                 "static bool fx_nav(void*c,risc_input_navigation_frame_v1*s){\n"
                 " (void)c;fx_io();static uint32_t last;\n"
                 " *s=(risc_input_navigation_frame_v1){.buttons=fx_buttons,\n"
                 " .pressed=fx_buttons&~last,.released=last&~fx_buttons};last=fx_buttons;return true;\n}")
    s = function(s, "static bool fx_health(", "static void fx_yield(",
                 "static bool fx_health(risc_runtime_health_v1*h){h->uptime_ms=fx_ticks;return true;}")
    signature = "static bool fx_acquire(const char*n,uint32_t v,uint64_t id,risc_runtime_capability_v1*g){"
    s = once(s, signature, '#include "rf_shared_canvas_peripherals.inc"\n' + signature)
    s = once(s, " fx_io();assert(g->struct_size==sizeof(*g));",
             " fx_io();assert(g->struct_size==sizeof(*g));"
             "int handled=0;bool result=sc_acquire(n,v,id,g,&handled);if(handled)return result;")
    s = once(s, ".request_launch=fx_launch,.acquire=fx_acquire,.release=fx_release};",
             ".request_launch=fx_launch,.acquire=fx_acquire,.release=fx_release,"
             ".retain_invocation=sc_retain,.exit_requested=sc_exit_requested};")
    s = once(s, "int main(int argc,char**argv){", "int rf_unused_fixture_main(int argc,char**argv){")
    s += '\n#include "rf_shared_canvas_main.inc"\n'
    return s


def logical(case):
    w, h, _, r, _, l, t, right, bottom, _, _, _ = case
    ins = (l, t, right, bottom)
    return ((h if r % 2 else w) - ins[r] - ins[(r+2) % 4],
            (w if r % 2 else h) - ins[(r+1) % 4] - ins[(r+3) % 4])


def cases():
    # Format IDs come from RiscDisplayOutputV1: MONO1=1, RGB565=5.
    result = []
    for w, h in [(160, 160), (200, 200), (240, 240), (410, 502),
                 (320, 240), (640, 240), (240, 320), (480, 800), (800, 480)]:
        result.append((w, h, 5, 0, 0, 0, 0, 0, 0, 2, 0, 1))
    rng = random.Random(0xCA45A5)
    for r in range(4):
        for tr in range(4):
            for fmt in (1, 5):
                w, h = rng.randrange(241, 400, 2), rng.randrange(401, 600, 2)
                ins = [rng.randrange(3, 16) for _ in range(4)]
                result.append((w, h, fmt, r, tr, *ins, 2, (r+tr) % 2, 1))
    # Conservative rectangular safe viewport within a circular physical panel.
    for fmt in (1, 5):
        result.append((240, 240, fmt, 0, 1, 36, 36, 36, 36, 2, 0, 1))
        for mode in (0, 1):
            result.append((301, 353, fmt, 3, 2, 7, 11, 13, 17, mode, 0, 1))
    # Logical viewport, not physical panel, is the production admission boundary.
    for side, admitted in [(159, 0), (160, 1), (1024, 1), (1025, 0)]:
        for axis in (0, 1):
            lw, lh = (side, 240) if axis == 0 else (240, side)
            result.append((lw+8, lh+12, 5, 0, 0, 3, 5, 5, 7, 2, 0, admitted))
    return result


def scene(case):
    w, h = logical(case)
    mode = case[9]
    lines = ["wait 10", "sc_geometry", "check native_canvas eq 1",
             "check running eq 1", "remember captures", "wait 3",
             "different captures"]
    if mode:
        lines += ["nav 8", "wait 4", "check view eq 1",
                  "nav 32", "wait 4", "check frozen eq 1",
                  "remember history", "remember captures", "wait 3",
                  "same history", "same captures"]
    if mode == 2:
        lines += ["nav 8", "wait 3", "nav 8", "wait 3", "check view eq 0",
                  f"tap 6 {min(100, h-85)}", "wait 3",
                  f"tap 6 {min(100, h-85)}", "wait 3",
                  "check cursor_hz eq 2400000000",
                  f"tap {w-7} {min(100, h-85)}", "wait 3",
                  f"tap {w-7} {min(100, h-85)}", "wait 3",
                  "check cursor_hz eq 2480000000"]
        # A contact in the physical right inset must not become an app contact.
        r = case[3]
        if case[5+(r+2) % 4]:
            lines += [f"tap 6 {min(100, h-85)}", "wait 3",
                      f"tap 6 {min(100, h-85)}", "wait 3",
                      "check cursor_hz eq 2400000000",
                      "remember cursor_hz", "sc_rejection_begin", f"tap {w} {h//2}",
                      "wait 3", "same cursor_hz", "sc_rejection_end"]
        lines += [f"tap {w-15} 26", "wait 3", "check page eq 1",
                  "nav 32", "wait 3",
                  f"check scroll eq {min(48, max(0, 624-(max(240, (h*240+w-1)//w)-94)))}", "nav 1",
                  "wait 3", "check page eq 0"]
    # Exercise a height-only change and neutral input after geometry invalidation.
    # Start from a settled frame and preserve the physical allocation.
    if case[-1] and 200 <= w <= 900 and 200 <= h <= 900:
        nw, nh = case[0], case[1]
        if case[3] % 2:
            nw += 7
        else:
            nh += 7
        lines += ["wait 8", f"sc_resize {nw} {nh}", "wait 8",
                  "sc_geometry", "sc_gap"]
    lines += ["finish"]
    return "\n".join(lines) + "\n"


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--shared", type=Path, required=True)
    p.add_argument("--canvas-system", type=Path, required=True)
    p.add_argument("--output", type=Path, required=True)
    p.add_argument("--cc", default="cc")
    a = p.parse_args()
    shared, system, out = a.shared.resolve(), a.canvas_system.resolve(), a.output.resolve()
    out.mkdir(parents=True, exist_ok=False)
    build = json.loads((shared / "build.json").read_text())
    canvas_head = subprocess.check_output(["git", "-C", str(system), "rev-parse", "HEAD"], text=True).strip()
    if canvas_head != build["source_pins"]["canvas_system"]:
        raise ValueError("Canvas provider does not match shipping source pin")
    subprocess.run(["git", "-C", str(system), "diff", "--exit-code", "HEAD", "--"], check=True)
    # Header/source drift must not silently turn this into a different build.
    for name, digest in build["dependencies"].items():
        if sha(name) != digest:
            raise ValueError("Shipping dependency changed: " + name)
    if sha(shared / "waterfall.elf") != build["sha256"]:
        raise ValueError("Shipping ELF changed")
    src = out / "rf_shared_canvas.c"
    src.write_text(fixture())
    fixture_dir = ROOT / "test/native_apps"
    records = []
    custody_records = []
    manifest = json.loads((ROOT / "Apps/shared_sdr/waterfall.json").read_text())
    optional = {(r["capability"], r["api"]) for r in manifest["optional"]}
    contract = re.findall(r'\{"([^"]+)",(\d+),(\d+)\}',
                          (shared / "adapter/sdr_optional_acquire.h").read_text())
    if not contract or len(set(contract)) != len(contract) or any(
            (name, int(version)) not in optional for name, version, instance in contract):
        raise ValueError("Central custody contract must name distinct manifest optional services")
    evidence = out / "evidence.json"
    evidence.write_text(json.dumps({"hardware_verified": False,
        "shipping_elf_sha256": build["sha256"], "runs": []}, indent=2)+"\n")
    for sanitized in (False, True):
        flags = ["-fsanitize=address,undefined", "-fno-sanitize-recover=all",
                 "-fno-omit-frame-pointer", "-no-pie"] if sanitized else []
        # Exercise the actual generated custody include with a minimal Runtime,
        # before the controller scenarios. Do not inherit unrelated app defines.
        include_flags = []
        for shipping in build["commands"]:
            for arg in shipping[1:]:
                if arg.startswith("-I") and arg not in include_flags:
                    include_flags.append(arg)
        custody = out / ("custody-san" if sanitized else "custody")
        cmd = [a.cc, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
               "-Wno-unused-function", *flags, *include_flags,
               str(ROOT / "test/native_apps/sdr_optional_custody_test.c"), "-o", str(custody)]
        run_logged(cmd, out / (custody.name+"-compile.log"), "custody-compile", evidence)
        custody_result = run_logged([str(custody)], out / (custody.name+".log"), "custody-negative-controls",
                   evidence, env={**os.environ, "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                                  "UBSAN_OPTIONS": "halt_on_error=1"})
        custody_records.append({"sanitized": sanitized, "command": cmd,
                                "stdout": custody_result.stdout, "returncode": custody_result.returncode})
        prior = json.loads(evidence.read_text())
        prior["custody_negative_controls"] = custody_records
        evidence.write_text(json.dumps(prior, indent=2)+"\n")
        print(custody_result.stdout.strip(), "sanitized="+str(sanitized), flush=True)
        objects = []
        commands = []
        excluded = []
        for index, shipping in enumerate(build["commands"]):
            if "-c" not in shipping:
                continue
            original = Path(shipping[shipping.index("-c")+1])
            if original.name in ("portable_sleep.c", "SingleFloatDivisionCompat.c"):
                excluded.append(str(original))
                continue
            source = src if original == ROOT / "Apps/waterfall.c" else original
            if original == shared / "adapter/adapter.c":
                source = out / "adapter-observed.c"
                source.write_text(
                    "#define portable_sdr_canvas_refresh_geometry rf_host_canvas_refresh_impl\n"
                    + original.read_text() + "\n"
                    "#undef portable_sdr_canvas_refresh_geometry\n"
                    "static unsigned rf_host_refresh_count;\n"
                    "bool portable_sdr_canvas_refresh_geometry(void){rf_host_refresh_count++;"
                    "return rf_host_canvas_refresh_impl();}\n"
                    "unsigned rf_host_canvas_refresh_count(void){return rf_host_refresh_count;}\n"
                    "unsigned rf_host_canvas_pending(void){unsigned b=0;"
                    "b|=surface.frame?1u:0u;b|=paper_token?2u:0u;"
                    "b|=rf_canvas.surface.lease?4u:0u;b|=rf_canvas.surface.receipt?8u:0u;\n"
                    "#ifdef PORTABLE_QUICK_ACTIONS\n"
                    "b|=quick_modal?16u:0u;b|=quick_replay_pending?32u:0u;"
                    "b|=quick_replay_delivery?64u:0u;b|=quick_launch_pending?128u:0u;\n"
                    "#endif\n#ifdef PORTABLE_ALARM_CLIENT\n"
                    "b|=alarm_modal?256u:0u;\n#endif\nreturn b;}\n"
                    "unsigned rf_host_canvas_contract(void){return "
                    "(rf_canvas.input_active?1u:0u)|"
                    "((info.flags&RISC_DISPLAY_INFO_BRIGHTNESS)?2u:0u); }\n")
            # Keep the exact shipping preprocessor configuration and include order.
            common = []
            it = iter(shipping[1:])
            for arg in it:
                if arg.startswith(("-D", "-I")):
                    common.append(arg)
                elif arg == "-include":
                    common.extend((arg, next(it)))
            obj = out / (str(index) + ("-san" if sanitized else "") + ".o")
            cmd = [a.cc, "-std=c11", "-O1", "-g", "-Wall", "-Wextra", "-Werror",
                   "-Wno-misleading-indentation", *flags, *common,
                   "-DRF_RENDER_ORDERED_INPUT", "-DFX_W=1088u", "-DFX_H=1088u",
                   "-I"+str(fixture_dir),
                   "-I"+str(system / "Services/canvas_host"),
                   "-I"+str(system / "Services/scene_profile"),
                   "-I"+str(system / "lib/PortableApps/include"),
                   "-c", str(source), "-o", str(obj)]
            run_logged(cmd, out / (obj.stem+".log"), "compile", evidence)
            commands.append(cmd)
            objects.append(obj)
        exe = out / ("rf-shared-san" if sanitized else "rf-shared")
        cmd = [a.cc, *flags, *map(str, objects), "-lm", "-o", str(exe)]
        run_logged(cmd, out / (exe.name+"-link.log"), "link", evidence)
        commands.append(cmd)
        for index, case in enumerate(cases()):
            d = out / (str(index)+("-san" if sanitized else ""))
            d.mkdir()
            actions = d / "actions.txt"
            actions.write_text(scene(case) if case[-1] else "finish\n")
            cmd = [str(exe), str(d), str(actions), *map(str, case)]
            run = run_logged(cmd, d / "run.log", "scenario", evidence, timeout=180,
                                 env={**os.environ, "RF_RENDER_ASYNC": "1",
                                      "RF_RENDER_NO_IMAGES": "1",
                                      "ASAN_OPTIONS": "detect_leaks=0:halt_on_error=1",
                                      "UBSAN_OPTIONS": "halt_on_error=1"})
            (d / "run.log").write_text(run.stdout+run.stderr)
            records.append({"case": case, "logical": logical(case),
                            "sanitized": sanitized, "returncode": run.returncode,
                            "stdout": run.stdout, "log": str(d / "run.log")})
            (out / "evidence.json").write_text(json.dumps({
                "hardware_verified": False, "shipping_elf_sha256": build["sha256"],
                "shipping_build": str(shared / "build.json"),
                "host_substitutions": excluded,
                "fixture_sha256": sha(src), "commands": commands, "runs": records,
                "custody_negative_controls": custody_records,
            }, indent=2)+"\n")
            if run.returncode:
                print(run.stdout+run.stderr, flush=True)
            run.check_returncode()
            print(index, sanitized, run.stdout.strip(), flush=True)


if __name__ == "__main__":
    main()
