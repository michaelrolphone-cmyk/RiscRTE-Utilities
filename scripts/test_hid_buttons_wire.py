#!/usr/bin/env python3
"""Real Buttons + FT6336U + NimBLE against the independent HCI central."""
import argparse, ctypes, hashlib, importlib.util, json, pathlib

p = argparse.ArgumentParser(description=__doc__)
for name in ('drivers', 'provider', 'app-seam', 'output'):
    p.add_argument('--' + name, type=pathlib.Path, required=True)
p.add_argument('--boot-protocol', action='store_true')
p.add_argument('--expect-baseline-loss', action='store_true')
a = p.parse_args()
oracle = a.drivers.resolve() / 'test/ble_hid_protocol_test.py'
spec = importlib.util.spec_from_file_location('hid_protocol', oracle)
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)
f = m.Fixture(a.provider.resolve())
f.start()
f.pair()
f.discover()
handle = f.kbd
if a.boot_protocol:
    assert b'\x13' in f.att(b'\x12' + m.le(f.bootkey + 1) + b'\x01\x00')
    f.att(b'\x52' + m.le(f.proto) + b'\x00')
    f.outgoing(4)
    handle = f.bootkey
assert f.status().state == 5 and f.status().flags & 13 == 13
app = ctypes.CDLL(str(a.app_seam.resolve()))
app.hid_buttons_watch_report_seam.argtypes = [ctypes.c_void_p, ctypes.c_uint64]
app.hid_buttons_watch_report_seam.restype = None
transactions = []
for _ in range(2):
    app.hid_buttons_watch_report_seam(f.driver.api, f.t.value)
    f.pump()
    wire = f.outgoing(4)
    expected = [] if a.expect_baseline_loss else [
        b'\x1b' + m.le(handle) + bytes([5, 0, 76, 0, 0, 0, 0, 0]),
        b'\x1b' + m.le(handle) + bytes(8),
    ]
    assert wire == expected, [x.hex() for x in wire]
    transactions.append([x.hex() for x in wire])
    # Neutralized input must not cause a delayed watchdog duplicate.
    f.time += 1100
    f.pump()
    assert not f.outgoing(4)
assert f.status().state == 5 and f.status().flags & 13 == 13
m.close_fixture(f)
assert f.claims == f.releases == 1 and not f.lease
receipt = {'transactions': transactions, 'protocol': 'boot' if a.boot_protocol else 'report',
           'expected_baseline_loss': a.expect_baseline_loss, 'hardware_tested': False,
           'source_or_binary_sha256': {str(x.resolve()): hashlib.sha256(x.read_bytes()).hexdigest()
                                     for x in (oracle, a.provider, a.app_seam, pathlib.Path(__file__))}}
a.output.write_text(json.dumps(receipt, indent=2) + '\n')
print('Actual FT6336U Buttons -> secure NimBLE -> exact ATT press/neutral FIFO: PASS', receipt['protocol'], transactions)
