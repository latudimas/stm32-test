#!/usr/bin/env python3
"""Capture timestamped acceleration CSV and full UART output (no dependencies).

Close tio/other serial readers first. --reset resets the STM32 via ST-Link after
opening serial; it does not flash. Output should normally go under build/.
"""
import argparse
import csv
import math
import os
from pathlib import Path
import re
import select
import subprocess
import termios
import time


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", default="/dev/cu.usbmodem1102")
    parser.add_argument("--seconds", type=float, default=15)
    parser.add_argument("--output", type=Path, default=Path("build/accel.csv"))
    parser.add_argument("--reset", action="store_true")
    args = parser.parse_args()
    if args.seconds <= 0:
        parser.error("--seconds must be positive")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    fd = os.open(args.port, os.O_RDWR | os.O_NOCTTY | os.O_NONBLOCK)
    old = termios.tcgetattr(fd)
    data = bytearray()
    try:
        attr = termios.tcgetattr(fd)
        attr[0] = attr[1] = attr[3] = 0
        attr[2] = termios.CLOCAL | termios.CREAD | termios.CS8
        attr[4] = attr[5] = termios.B115200
        attr[6][termios.VMIN] = attr[6][termios.VTIME] = 0
        termios.tcsetattr(fd, termios.TCSANOW, attr)
        termios.tcflush(fd, termios.TCIFLUSH)
        if args.reset:
            subprocess.run([
                "openocd", "-f", "interface/stlink.cfg", "-f", "target/stm32f1x.cfg",
                "-c", "init", "-c", "reset run", "-c", "shutdown",
            ], check=True)
        end = time.monotonic() + args.seconds
        while time.monotonic() < end:
            if select.select([fd], [], [], min(0.1, max(0, end - time.monotonic())))[0]:
                data.extend(os.read(fd, 4096))
    finally:
        termios.tcsetattr(fd, termios.TCSANOW, old)
        os.close(fd)

    log = data.decode("ascii", errors="replace")
    log_path = args.output.with_suffix(".log")
    if log_path == args.output:
        log_path = args.output.with_name(args.output.name + ".serial.log")
    log_path.write_text(log)
    # USB-UART buffers can retain pre-reset output. Keep only the last boot
    # session for statistics; preserve all received bytes in the .log file.
    banner = "== stm32-test boot =="
    session = banner + log.rsplit(banner, 1)[1] if banner in log else log
    samples = []
    for line in session.splitlines():
        match = re.fullmatch(r"accel,(\d+),(-?\d+),(-?\d+),(-?\d+)", line)
        if match:
            row = tuple(map(int, match.groups()))
            if samples and ((row[0] - samples[-1][0]) & 0xFFFFFFFF) >= 0x80000000:
                print("Timestamp reset detected; retaining the latest sample session.")
                samples.clear()
            samples.append(row)
        elif line and not line.startswith("accel,"):
            print(line)
    with args.output.open("w", newline="") as out:
        writer = csv.writer(out)
        writer.writerow(["ms", "x_raw", "y_raw", "z_raw"])
        writer.writerows(samples)
    print(f"Saved {len(samples)} samples: {args.output}; serial: {log_path}")
    if not samples:
        raise SystemExit("No acceleration samples received; inspect the serial log.")
    means = [sum(s[i] for s in samples) / len(samples) for i in (1, 2, 3)]
    spans = [max(s[i] for s in samples) - min(s[i] for s in samples) for i in (1, 2, 3)]
    print("Mean raw XYZ:", ", ".join(f"{v:.1f}" for v in means))
    print("Peak-to-peak raw XYZ:", spans)
    print("Magnitude of mean (uncalibrated g):", f"{math.sqrt(sum(v*v for v in means))/16384:.4f}")
    if len(samples) > 1:
        intervals = [(b[0] - a[0]) & 0xFFFFFFFF for a, b in zip(samples, samples[1:])]
        elapsed = sum(intervals)
        rate = (len(samples) - 1) * 1000 / elapsed if elapsed else 0
        print(f"Acquisition rate: {rate:.2f} Hz; sample interval min/max: {min(intervals)}/{max(intervals)} ms")


if __name__ == "__main__":
    main()
