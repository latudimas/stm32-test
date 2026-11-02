# MPU9250 firmware acquisition validation

This document records the acquisition checkpoint and labelled tuning captures.
The subsequent classifier/renderers are described in
[motion-reactive eyes implementation](motion-reactive-eyes.md).

Branch: `feat/mpu9250-logging`, based on the committed hardware-probe checkpoint
`1e328af`. This step adds **acquisition and logging only**. No motion thresholds,
classifier, squinting renderer or spiral renderer is connected yet.

## Implementation

- `src/i2c_hw.c`: bounded combined register reads using repeated START and the
  STM32F1 receiver's distinct one-, two-, and last-three-byte endings. Short
  ACK/ADDR/STOP critical sections preserve PRIMASK. No polling wait runs with
  interrupts masked. Read calls reject in-flight OLED DMA and invalid arguments.
- `src/i2c_bitbang.c`: same register-read API, last-byte NACK, clock stretching
  bounded by a 2 ms timeout. Failed calls may leave partial buffer contents.
- `src/mpu9250.c`: probes `0x68`/`0x69`, accepts WHO_AM_I `0x71` only, wakes the
  accelerometer, disables gyro axes, sets +/-2 g with 41 Hz DLPF and a 100 Hz
  internal rate. It reads back configuration before reporting success.
- Sampling reads INT_STATUS plus six acceleration bytes in one burst. Only
  data-ready samples are published, and output remains unchanged on error or
  no-data. Timestamps are read-completion `millis()` values, not FIFO sample times.
- Shared-bus acquisition runs after the old OLED flush finishes and before the
  next one starts. Latest-only reads occur once per rendered frame; the 100 Hz
  internal sensor stream is intentionally not losslessly recorded at this stage.
- Three consecutive transfer errors or 500 ms without fresh data mark the sensor
  unavailable. Retry initialization after 5 seconds, with rate-limited fault logs.
  Healthy acquisition does not block on settling; initialization/reinitialization
  may pause frames for 100 ms after wake-up. Sensor failure does not select a new
  eye expression. A physically stuck shared bus can still disrupt the OLED too.
- `src/uart.c`: 512-byte thread-polled TX queue (511 usable), serviced by main.
  Sample and status formatting is queued so a long line does not block frame
  scheduling. Backpressure blocks only if the queue fills; do not exceed the
  115200-baud line rate. No UART interrupt or extra DMA channel is used.

## Hardware observations

Both drivers initialized the actual `0x68` sensor (identity decimal 113 = `0x71`),
read back configuration and acquired changing samples. Boot/read-back exercises
I2C receive lengths 1, 2, 3 and 7. The original animation stays unchanged.

| Driver | Capture duration | CSV samples | Sample rate | Observed FPS | Max observed flush | Errors |
|---|---:|---:|---:|---:|---:|---|
| Hardware I2C, 400 kHz | 20 s | 1187 | 60.00 Hz | 60.0 | 15 ms | 0 sensor, 0 display |
| Bit-bang, ~100 kHz | 9 s | 104 | 12.00 Hz | 12.0 | 75 ms | 0 sensor, 0 display |
| Final hardware I2C logging build | 10 s | 587 | 60.00 Hz | 60.0 | 15 ms | 0 sensor, 0 display |

Hardware sample intervals were 16/17 ms; bit-bang intervals were 83/84 ms.
No not-ready samples were reported in those captures. Maximum sensor read time
was 0 ms hardware / 1 ms bit-bang on the coarse `millis()` clock; **0 ms means
below the clock's measurement resolution**, not an instantaneous transfer.

Example hardware status:

```text
tick 4 at 16225 ms, frames 961, fps x10 600, max flush ms 15, frame errors 0, samples 961, sensor errors 0, not ready 0, max read ms 0
```

The board was restored to the hardware I2C logging build after testing bit-bang.
A 64 KB flash backup was made before the first logging flash at
`build/pre-mpu-flash.bin`; keep it if rollback is needed. `build/` is local and
ignored, not a durable repository archive.

**The early 20 s / 9 s runs include substantial movement/transients**, so their
entire ranges or standard deviations must not be treated as stationary noise.
After asking the user to leave the breadboard still, the final 10 s run was much
quieter: mean raw XYZ **(470.7, 1261.1, 14016.3)** and peak-to-peak raw XYZ
**(104, 96, 173)**, with a mean-vector magnitude of **0.8594 g**. That is a useful
candidate quiet baseline, not an absolute calibration. Its CSV/log are saved at
`build/accel-hw-final.csv` / `.log`.

Next capture gentle wobble and deliberate shake with explicitly coordinated
operator actions. Absolute scale/bias remains uncalibrated as discussed in
[initial bring-up](mpu9250-bringup.md).

## Capture protocol

Close `tio` or any other serial reader. The Python capture tool uses only the
standard library and writes both CSV and full serial output:

```sh
python3 tools/capture_accel.py --seconds 15 --output build/stationary.csv
python3 tools/capture_accel.py --seconds 15 --output build/wobble.csv
python3 tools/capture_accel.py --seconds 15 --output build/shake.csv
```

Run one recording at a time **after agreeing with the operator which movement to
perform**. Keep the sensor secured to the breadboard; do not strain wires or let
exposed contacts touch conductive surfaces. Start with stationary, then gentle
wobble, then a deliberate but safe shake. Rotation is useful to check that slow
orientation changes do not falsely trigger a shake later.

The capture tool's optional `--reset` resets the MCU after opening serial, without
flashing. USB-UART buffers may contain an earlier boot; summaries keep the latest
boot/sample session, while the `.log` preserves the full received text.

## Operator-labelled gentle-wobble recording

The operator said "ready to wobble" and was cued to gently rock the entire
breadboard for a 15-second capture. Saved `build/wobble.csv` and `.log`.

- 895 valid CSV samples, timestamps 133241–148225 ms; valid-row rate 59.66 Hz.
- Firmware status remained 60.0 FPS, with zero sensor/display errors and no
  not-ready reads. Maximum observed flush remained 15 ms.
- Raw XYZ peak-to-peak: **(8943, 17558, 16324)** counts.
- Consecutive vector-change magnitude (nominal g, only 16/17 ms sample pairs):
  median **0.0409**, 95th percentile **0.1266**, maximum **0.7555**.
- For comparison, the quiet final hardware recording had vector-change median
  **0.0029**, 95th percentile **0.0057**, maximum **0.0082**.

This shows useful separation from quiet, but is not yet a threshold calibration:
vector differences depend on sample interval and are not the final filtered motion
metric. The isolated maximum also argues against using a single sample to classify
shake intensity. Deliberate-shake data is still needed.

Four serial lines contained decoding replacements and were not valid CSV. There
were five 33 ms gaps among otherwise 16/17 ms intervals; the firmware counters
showed no I2C acquisition errors. Treat this as an imperfect host-side capture,
not evidence of sensor read failures. The CSV retains only syntactically valid
sample lines; the full log preserves the malformed output for inspection.

## Operator-labelled deliberate-shake recording

The operator said "ready to shake" and was cued to shake the entire breadboard
more strongly than gentle wobble, without loosening components or straining wires.
A 15-second capture saved `build/shake.csv` and `.log`.

- 890 valid CSV samples, timestamps 225225–240191 ms; valid-row rate 59.40 Hz.
- Firmware remained at 60.0 FPS with zero sensor/display errors, no not-ready
  reads, and a 15 ms maximum observed flush.
- Raw XYZ peak-to-peak: **(38407, 43082, 22116)** counts.
- Consecutive vector-change magnitude (nominal g, only 16/17 ms sample pairs):
  median **0.2500**, 95th percentile **0.5352**, maximum **0.8781**.
- No samples reached the near-saturation criterion of abs(axis) >= 32760 counts.
- Eight serial lines contained decoding replacements. Valid timestamps include
  four 33 ms gaps, one 34 ms gap and two 50 ms gaps; other intervals are 16/17 ms.
  These gaps must not be treated as sensor faults or fed into a fixed-dt metric.

Summary of uncalibrated adjacent-sample change (not the final classifier metric):

| Operator label | Median | 95th percentile | Maximum |
|---|---:|---:|---:|
| Quiet candidate | 0.0029 | 0.0057 | 0.0082 |
| Gentle wobble | 0.0409 | 0.1266 | 0.7555 |
| Deliberate shake | 0.2500 | 0.5352 | 0.8781 |

The typical deliberate-shake change is about six times the wobble change, with
clear separation from quiet. However, their single-sample maxima overlap: use
sustained evidence / a time window, not one peak, to distinguish reactions.
These recordings support starting an offline/replay-tuned classifier, not claiming
universal thresholds from one person's movements. Keep the two-second continuous
quiet return and timestamp-aware filtering. No extra rotation is needed before
that implementation step; later validate slow-tilt rejection and state transitions.

## Automated checks and remaining validation

Host tests under address/undefined-behavior sanitizers cover:
- Sensor initialization, alternate address, unsupported identity, busy/NACK/read
  failures, fresh/not-ready output semantics, signed extremes and timestamp wrap.
- UART queue ordering, signed formatting, CRLF and repeated ring-buffer wrap.
- Existing animation bounds, partial-region DMA packing/ownership and frame timing.

Both bare-metal builds compile without warnings and link without a C library.
Host sensor tests mock I2C; they do not replace hardware receiver-sequence tests.
Electrical fault injection (stuck lines/unplugging) has not been performed, and
not every generic read length up to the API's 256-byte bound has been tested.
Gyro, magnetometer, FIFO, complete bias/gain calibration and automatic motion
classification remain out of scope for this checkpoint.
