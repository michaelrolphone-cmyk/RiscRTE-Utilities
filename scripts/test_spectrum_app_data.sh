#!/usr/bin/env bash
# Host-only production consumer/backend integration; never mount a device.
set -euo pipefail
repo="$(cd "$(dirname "$0")/.." && pwd)"
runtime="$(cd "${1:?Usage: test_spectrum_app_data.sh RUNTIME_CHECKOUT}" && pwd)"
python3 - "$repo/sdk/spectrum-temporal-sources.json" "$runtime" <<'PY'
import json,subprocess,sys
dependency=json.load(open(sys.argv[1]))
runtime=sys.argv[2]
assert dependency['runtime_publication']=='published'
assert subprocess.check_output(['git','rev-parse','HEAD'],cwd=runtime,text=True).strip()==dependency['runtime_commit'], 'Exact Spectrum app-data Runtime commit required'
assert not subprocess.check_output(['git','status','--porcelain','--untracked-files=no'],cwd=runtime,text=True).strip(), 'Clean app-data Runtime source required'
PY
cmp "$repo/Apps/RiscAppDataV1.h" "$runtime/sdk/app/RiscAppDataV1.h"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
for mode in normal sanitizer;do
 flags=();if [[ "$mode" == sanitizer ]];then flags=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g);fi
 mkdir "$build/$mode"
 "${CXX:-c++}" "${flags[@]}" -std=c++17 -O1 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -I"$runtime/src" -I"$runtime/sdk/app" \
  "$repo/tests/spectrum_temporal_runtime_test.cpp" "$runtime/src/runtime/storage/AppDataFiles.cpp" \
  -Wl,--wrap=read,--wrap=write,--wrap=rename,--wrap=close -lm -o "$build/test-$mode"
 "$build/test-$mode" "$build/$mode"
done
