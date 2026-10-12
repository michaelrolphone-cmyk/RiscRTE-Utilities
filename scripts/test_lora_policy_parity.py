#!/usr/bin/env python3
"""Disabled modal policy preserves exact C tokens and the selected LoRa model."""
import argparse,hashlib,json
from pathlib import Path
from test_spectrum_shared_input_safety import preprocess
ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser(description=__doc__);p.add_argument('--system-apps',type=Path,required=True);p.add_argument('--baseline-utilities',type=Path,required=True);p.add_argument('--output',type=Path,required=True);a=p.parse_args();rows=[]
for defines in ([],['PORTABLE_NOVA_UI','PORTABLE_ALARM_CLIENT'],['PORTABLE_LORA_SHARED_TEXT_INPUT','PORTABLE_TEXT_INPUT_CLIENT'],['PORTABLE_LORA_SHARED_TEXT_INPUT','PORTABLE_TEXT_INPUT_CLIENT','PORTABLE_NOVA_UI','PORTABLE_ALARM_CLIENT']):
 before=preprocess(a.baseline_utilities.resolve(),a.system_apps.resolve(),'lora_messages.c',defines)
 after=preprocess(ROOT,a.system_apps.resolve(),'lora_messages.c',defines)
 assert before==after,defines
 rows.append({'defines':defines,'token_identical':True,'sha256':hashlib.sha256(after).hexdigest()})
p=ROOT/'Apps/lora_messages_model.h';assert p.read_bytes()==(a.baseline_utilities/'Apps/lora_messages_model.h').read_bytes()
a.output.parent.mkdir(parents=True,exist_ok=True);a.output.write_text(json.dumps({'parity':rows,'model_sha256':hashlib.sha256(p.read_bytes()).hexdigest()},indent=2)+'\n');print('Four disabled-policy C token profiles and original LoRa model identical')
