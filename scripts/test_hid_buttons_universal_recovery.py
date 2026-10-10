#!/usr/bin/env python3
"""Buttons production UI + exact GT911 queued-tap regression and safety matrix.

Only peripheral fixtures are transformed. Application, shared adapter and GT911
sources are compiled independently. Split cases explicitly simulate an 80 ms
HID-poll delay; this is not a measured hardware latency or over-air test.

Reconstructed after executor reset from the original task's retained tool text.
The application source is byte-exact; rerun this runner before claiming results.
"""
import argparse
import hashlib
import json
import os
import pathlib
import re
import subprocess
import sys

p = argparse.ArgumentParser(description=__doc__)
for name in ('target-receipt', 'system-apps', 'app-sdk', 'gt911-source', 'gt911-fixture', 'output'):
    p.add_argument('--' + name, type=pathlib.Path, required=True)
p.add_argument('--application-root', type=pathlib.Path, help='Defaults to this checkout; may select the unchanged baseline')
p.add_argument('--sanitize', action='store_true')
p.add_argument('--asynchronous-display', action='store_true', help='Independent 1600 ms immutable pending display token; requires decoupled adapter')
p.add_argument('--expect-baseline-loss', action='store_true', help='Require the original zero-report split-tap failure, while all safety/ordinary cases still pass')
p.add_argument('--case', action='append')
a = p.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
system = a.system_apps.resolve()
headers = a.app_sdk.resolve()
application = (a.application_root or root).resolve()
out = a.output.resolve()
out.mkdir(parents=True, exist_ok=True)
src = (root / 'test/native_apps/hid_renderer_test.c').read_text()


def replace(old, new):
    global src
    assert src.count(old) == 1, (old, src.count(old))
    src = src.replace(old, new)


replace('static unsigned ticks,polls,grants,frames,subs,presents;', '''static unsigned ticks,polls,grants,frames,subs,presents;
static unsigned probe_stage,probe_status_change,probe_neutral_after_failure;static bool probe_done,probe_gap,probe_report_failed;
static bool is_case(const char*s){return getenv("HID_PROBE_NEGATIVE")&&!strcmp(getenv("HID_PROBE_NEGATIVE"),s);}
static unsigned probe_at(void){return is_case("configured-split")||is_case("mouse-split")?100:30;}
static bool probe_split(void){const char*s=getenv("HID_PROBE_NEGATIVE");return s&&strncmp(s,"sampled-",8)&&strcmp(s,"configured-combo")&&strcmp(s,"boundaries");}
''')
replace('directory=argv[1];', 'setvbuf(stdout,NULL,_IONBF,0);directory=argv[1];')
replace('assert(++diagnostic_lines<80);', 'assert(++diagnostic_lines<2000);')
replace('void hid_renderer_watch_report(risc_touch_snapshot_v1*s){current_touch(s);}', '''void hid_renderer_watch_report(risc_touch_snapshot_v1*s){
 current_touch(s);
 if(probe_stage){s->contact_count=1;s->contacts[0]=(risc_touch_contact_v1){.id=1,.x=132,.y=528};
  if(probe_stage==2)s->contact_count=0;
  if(probe_stage==3)s->contacts[0].id=2;
  if(probe_stage==4){s->contact_count=2;s->contacts[1]=(risc_touch_contact_v1){.id=2,.x=348,.y=528};}
  if(probe_stage==5)s->contacts[0].x=20;
  if(probe_stage==6){s->contacts[0].x=134;s->contacts[0].y=530;}
  if(probe_stage==7)s->buttons=RISC_TOUCH_BUTTON_PRIMARY;
 }else if(probe_split()&&!is_case("held-release")&&hid_owned&&polls>=probe_at()&&polls<probe_at()+(is_case("rapid-split")?20:10)&&s->contact_count&&(touch_count%2)==1)s->contact_count=0;
}''')
replace('return watch_touch->next(watch_touch->context,watch_subscriptions[n],e);', '''if(n==2&&probe_gap){probe_gap=false;while(watch_touch->next(watch_touch->context,watch_subscriptions[n],e)>0){};puts("INJECT queue-gap");return -1;}
 int32_t rc=watch_touch->next(watch_touch->context,watch_subscriptions[n],e);
 if(n==2&&rc>0){printf("RAW poll=%u tick=%u seq=%llu kind=%u id=%u xy=%u,%u event_ms=%llu\\n",polls,ticks,(unsigned long long)e->sequence,e->kind,e->id,e->x,e->y,(unsigned long long)e->timestamp_ms);}return rc;''')
replace('return watch_touch->poll(watch_touch->context,n);', '''bool okay=watch_touch->poll(watch_touch->context,n);if(is_case("poll-fail")&&polls==31&&(touch_count%2)==1){puts("INJECT raw-poll-fail");return false;}return okay;''')
replace('static bool fake_hid_poll(', '''static void probe_driver_poll(unsigned stage){probe_stage=stage;assert(watch_touch->poll(watch_touch->context,1));probe_stage=0;}
static void probe_inject(void){
 if(!probe_split())return;
 if(is_case("rapid-split")){if(polls<30||polls>48||polls%2)return;}else if(probe_done||polls!=probe_at())return;
 probe_done=true;printf("INJECT case=%s poll=%u\\n",getenv("HID_PROBE_NEGATIVE"),polls);
 if(is_case("multi-contact"))probe_driver_poll(4);
 if(is_case("second-down")){probe_driver_poll(2);probe_driver_poll(1);}
 if(is_case("different-id"))probe_driver_poll(3);
 if(is_case("move-outside")){probe_driver_poll(5);probe_driver_poll(1);}
 if(is_case("move-inside"))probe_driver_poll(6);
 if(is_case("home-edge"))probe_driver_poll(7);
 if(is_case("gap"))probe_gap=true;
 if(is_case("no-auth"))probe_status_change=1;
 if(is_case("no-encryption"))probe_status_change=2;
 if(is_case("keyboard-unready"))probe_status_change=3;
 if(is_case("generation-change"))probe_status_change=4;
 if(is_case("disconnected"))probe_status_change=5;
 ticks+=is_case("timing-120ms")?120:80;
}
static bool fake_hid_poll(''')
replace('hid_polls++;if(getenv("HID_RENDER_PAIR_ERROR")', 'hid_polls++;probe_inject();if(getenv("HID_RENDER_PAIR_ERROR")')
replace('if(hid_owned&&getenv("HID_RENDER_DISCONNECT")&&polls>=40)', '''if(probe_status_change&&polls<40){
  if(probe_status_change==1)s->flags&=~RISC_HID_AUTHENTICATED;
  if(probe_status_change==2)s->flags&=~RISC_HID_ENCRYPTED;
  if(probe_status_change==3)s->flags&=~RISC_HID_KEYBOARD_READY;
  if(probe_status_change==4)s->connection_generation=2;
  if(probe_status_change==5){s->state=RISC_HID_ADVERTISING;s->flags=16;}
 }
 if(hid_owned&&getenv("HID_RENDER_DISCONNECT")&&polls>=40)''')
replace('held_mod=mods;held_key=keys[0];hid_keyboard_reports++;return true;', 'held_mod=mods;held_key=keys[0];hid_keyboard_reports++;bool okay=!(polls<40&&((is_case("press-fail")&&keys[0])||(is_case("release-fail")&&!keys[0])));probe_report_failed|=!okay;printf("KEY poll=%u tick=%u mods=%02X key=%02X accepted=%u\\n",polls,ticks,mods,keys[0],okay?1u:0u);return okay;')
replace('held_mod=held_key=held_mouse=0;hid_releases++;return true;', 'printf("RELEASE poll=%u tick=%u\\n",polls,ticks);if(probe_report_failed)probe_neutral_after_failure++;held_mod=held_key=held_mouse=0;hid_releases++;return true;')
replace('held_mouse=b;hid_mouse_reports++;return true;', 'held_mouse=b;hid_mouse_reports++;printf("MOUSE poll=%u tick=%u buttons=%u dx=%d dy=%d wheel=%d\\n",polls,ticks,b,x,y,w);return true;')
replace('assert(presents>0);', 'if((is_case("press-fail")||is_case("release-fail"))&&!getenv("HID_PROBE_BASELINE")){assert(probe_report_failed&&probe_neutral_after_failure>=1&&hid_closes==1);}assert(presents>0);')
if a.asynchronous_display:
    replace('if(polls>=20&&!resident_jump)', 'if(false&&polls>=20&&!resident_jump)')
    replace('assert(resident_jump);', 'assert(!resident_jump);')
    replace('static unsigned ticks,polls,grants,frames,subs,presents;', 'static unsigned ticks,polls,grants,frames,subs,presents,display_until;')
    replace('.flags=RISC_DISPLAY_INFO_PARTIAL_DAMAGE|', '.flags=RISC_DISPLAY_INFO_ASYNC_PRESENT|RISC_DISPLAY_INFO_PARTIAL_DAMAGE|')
    replace('if(paper_profile)ticks+=1600;', 'display_until=ticks+(paper_profile?1600:17);')
    replace('s->state=RISC_DISPLAY_PRESENT_COMPLETE;return true;', 's->state=ticks<display_until?RISC_DISPLAY_PRESENT_ACTIVE:RISC_DISPLAY_PRESENT_COMPLETE;return true;')
fixture = out / 'fixture.c'
fixture.write_text(src)
r = json.loads(a.target_receipt.read_text())
flags = list(r['build_defines']) + ['-I' + str(i) for i in [
    headers, system / 'lib/NativeApps/include', root / 'lib/NativeApps/include',
    root / 'lib/Bluetooth/include', root / 'lib/Contexts/include', root / 'Apps',
    system / 'Apps', root / 'test/native_apps',
]] + ['-DHID_RENDER_WATCH_TOUCH', '-DHID_RENDER_PAPER', '-DHID_RENDER_RESIDENT']
san = ['-fsanitize=address,undefined', '-fno-sanitize-recover=all', '-fno-omit-frame-pointer', '-no-pie'] if a.sanitize else []
flags += san
catalog = out / 'catalog.c'
catalog.write_text('#include "PortableApps.h"\nconst t5_app_manifest_t portable_catalog[1]={{.compatible=false}};\nconst unsigned portable_catalog_count=0;\n')
sources = [str(application / 'Apps/ble_buttons.c')] + [str(system / 'lib/PortableApps/src' / f) for f in [
    'adapter.c', 'PortableNativeTimeSource.c', 'PortableRealtimeClient.c', 'PortableTimeZone.c',
    'PortableTimeZoneCatalog.c', 'PortableTimeZonePreference.c',
]] + [str(catalog)]
backend = '''/* Physical GPIO/I2C/time doubles only; the production GT911 owns event queues. */
#define main x4_gt911_fixture_main
#include FIXTURE_PATH
#undef main
void hid_renderer_watch_report(risc_touch_snapshot_v1 *sample);
uint64_t hid_renderer_watch_millis(void);
static bool hid_transact(void*c,uint64_t id,const uint8_t*w,size_t wn,uint8_t*r,size_t rn,uint32_t timeout){
 if(wn==2&&w[0]==0x81&&w[1]==0x4e){
  risc_touch_snapshot_v1 value={0};hid_renderer_watch_report(&value);
  assert(value.width==480&&value.height==800);
  packet(value.contact_count,value.contacts[0].x,value.contacts[0].y,(value.buttons&RISC_TOUCH_BUTTON_PRIMARY)!=0);
  for(unsigned i=0;i<value.contact_count;i++)wire_point(i,(uint8_t)(value.contacts[i].id-1),value.contacts[i].x,value.contacts[i].y);
 }
 return transact(c,id,w,wn,r,rn,timeout);
}
static uint64_t hid_now(void*c){(void)c;return hid_renderer_watch_millis();}
const risc_touch_api_v1 *hid_watch_touch_start(void){
 driver=t5_driver_get(2);assert(driver);api=driver->capability;power=risc_touch_power(api);assert(power);
 bus.base.transact=hid_transact;clock_api.monotonic_ms=hid_now;assert(start());return api;
}
void hid_watch_touch_stop(void){done(0);}
'''.replace('FIXTURE_PATH', json.dumps(str(a.gt911_fixture.resolve())))
bp = out / 'backend.c'
bp.write_text(backend)
for i, source in enumerate([bp, a.gt911_source.resolve()]):
    obj = out / ('gt911-' + str(i) + '.o')
    subprocess.run(['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *san, '-I' + str(headers), '-c', str(source), '-o', str(obj)], check=True)
    sources.append(str(obj))
evidence_sources = [
    application / 'Apps/ble_buttons.c', application / 'Apps/ble_hid_app.inc', application / 'Apps/ble_hid_paper.inc',
    system / 'lib/PortableApps/src/adapter.c', a.gt911_source.resolve(), a.gt911_fixture.resolve(),
    a.target_receipt.resolve(), pathlib.Path(__file__).resolve(),
]
source_hashes = {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in evidence_sources}
exe = out / 'buttons'
cmd = ['cc', '-std=c11', '-O1', '-g', '-Wall', '-Wextra', '-Werror', *flags, str(fixture), *sources, '-o', str(exe)]
(out / 'compile-command.json').write_text(json.dumps(cmd, indent=2))
subprocess.run(cmd, check=True)
results = []
cases = [
    'sampled-defaults', 'sampled-rapid', 'sampled-hold', 'configured-combo', 'configured-split', 'mouse-split',
    'boundaries', 'plain-split', 'rapid-split', 'move-inside', 'multi-contact', 'second-down', 'different-id',
    'move-outside', 'home-edge', 'gap', 'poll-fail', 'timing-120ms', 'no-auth', 'no-encryption',
    'keyboard-unready', 'generation-change', 'disconnected', 'held-release', 'press-fail', 'release-fail',
]
for case in cases:
    if a.case and case not in a.case:
        continue
    d = out / case
    d.mkdir(exist_ok=True)
    actions = [(3, 110, 732)] + ([(29, 132, 528)] if case == 'held-release' else []) + [(30, 132, 528), (80, 348, 320), (180, 60, 50)]
    if case == 'sampled-defaults':
        actions = [(3, 110, 732), (30, 132, 320), (40, 348, 320), (50, 132, 528), (60, 348, 528), (180, 60, 50)]
    if case == 'rapid-split':
        actions = [(3, 110, 732)] + [(n, 132, 528) for n in range(30, 50, 2)] + [(80, 348, 320), (180, 60, 50)]
    if case == 'sampled-rapid':
        actions = [(3, 110, 732)] + [(n, 132, 528) for n in range(30, 50, 2)] + [(180, 60, 50)]
    if case == 'sampled-hold':
        actions = [(3, 110, 732)] + [(n, 132, 528) for n in range(30, 80)] + [(180, 60, 50)]
    if case in ('configured-combo', 'configured-split'):
        actions = [(3, 348, 732), (10, 132, 540), (20, 405, 345), (30, 200, 475), (40, 132, 228), (50, 132, 348), (60, 60, 50), (70, 348, 732), (80, 110, 732), (100, 132, 320), (180, 60, 50)]
    if case == 'boundaries':
        actions = [(3, 110, 732), (30, 32, 224), (40, 231, 415), (50, 232, 415), (60, 248, 224), (70, 447, 415), (80, 448, 415), (90, 32, 432), (100, 231, 623), (110, 231, 624), (120, 248, 432), (130, 447, 623), (140, 240, 425), (180, 60, 50)]
    if case == 'configured-split':
        actions.insert(-1, (140, 348, 320))
    if case == 'mouse-split':
        actions = [(3, 348, 732), (10, 132, 540), (20, 200, 220), (30, 348, 732), (40, 110, 732), (100, 132, 320), (140, 348, 320), (180, 60, 50)]
    ap = d / 'actions.txt'
    ap.write_text(''.join(f'{n} {x} {y}\n' for n, x, y in actions))
    env = {**os.environ, 'ASAN_OPTIONS': 'detect_leaks=0', 'HID_RENDER_ACTIVE': '1', 'HID_PROBE_NEGATIVE': case, **({'HID_PROBE_BASELINE': '1'} if a.expect_baseline_loss else {})}
    run = subprocess.run([str(exe), str(d), str(ap)], env=env, capture_output=True, text=True, timeout=30)
    (d / 'run.log').write_text(run.stdout + run.stderr)
    keys = [(int(a), int(b, 16), int(c, 16)) for a, b, c in re.findall(r'KEY poll=(\d+) tick=\d+ mods=([A-F0-9]+) key=([A-F0-9]+)', run.stdout)]
    early = [(a, b, c) for a, b, c in keys if a < 80]
    late = [(a, b, c) for a, b, c in keys if a >= 80]
    mouse = [(int(a), int(b), int(c), int(d), int(e)) for a, b, c, d, e in re.findall(r'MOUSE poll=(\d+) tick=\d+ buttons=(\d+) dx=(-?\d+) dy=(-?\d+) wheel=(-?\d+)', run.stdout)]
    expected_early = 2 if case in ['plain-split', 'move-inside', 'held-release'] else 0
    expected_late = 0 if case == 'home-edge' else 2
    if a.expect_baseline_loss and case in ['plain-split', 'move-inside']:
        expected_early = 0
    def pair(mod, key):
        return [(mod, key), (0, 0)]
    wanted = (pair(1, 6) if expected_early else []) + (pair(0, 78) if expected_late else [])
    if case == 'sampled-defaults':
        expected_early, expected_late = 8, 0
        wanted = pair(0, 75) + pair(0, 78) + pair(1, 6) + pair(1, 25)
    if case == 'rapid-split':
        expected_early, expected_late = (0 if a.expect_baseline_loss else 20), 2
        wanted = ([] if a.expect_baseline_loss else pair(1, 6) * 10) + pair(0, 78)
    if case == 'sampled-rapid':
        expected_early, expected_late = 20, 0
        wanted = pair(1, 6) * 10
    if case == 'sampled-hold':
        expected_early, expected_late = (1 if a.asynchronous_display else 3), 1
        wanted = [(1, 6)] * expected_early + [(0, 0)]
        if a.asynchronous_display:
            key_times=[int(t) for t in re.findall(r'KEY poll=\d+ tick=(\d+)', run.stdout)]
            assert len(key_times)==2 and 0<key_times[1]-key_times[0]<400, key_times
    if case == 'configured-combo':
        expected_early, expected_late = 0, 2
        wanted = pair(5, 76)
    if case == 'boundaries':
        expected_early, expected_late = 8, 8
        wanted = pair(0, 75) * 2 + pair(0, 78) * 2 + pair(1, 6) * 2 + pair(1, 25) * 2
    if case == 'configured-split':
        expected_early, expected_late = 0, (2 if a.expect_baseline_loss else 4)
        wanted = ([] if a.expect_baseline_loss else pair(5, 76)) + pair(0, 78)
    if case == 'mouse-split':
        expected_early, expected_late = 0, 2
        wanted = pair(0, 78)
    if case == 'press-fail':
        expected_early, expected_late = 1, 0
        wanted = [(1, 6)]
    if case == 'release-fail':
        expected_early, expected_late = 2, 0
        wanted = pair(1, 6)
    if a.expect_baseline_loss and case in ('press-fail', 'release-fail'):
        expected_early, expected_late = 0, 2
        wanted = pair(0, 78)
    expected_mouse = [(1, 0, 0, 0), (0, 0, 0, 0)] if case == 'mouse-split' and not a.expect_baseline_loss else []
    item = dict(mouse_reports=mouse, expected_mouse=expected_mouse, case=case, returncode=run.returncode, early=early, late=late, expected_early=expected_early, expected_late=expected_late, expected_reports=wanted, passed=run.returncode == 0 and len(early) == expected_early and len(late) == expected_late and [(b, c) for _, b, c in keys] == wanted and [tuple(v[1:]) for v in mouse] == expected_mouse)
    results.append(item)
    (d / 'result.json').write_text(json.dumps(item, indent=2) + '\n')
    print(json.dumps(item), flush=True)
(out / 'results.json').write_text(json.dumps(results, indent=2) + '\n')
assert source_hashes == {str(p): hashlib.sha256(p.read_bytes()).hexdigest() for p in evidence_sources}, 'Source changed during compile or execution; rerun'
receipt = {'expected_baseline_loss': a.expect_baseline_loss, 'sanitized': a.sanitize, 'hardware_verified': False, 'simulated_hid_poll_ms': 80, 'source_hashes': source_hashes, 'cases': results}
(out / 'evidence.json').write_text(json.dumps(receipt, indent=2) + '\n')
if not results or not all(v['passed'] for v in results):
    sys.exit(1)
print(f'Buttons raw queued taps: {len(results)} cases passed; hardware not tested')
