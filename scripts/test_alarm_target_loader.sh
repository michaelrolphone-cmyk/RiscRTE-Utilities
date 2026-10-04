#!/usr/bin/env bash
set -euo pipefail
root="$(cd "$(dirname "$0")/.." && pwd)";runtime="$(cd "$1" && pwd)"
build="$(mktemp -d)";trap 'rm -rf "$build"' EXIT
cc -std=gnu11 -O1 -g -I"$root/test/alarm-loader-stubs" -I"$runtime/lib/elf_loader/include" \
 "$root/test/alarm_target_loader.c" "$runtime/lib/elf_loader/src/esp_elf.c" \
 "$runtime/lib/elf_loader/src/arch/esp_elf_xtensa.c" "$runtime/lib/elf_loader/src/esp_elf_validate.c" \
 -o "$build/target-loader"
"$build/target-loader" "$root/dist/alarm-apps/alarms/alarms.elf" \
 "$root/dist/alarm-apps/countdown/countdown.elf" "$root/dist/alarm-apps/alarm-service/driver.elf"
