#!/usr/bin/env python3
"""Materialize RF-only transport hooks from exact installed adapter.
Draft: owning CI must compile and finish optional-service/lifecycle integration.
No repository writes or publication. Output includes use baseline src via -I.
"""
import argparse, hashlib
from pathlib import Path
p=argparse.ArgumentParser()
p.add_argument("--system",type=Path,required=True)
p.add_argument("--output",type=Path,required=True)
a=p.parse_args()
src=a.system/"lib/PortableApps/src"
def checked(name,blob):
    data=(src/name).read_bytes()
    actual=hashlib.sha1(b"blob "+str(len(data)).encode()+b"\0"+data).hexdigest()
    if actual!=blob: raise ValueError(f"{name}: expected exact installed blob {blob}, got {actual}")
    return data.decode()
adapter=checked("adapter.c","49ff2abf2d06270aab5ec5b1e1ef79fb62f551a4")
foreground=checked("foreground_adapter_open.inc","b02c3b488ae12b4dcbb92c75b694021a47d34144")
def once(s,old,new):
    if s.count(old)!=1: raise ValueError("Ambiguous source anchor: "+old[:120])
    return s.replace(old,new,1)
adapter="""#ifdef PORTABLE_RF_SHARED_CANVAS
#ifndef PORTABLE_NATIVE_CUSTODY_FENCE
#error "RF canvas requires native custody fencing"
#endif
/* These coordinates are provider logical content, never physical rotation. */
#undef PORTABLE_TOUCH_ROTATION
#define PORTABLE_TOUCH_ROTATION 0
#undef PORTABLE_DISPLAY_ROTATION
#define PORTABLE_DISPLAY_ROTATION 0
#endif
"""+adapter
# Geometry refresh uses the existing history buffers before navigation opens.
# Move their declarations, preserving one state identity for all adapter paths.
history_state="static uint16_t *previous_pixels;\nstatic uint8_t *paper_previous;\nstatic bool paper_previous_valid;\nstatic bool previous_valid;\n"
adapter=once(adapter,history_state,"")
adapter=once(adapter,'#ifdef PORTABLE_INPUT_NAVIGATION\n#include "PortableNavigation.h"',
    history_state+'#include "rf_capability_canvas_adapter.inc"\n#ifdef PORTABLE_INPUT_NAVIGATION\n#include "PortableNavigation.h"')
adapter=once(adapter,"static bool input_navigation_open(void) {\n#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL",
    "static bool input_navigation_open(void) {\n#ifdef PORTABLE_RF_SHARED_CANVAS\n navigation=rf_adapter_navigation_open();\n#elif defined(PORTABLE_INPUT_NAVIGATION_LOCAL)")
# The logical canvas owns navigation; only raw-navigation builds need a grant.
adapter=once(adapter,"#ifndef PORTABLE_INPUT_NAVIGATION_LOCAL\nstatic risc_runtime_capability_v1 navigation_grant;",
    "#if !defined(PORTABLE_INPUT_NAVIGATION_LOCAL) && !defined(PORTABLE_RF_SHARED_CANVAS)\nstatic risc_runtime_capability_v1 navigation_grant;")
# Both finalization paths keep logical navigation reset, but own no local raw grant.
adapter=once(adapter,"#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL\n portable_input_navigation_close(rt);",
    "#if defined(PORTABLE_RF_SHARED_CANVAS)\n /* Logical navigation has no independent grant. */\n#elif defined(PORTABLE_INPUT_NAVIGATION_LOCAL)\n portable_input_navigation_close(rt);")
adapter=once(adapter,"#ifdef PORTABLE_INPUT_NAVIGATION_LOCAL\n  portable_input_navigation_close(rt);",
    "#if defined(PORTABLE_RF_SHARED_CANVAS)\n  /* Logical navigation has no independent grant. */\n#elif defined(PORTABLE_INPUT_NAVIGATION_LOCAL)\n  portable_input_navigation_close(rt);")
adapter=once(adapter,'  portable_stage_log(rt,"draw-begin","");',
    '#ifdef PORTABLE_RF_SHARED_CANVAS\n  if(!rf_adapter_canvas_geometry())return false;\n#endif\n  portable_stage_log(rt,"draw-begin","");')
adapter=once(adapter,"  if(dg.api && !rt->release(&dg))return;",
    "#ifdef PORTABLE_RF_SHARED_CANVAS\n  if(rf_canvas_close(&rf_canvas)!=RF_CANVAS_READY)return;\n#endif\n  if(dg.api && !rt->release(&dg))return;")
adapter=once(adapter,"  if (dg.api && !rt->release(&dg))",
    "#ifdef PORTABLE_RF_SHARED_CANVAS\n  if(rf_canvas_close(&rf_canvas)!=RF_CANVAS_READY)return;\n#endif\n  if (dg.api && !rt->release(&dg))")
foreground=once(foreground,'  if (!rt->acquire("display.output", 1, 0, &dg))',
    '#ifdef PORTABLE_RF_SHARED_CANVAS\n  if(!rf_adapter_canvas_open())return -1;\n#else\n  if (!rt->acquire("display.output", 1, 0, &dg))')
foreground=once(foreground,'  if (!display->get_info(display->context, &info)',
    '#endif\n  if (!display->get_info(display->context, &info)')
foreground=once(foreground,"#ifdef PORTABLE_WATCH_COLOR_ONLY",
    "#if defined(PORTABLE_WATCH_COLOR_ONLY) && !defined(PORTABLE_RF_SHARED_CANVAS)")
# Required follow-on integration: optional alarms/Quick, logical sleep/resume,
# retained-frame text/resident handoff, controls intent dispatch and RF layout
# refresh. Do not publish this intermediate transport draft as complete.
a.output.mkdir(parents=True,exist_ok=True)
(a.output/"adapter.c").write_text(adapter)
(a.output/"foreground_adapter_open.inc").write_text(foreground)
