#!/bin/sh
# Host regression suite; no board connected or flashed by this script.
set -eu
cd "$(dirname "$0")/.."
tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT HUP INT TERM
run() {
    name=$1
    shift
    "${HOST_CC:-cc}" -std=c11 -Wall -Wextra -Werror \
        -fsanitize=address,undefined -Isrc "$@" -o "$tmp/$name"
    "$tmp/$name"
    printf 'PASS %s\n' "$name"
}
run motion tests/motion_test.c src/motion.c
run robot-eyes tests/robot_eyes_test.c src/robot_eyes.c
run robot-reactions tests/robot_reactions_test.c src/robot_eyes.c
run mpu9250 tests/mpu9250_test.c src/mpu9250.c
run uart tests/uart_test.c
run frame-schedule tests/frame_schedule_test.c
run ssd1306-region tests/ssd1306_region_test.c src/ssd1306.c src/font5x7.c
