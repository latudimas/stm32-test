# MPU9250 first hardware probe

Status: communication and raw accelerometer reads verified on the connected
prototype. This is **not** yet a calibrated sensor, motion detector, or production
I2C-read implementation. Physical wiring and resistor values were not inspected.

## Method

Used ST-Link/OpenOCD to reset/halt the CPU and bit-bang PB6/PB7 with slow SWD
register accesses. No firmware was flashed and no production source was changed.
The existing eye animation pauses during the probe. Sensor power/range settings
are saved, temporarily set for awake operation at +/-2 g, then restored; the
STM32 is reset/run afterward to resume normal firmware.

Reproduce (close other OpenOCD/GDB sessions first):

```sh
openocd -f interface/stlink.cfg -f target/stm32f1x.cfg \
        -f tools/mpu_probe.tcl
```

This is deliberately slow and **does not verify 400 kHz sensor transactions**,
shared-bus scheduling, or analog signal integrity. It does not use the serial port.
The script prints errors explicitly; check for `PROBE ERROR` in its output.

## Observed results

- ST-Link target voltage: **3.248 V**. This is the debugger's target-voltage
  measurement, not an independent measurement of the sensor's regulated rail.
- Idle bus levels: SCL high, SDA high.
- OLED `0x3C`: ACK.
- Sensor `0x68`: ACK; `0x69`: no ACK (consistent with AD0 low).
- Register `0x75` WHO_AM_I: **`0x71`**, matching MPU9250 identity. This is not
  proof of genuine silicon or magnetometer functionality.
- Temporary read-back: PWR_MGMT_1 = `0x01`, PWR_MGMT_2 = `0x00`,
  ACCEL_CONFIG = `0x00` (+/-2 g, nominal 16384 counts/g).

Eight consecutive coherent acceleration bursts from `0x3B`:

| Sample | Raw X | Raw Y | Raw Z |
|---|---:|---:|---:|
| 0 | -908 | 1320 | 13940 |
| 1 | -836 | 1340 | 14020 |
| 2 | -824 | 1324 | 14004 |
| 3 | -932 | 1292 | 14036 |
| 4 | -808 | 1376 | 14016 |
| 5 | -872 | 1336 | 13892 |
| 6 | -860 | 1344 | 14024 |
| 7 | -812 | 1364 | 14024 |

Mean XYZ: **(-0.0523, +0.0816, +0.8542) g** at nominal sensitivity.
Magnitude of the mean: **0.8596 g**. Axis peak-to-peak: (124, 84, 144) counts.
Readings are nonzero and vary, rather than being stuck at 0 or 0xFF.

**User-confirmed setup:** the breadboard was lying flat and stationary on a desk.
A repeat probe in this same setup again returned WHO_AM_I `0x71` and successful
reads. Mean raw XYZ was (-804, 1333.5, 13972), equivalent to
**(-0.0491, +0.0814, +0.8528) g**, with magnitude **0.8581 g**.
Thus the low magnitude is repeatable, not just a single anomalous sample.

A third probe, requested after suggesting a side-on orientation, returned mean
raw XYZ **(1.5, 1279.5, 14002.5)** = **(+0.0001, +0.0781, +0.8546) g**,
magnitude **0.8582 g**. WHO_AM_I remained `0x71`, all eight reads succeeded,
and both bus lines were idle-high. The actual new orientation was not confirmed.
Z still dominates, so this is not evidence of gravity transferring to another
axis; verify the sensor module itself rotates with the breadboard before the
next orientation test.

**Follow-up required:** stationary gravity should measure roughly 1 g regardless
of orientation. The observed ~0.86 g is lower than expected; do not call the
sensor calibrated or choose final thresholds from these samples. Check stationary
readings in multiple orientations (ideally all six faces) to separate offsets
from sensitivity error and verify response to physical movement. Do not simply
multiply all readings by 1/0.86: a single orientation cannot distinguish an axis
bias from a gain error. The gyro and magnetometer remain untested.

## Side-on orientation check

After requesting an approximately 90-degree rotation of the entire breadboard,
a further eight successful samples returned mean raw XYZ
**(-215, 17275, -3318.5)** = **(-0.0131, +1.0544, -0.2025) g**,
with magnitude **1.0737 g**. WHO_AM_I was still `0x71`; OLED and sensor both ACKed.

Gravity moved from predominantly Z to predominantly Y, confirming the sensor
responds to physical orientation changes. The readings are not stuck. The change
from ~0.86 g flat to ~1.07 g side-on also means a single global scale correction
is not justified. Bias/scale accuracy remains uncalibrated; these few orientations
do not determine a full calibration.

The user agreed to defer full calibration for the initial relative-motion
interaction. Use a filtered baseline and tune from actual stationary/wobble/shake
logs; do not interpret these readings as calibrated absolute acceleration.
The side-on probe restored sensor settings and restarted the existing firmware;
no flashing or production sensor integration was performed.

## Further rotated orientation check

The user rotated the breadboard again onto another side. Eight successful samples
returned mean raw XYZ **(-15865, 790, -1785.5)** =
**(-0.9683, +0.0482, -0.1090) g**, magnitude **0.9756 g**. WHO_AM_I remained
`0x71`, and both OLED and sensor addresses ACKed.

Gravity now lies mainly along negative X, rather than the previous positive Y.
Together with the flat Z-dominant readings, this confirms orientation response
on all three axes. This is not an opposite-polarity measurement of the same axis,
so it cannot be paired with the previous Y reading to calculate gain or offset.
No gain/offset correction has been applied; full calibration remains deferred.
Normal firmware was restarted after the probe without flashing.

## Existing display after test

A six-second UART capture after restoring normal execution contained:

```text
tick 9 at 36132 ms, frames 2161, fps x10 600, max flush ms 14, frame errors 0
tick 10 at 40132 ms, frames 2401, fps x10 600, max flush ms 14, frame errors 0
```

Thus the existing animation was observed at **60.0 FPS**, with a **14 ms** maximum
observed flush and **zero reported frame errors**, while the newly wired sensor
was present but not being sampled by firmware. This does not establish the frame
rate once sensor acquisition is integrated.

## Next verification

1. Secure the sensor on the breadboard; capture a stationary baseline.
2. Side-on response is verified above. Full six-face direction/scale calibration
   is deferred by agreement; retain it as a later accuracy check.
3. Capture gentle wobble and deliberate shake separately, with timestamps. The
   slow SWD probe is not suitable for setting high-rate dynamic thresholds.
4. Add/test bounded I2C register reads for both drivers, then repeat at 400 kHz
   between OLED transfers. Verify FPS and errors again under active sensor reads.
5. Only then tune motion thresholds and connect the expression state machine.

The gyro, temperature sensor and AK8963 magnetometer were not tested in this run.
