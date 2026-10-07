#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)"
runtime="$(cd "$1" && pwd)";system="$(cd "$2" && pwd)"
out="$(mktemp -d)";trap 'rm -rf "$out"' EXIT
san=();if [[ "${SANITIZE:-0}" == 1 ]];then san=(-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -g);fi
inc=(-I"$root/lib/Alarm/include" -I"$runtime/sdk/driver" -I"$runtime/sdk/app" -I"$system/lib/PortableApps/include")
cc "${san[@]}" -std=c11 -O1 -Wall -Wextra -Werror -fPIC -fvisibility=hidden -shared "${inc[@]}" -DALARM_NATIVE_UTC -DALARM_VISUAL_ONLY -DALARM_DND_CONTROL -DPOINTS_IN_TIME_SERVICE "$root/Services/alarm_service/service.c" "$system/lib/PortableApps/src/PortableTimeZone.c" "$system/lib/PortableApps/src/PortableTimeZoneCatalog.c" -o "$out/driver.elf"
cp "$root/Services/alarm_service/native-utc-visual-manifest.json" "$out/service.json"
cc "${san[@]}" -std=c11 -O1 -Wall -Wextra -Werror -fPIC -fvisibility=hidden -shared "$root/test/fixtures/native_utc_alarm_client.c" -o "$out/app.elf"
c++ "${san[@]}" -std=c++17 -Wall -Wextra -Werror -Wno-missing-field-initializers -rdynamic "${inc[@]}" \
 -I"$runtime/src" -I"$runtime/sdk/hardware" -I"$runtime/lib/ArduinoJson/src" -I"$runtime/test/drivers/stubs" \
 "$runtime/src/bootstrap/Json.cpp" "$runtime/src/bootstrap/Board.cpp" "$runtime/src/bootstrap/Runtime.cpp" \
 "$runtime/src/runtime/drivers/ProviderGraphV2.cpp" "$runtime/src/runtime/drivers/ProviderModuleV2.cpp" \
 "$root/test/native_utc_alarm_runtime_test.cpp" -ldl -o "$out/test"
"$out/test" "$out"
