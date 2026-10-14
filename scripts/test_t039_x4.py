#!/usr/bin/env python3
"""Reuse the exact X4 production-profile fixture with its redundant typedef removed."""
from pathlib import Path
script=Path(__file__).with_name('test_hid_buttons_universal_recovery.py');body=script.read_text()
needle="src = (root / 'test/native_apps/hid_renderer_test.c').read_text()"
assert body.count(needle)==1
body=body.replace(needle,needle.replace("hid_renderer_test.c","t039_frozen_hid_renderer_test.c")+"\nsrc=src.replace('typedef struct {alarm_service_v1 service;uint32_t output_modes;} alarm_service_outputs_v1;', '')")
# GT911 closes the old contact before beginning a new ID. These are two
# complete serial taps, not overlapping/malformed input; T039 must retain both.
anchor="    if case == 'sampled-defaults':\n        expected_early, expected_late = 8, 0"
assert body.count(anchor)==1
body=body.replace(anchor,"    if case in ('second-down','different-id') and not a.expect_baseline_loss:\n        expected_early, expected_late = 4, 2\n        wanted = pair(1, 6) * 2 + pair(0, 78)\n"+anchor)
exec(compile(body,str(script),'exec'),{'__file__':str(script),'__name__':'__main__'})
