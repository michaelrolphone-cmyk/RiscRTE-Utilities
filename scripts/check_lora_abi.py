#!/usr/bin/env python3
"""Verify the consumer declaration against its clean immutable Watch source."""
import argparse
import json
from pathlib import Path
import subprocess
ROOT=Path(__file__).resolve().parents[1]
START='#define TWATCH_RADIO_API_V1'
def verify(watch):
 pin=json.loads((ROOT/'sdk/lora-sources.json').read_text())['watch_radio_abi']
 actual=subprocess.check_output(['git','rev-parse','HEAD'],cwd=watch,text=True).strip()
 dirty=subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=watch,text=True).strip()
 if actual!=pin or dirty:raise ValueError('Clean exact Watch radio ABI source required')
 provider=(watch/'include/twatch_caps.h').read_text()
 consumer=(ROOT/'Apps/PortableLoRaV2.h').read_text()
 expected=provider[provider.index(START):provider.index('/* Speaker and microphone')]
 declared=consumer[consumer.index(START):consumer.index('_Static_assert(sizeof(twatch_lora_config_v2)')]
 if declared!=expected:raise ValueError('Consumer radio ABI differs from exact provider source')
 print('LoRa consumer ABI matches pinned Watch provider declaration exactly')
if __name__=='__main__':
 parser=argparse.ArgumentParser();parser.add_argument('--watch',required=True,type=Path);args=parser.parse_args();verify(args.watch.resolve())
