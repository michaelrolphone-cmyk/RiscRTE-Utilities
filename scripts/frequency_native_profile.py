"""Bounded native qualification manifest/feature correspondence; no publishing."""
import copy
import json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]

def validate(profile,defines):
    if profile.get('schema')!=1 or profile.get('publish_to_catalog') is not False:
        raise ValueError('Native Frequency must remain a qualification-only candidate')
    app=profile['application'];flags=set(defines)
    for field,value in {'type':'application','id':'frequency_generator','architecture':'xtensa-esp32s3','file_name':'frequency_generator.elf','entry':'app_main'}.items():
        if app.get(field)!=value:raise ValueError('Wrong native Frequency '+field)
    if 'qualification' not in app.get('display_name','').lower():raise ValueError('Candidate label required')
    if 'PORTABLE_INPUT_NAVIGATION_LOCAL' in flags or 'PORTABLE_APP_SLEEP_LOCAL' in flags:
        raise ValueError('Standalone profile cannot claim unlinked local bridges')
    native={'PORTABLE_FREQUENCY_NATIVE_UI','PORTABLE_NATIVE_CANVAS','PORTABLE_APP_OWNS_TOUCH_CHROME','PORTABLE_NOVA_UI','PORTABLE_FORCE_FULL_FRAMES','PORTABLE_QUICK_ACTIONS','PORTABLE_QUICK_RADIOS','PORTABLE_QUICK_NATIVE'}
    if not native<=flags:raise ValueError('Native presentation/Quick feature missing')
    required={'display.output':1,'input.touch.raw':1}
    for feature,capability in [('PORTABLE_AUDIO_SESSION','audio.output'),('PORTABLE_ALARM_CLIENT','alarm.service'),('PORTABLE_INPUT_NAVIGATION','input.navigation')]:
        if feature not in flags:raise ValueError('Required native feature missing: '+feature)
        required[capability]=1
    if 'PORTABLE_RETURN_APP="springboard.elf"' not in flags:raise ValueError('Explicit return destination required')
    actual={row['capability']:row['api'] for row in app['requires']}
    if actual!=required or len(actual)!=len(app['requires']):raise ValueError('Native feature/grant mismatch')
    optional={'board.battery':1,'rtc.clock':2,'storage.key-value':1,'net.wifi':1,'bluetooth.hci':1}
    declared={row['capability']:row['api'] for row in app['optional']}
    if declared!=optional or len(declared)!=len(app['optional']) or actual.keys()&declared.keys():raise ValueError('Invalid optional Quick/telemetry grants')
    return app

def load():
    pin=json.loads((ROOT/'sdk/frequency-native-sources.json').read_text())
    profile=json.loads((ROOT/'profiles/frequency-native-qualification.json').read_text())
    validate(profile,pin['defines'])
    return pin,profile

if __name__=='__main__':
    pin,profile=load()
    for capability in ('display.output','input.touch.raw','audio.output','alarm.service','input.navigation'):
        bad=copy.deepcopy(profile);bad['application']['requires']=[r for r in bad['application']['requires'] if r['capability']!=capability]
        try:validate(bad,pin['defines'])
        except ValueError:pass
        else:raise AssertionError('Missing required grant accepted: '+capability)
    for feature in pin['defines']:
        try:validate(profile,[f for f in pin['defines'] if f!=feature])
        except ValueError:pass
        else:raise AssertionError('Missing required feature accepted: '+feature)
    print('PASS: native profile/flags match; five missing-grant and twelve missing-feature negative controls rejected')
