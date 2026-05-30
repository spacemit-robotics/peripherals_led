#!/usr/bin/env bash
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
module_root="$(cd "$script_dir/.." && pwd)"
artifact_dir="${SROBOTIS_TEST_ARTIFACT_DIR:-${SROBOTIS_OUTPUT_ROOT:-$PWD/output}/test-artifacts/components/peripherals/led/${SROBOTIS_TEST_NAME:-led-generic-hardware-smoke}}"
log_dir="$artifact_dir/logs"
build_dir="$artifact_dir/build"
log_file="$log_dir/led_generic_hardware_smoke.log"

sysfs_name="${LED_SYSFS_NAME:-sys-led_1}"
active_level="${LED_ACTIVE_LEVEL:-low}"
timeout_s="${LED_HW_SMOKE_TIMEOUT_S:-25}"

mkdir -p "$log_dir" "$build_dir"

{
    echo "[info] module_root=$module_root"
    echo "[info] build_dir=$build_dir"
    echo "[info] sysfs_name=$sysfs_name"
    echo "[info] active_level=$active_level"
    echo "[info] timeout_s=$timeout_s"

    cmake -S "$module_root" -B "$build_dir" \
        -DBUILD_TESTS=ON \
        -DSROBOTIS_PERIPHERALS_LED_ENABLED_DRIVERS=drv_generic
    cmake --build "$build_dir" --target test_led_generic -j"$(nproc)"
    LD_LIBRARY_PATH="$build_dir:${LD_LIBRARY_PATH:-}" \
        timeout "$timeout_s" "$build_dir/test_led_generic" "$sysfs_name" "$active_level"
} 2>&1 | tee "$log_file"

grep -q "=== LED GENERIC Test ===" "$log_file"
grep -q "\\[assert\\] sysfs brightness assertions PASSED" "$log_file"
