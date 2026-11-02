# MPU9250 motion-reactive robot eyes

Status: **proposal / wiring preparation only**. No sensor driver, shake detector,
or confused animation is implemented yet. The existing eye animation stays intact.
A user-supplied photo-review report identifies a GY-9250/6500-style breakout;
the exact sensor silicon and electrical details remain open. Confusion will
replace both normal eyes entirely with spinning dizzy spirals (user-approved).

## Behavior

| State | Display | Transition |
|---|---|---|
| Default | Current blink / left-right / double-blink loop | Confirmed gentle wobble → squinting; confirmed deliberate shake → confused |
| Squinting | Both eyes partially closed; visual details TBD | Confirmed deliberate shake → confused; 2 continuous seconds of quiet → default |
| Confused | Spinning dizzy spirals replacing both eyes entirely | 2 continuous seconds of quiet → default |

**User decisions:** distinguish gentle wobble from deliberate shake; return to
normal after **2 seconds (2000 ms) of continuous quiet**. The sensor will be
mounted on the same breadboard as the display/Blue Pill during prototyping.
Secure the module so loose mounting or wires do not introduce independent motion.

Recommended transition policy (pending visual/interaction approval):
- Priority is confused > squinting > default. A confirmed shake can escalate
  directly from default or interrupt squinting without waiting for its animation.
- Confused stays latched during weaker movement; do not downgrade to squinting
  until returning through default. This avoids jitter between expressions.
- Any valid sample above the quiet threshold resets the quiet timer, including
  movement too weak to trigger a new reaction. Start timing again only below it.
- Return on the first valid quiet sample at/after 2000 ms of uninterrupted quiet;
  use elapsed timestamps, not a fixed number of samples or animation frames.

Use ordered thresholds: quiet < wobble < shake, with hysteresis and time-window
confirmation. Do not switch from a single noisy sample. Numeric acceleration
thresholds and the evidence window must be tuned from real sensor logs; a
200–300 ms confirmation window is a starting experiment, not a fixed requirement.
The two-second quiet hold already prevents a fleeting reaction; no separate
one-second minimum hold is required.

Recovery rules:
- After exit, restart the default timeline at its centered open-eye pose.
- On sensor absence, wrong identity, stale readings, or repeated read failures:
  stay/return to default and log a bounded-rate diagnostic. Missing readings are
  not proof of physical stillness. Do not freeze the display on sensor failure.

### Wobble/shake detection, not just tilt

Start with the accelerometer only; gyro and magnetometer are unnecessary for
this first interaction. A stationary sensor measures gravity, not zero acceleration.
Estimate the slow gravity component per axis with a low-pass filter, then measure
the remaining fast acceleration (sum of squares avoids a square root). Count
repeated high-motion samples in a short window rather than treating a single
impact as a sustained shake. Use the same filtered motion metric with separate
wobble and shake evidence levels. Test the higher-priority shake level first, so
a strong movement is not incorrectly classified only as wobble. Calibrate noise
and choose thresholds from stationary, gentle-wobble and deliberate-shake logs.

A slow change in orientation should normally not trigger confusion. An
acceleration-only detector can still react to fast rotation or shocks; test and
tune that behavior before adding a gyro-based refinement. Do not make a long,
stationary startup calibration mandatory: the user may power it on while moving.

### Squinting expression: details open

Gentle wobble should smoothly reduce both eyes' height into a squint, preserving
the narrow rounded shape. Exact height, glance behavior and transition duration
remain to be agreed; do not reuse the confusion expression for this state.

### Confused expression: spinning dizzy spirals

**User-approved:** replace both eyes entirely with animated spirals. Do not draw
rounded eye outlines behind them or place the spirals inside the normal eyes.
This should be visibly distinct from the gentle-wobble squint.

Proposed initial rendering details, to tune on the OLED:
- Two spirals centered at the normal eye centers, rotating in opposite directions.
- Start near one revolution per second; rotation follows elapsed time rather
  than the number of completed display frames.
- Use precomputed integer coordinates or compact bitmap phases; no floating-point
  math, runtime trigonometry libraries, or additional display libraries.
- Choose enough rotation phases for a smooth-looking result at the 60 FPS target;
  sending repeated phases at 60 FPS alone does not make the rotation smooth.
- Smoothly transition from the current pose into spirals; after 2 continuous
  seconds of quiet, transition back to the centered default pose.

Keep the expression readable on the monochrome 128x64 panel and avoid full-screen
flashing. All spiral phases and transition frames must fit inside the existing
partial-update window (x=13–114, y=8–55), including clearing the old eye shapes.
Test those bounds so the update remains 612 bytes and leaves no trails. If the
design later exceeds the window, expand it and recheck the 60 FPS bus budget.

## Proposed wiring

See [wiring.svg](wiring.svg). It is a **logical connection diagram**, not a
physical header map. The following report is provisional, not a verified schematic.

### Breakout review report (supplied by user)

- Photos were assessed by another agent as a **GY-9250/6500-style breakout**.
- VCC reportedly accepts **3–5 V if the marketplace specification matches**;
  proposed supply is Blue Pill **3.3 V**, not 5 V. This claim is not independently verified.
- Reported connections: SCL → PB6, SDA → PB7, GND → GND, AD0 → GND,
  FSYNC → GND, NCS high for I2C; INT, EDA and ECL disconnected.
- **Still unverified:** exact sensor model, pull-up voltage/resistance, and
  existing NCS/AD0 straps. A board-family marking does not identify its sensor.

### Checks before using the shared bus

1. With power off, inspect the schematic or measure straps/resistors to establish
   NCS/AD0 connections and pull-up destinations; do not short a hard-wired high
   signal to ground or vice versa.
2. Confirm the VCC input/regulator arrangement. If testing the sensor powered
   separately from the signal bus, use a current-limited 3.3 V supply and check
   its sensor rail and idle SDA/SCL voltages. Do not join it to the OLED until
   logic levels and combined pull-ups are compatible. Never measure resistance
   on a powered board.
3. During firmware bring-up, verify the address and read WHO_AM_I before selecting
   a driver configuration. An ACK at `0x68` alone does not identify an MPU9250.

The expected sensor address `0x68` (or `0x69`) differs from the OLED's `0x3C`.
Bus sharing remains conditional on the electrical checks above.

| MPU9250 breakout signal | Proposed connection | Notes |
|---|---|---|
| VCC (reported) | Blue Pill 3.3 | Reported 3–5 V input is conditional on matching board specification; verify regulator |
| GND | Blue Pill GND | Common ground with OLED and probe |
| SCL | PB6 | In parallel with OLED SCL |
| SDA | PB7 | In parallel with OLED SDA |
| nCS / CS | High at 3.3 V logic level | Selects I2C; inspect existing straps/pull-ups first |
| AD0 / SDO | GND | Selects `0x68`; high selects `0x69`; do not fight a board strap |
| INT | Unconnected initially | Poll/FIFO-based acquisition first |
| FSYNC | GND if exposed and unconnected | Verify breakout's existing bias first |
| Auxiliary SDA/SCL (EDA/ECL etc.) | Unconnected | Not the main I2C pins; magnetometer not needed |

Electrical checks before power-on:
- The IC's VDD range is 2.4–3.6 V; VDDIO is 1.71 V to VDD. This does **not**
  establish a breakout's VIN requirements: onboard regulators/level shifters vary.
- Use 3.3 V logic; never let SDA/SCL be pulled up to 5 V.
- Inspect pull-ups on both modules. Parallel resistors reduce effective resistance;
  check sink current and rise time before adding/removing resistors. Keep wires short.
- Power off to change wiring. Use one board power source, not probe 3V3 and USB together.
- Extra sensor pins require no STM32 GPIO in this initial design.

Reference: [TDK/InvenSense MPU-9250 Product Specification, revision 1.1](https://invensense.tdk.com/wp-content/uploads/2015/02/PS-MPU-9250A-01-v1.1.pdf),
particularly electrical characteristics, pin descriptions, and I2C interface.
Use its companion register map when implementing; do not assume an MPU6500 or
MPU9255-compatible module is an MPU9250 without checking identity.

## Firmware architecture and 60 FPS budget

Keep this split small and testable:
1. `mpu9250.c/.h`: identity, reset/wake/configuration, bounded register reads,
   acceleration conversion, FIFO support if needed.
2. `motion.c/.h`: sample-driven gravity filtering, shake evidence, quiet timer,
   hysteresis, and sensor-health handling. No display or hardware dependencies.
3. Eye renderer: take expression state and elapsed time; preserve the default loop.
4. Main loop: serialize I2C access, acquire samples, update state, then render/flush.

**Important existing limitation:** the I2C API currently supports writes only.
Add and test register-read transactions (including repeated START, ACK/NACK and
STOP handling) for both hardware and bit-bang drivers. STM32F1 receive sequencing
has special one-, two-, and multi-byte cases; follow RM0008 and applicable errata.
Do not just reuse the current transmitter-only address handling for reads.

The OLED's 612-byte DMA update occupies about 14 ms of each 16.67 ms frame.
The CPU is available during DMA, but the **shared I2C bus is not**. Never start a
sensor transaction while an OLED transfer is active. Read the sensor after
flush completion and before starting the next OLED transfer; keep all reads bounded.

Acquisition proposal:
- Begin with one 6-byte acceleration burst per rendered frame (~60 samples/s
  on hardware I2C) to validate wiring, identity, noise and a basic shake detector.
- If we need regular 100 Hz acquisition, let the MPU sample at that rate and use
  its FIFO, draining the queued acceleration samples in the available bus slot.
  Merely polling every 10 ms cannot work evenly while a 14 ms OLED transfer owns
  the same bus. Handle FIFO overflow and bound the number of samples per drain.
- A short burst is sub-millisecond at 400 kHz, but count/status reads, software
  overhead, clock stretching and error recovery also count. Measure FPS and the
  complete read + render + flush cost rather than promising 60 FPS in advance.
- Bit-bang draws fewer frames and needs separate acquisition-rate consideration;
  do not infer quiet duration from a fixed number of rendered frames.
- If shared-bus timing proves too tight, discuss I2C2 on PB10/PB11 or SPI as a
  separate wiring revision. Do not overclock the current bus as the first fix.

Use timestamp-based timers, wrap-safe `millis()` arithmetic, integer math and no
vendor HAL/libraries. Sensor transactions must not overwrite in-flight OLED buffers.

## Implementation milestones

1. **Confirm hardware and UX**: resolve remaining supply/pull-up/strap checks and
   expression details. Two trigger levels, a shared prototype breadboard, and a
   two-second quiet return are agreed; sensitivity needs measured calibration.
2. **Bring-up only**: verify sensor address and WHO_AM_I, configure accelerometer,
   log stationary/tilted/shaken readings while the existing animation keeps running.
3. **Detector**: host tests using stationary noise, orientation changes, gentle
   wobble, deliberate shake, isolated impacts, escalation and no downgrade during
   weaker movement. Check quiet recovery at 1999/2000 ms, timer reset on renewed
   motion, missing samples/faults and time wraparound.
4. **Reaction renderers**: approve distinct squinting and confused sequences; test
   all poses against the partial-update window, then connect them to the detector.
5. **Board validation**: check state transitions, false triggers, FPS, I2C errors,
   unplugged-sensor behavior and FIFO overflow if FIFO is enabled. Build both drivers.

## Questions to resolve

- Resolve the report's remaining unknowns: exact sensor model, pull-up voltage/resistance,
  and existing NCS/AD0 straps. GY-9250/6500-style appearance is only provisional.
- Exact squint height/behavior and transition durations? Confusion's full-eye
  spiral replacement is agreed; spiral size, line thickness and speed need visual tuning.
- Approve latching confusion until quiet rather than downgrading to squinting?
- Should an isolated impact trigger a reaction, or only repeated wobbling/shaking?
- Choose numeric sensitivities from logs on the shared breadboard; mounting is
  agreed, but the module should be secured before calibration.
