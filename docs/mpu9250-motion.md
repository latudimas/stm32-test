# MPU9250 motion-reactive robot eyes

Status: **classifier and reaction animations implemented; oscillatory-wobble
revision under test**. Current behavior, thresholds and validation limits are in
[motion-reactive eyes implementation](motion-reactive-eyes.md). Original visual
reactions are confirmed; revised live sensitivity checks are pending. This
document retains the design/wiring rationale.
Debugger-based identity/orientation checks are in [bring-up results](mpu9250-bringup.md).
Firmware acquisition using both I2C drivers is in
[acquisition validation](mpu9250-acquisition.md). The original eye loop is preserved
as the default state.
A user-supplied photo-review report identifies a GY-9250/6500-style breakout;
WHO_AM_I now matches MPU9250 (`0x71`), but sensor calibration, genuine silicon
provenance and electrical details remain unverified. Confusion will
replace both normal eyes entirely with spinning dizzy spirals (user-approved).

## Behavior

| State | Display | Transition |
|---|---|---|
| Default | Current blink / left-right / double-blink loop | Confirmed gentle wobble → squinting; confirmed deliberate shake → confused |
| Squinting | Both eyes 12 px high, small glances and blinks | Confirmed deliberate shake → confused; 1.2 continuous seconds of quiet → default |
| Confused | Spinning dizzy spirals replacing both eyes entirely | 2 continuous seconds of quiet → default |

**User decisions:** distinguish incidental movement from repeated gentle wobble,
then distinguish wobble from deliberate shake. Quiet return is now **1.2 seconds
(1200 ms) for squinting**, while confusion retains **2 seconds (2000 ms)**. The sensor will be
mounted on the same breadboard as the display/Blue Pill during prototyping.
Secure the module so loose mounting or wires do not introduce independent motion.

Implemented transition policy (revised sensitivity validation pending):
- Priority is confused > squinting > default. A confirmed shake can escalate
  directly from default or interrupt squinting without waiting for its animation.
- Confused stays latched during weaker movement; do not downgrade to squinting
  until returning through default. This avoids jitter between expressions.
- Any valid sample above the quiet threshold resets the quiet timer, including
  movement too weak to trigger a new reaction. Start timing again only below it.
- Return on the first valid quiet sample at/after 1200 ms (squinting) or 2000 ms
  (confusion) of uninterrupted quiet; use elapsed timestamps, not a fixed number
  of samples or animation frames.

Do not switch from a single noisy sample. Squint entry now requires repeated
signed acceleration excursions on one axis, not merely elevated magnitude.
Shake retains its original higher-magnitude evidence rule and priority. Numeric
thresholds and gesture timing need validation with real lift/carry/wobble logs.
The state-specific quiet holds prevent fleeting reactions; no separate minimum
expression duration is required.

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
impact as a sustained shake. For wobble, keep signed per-axis residuals and count
persistent alternating excursions on the same axis; magnitude alone cannot
distinguish a lift from back-and-forth motion. Test the higher-priority shake
level first, so a strong movement is not classified only as wobble. Choose
thresholds from stationary, incidental-handling, wobble and deliberate-shake logs.

A slow change in orientation should normally not trigger confusion. An
acceleration-only detector can still react to fast rotation or shocks; test and
tune that behavior before adding a gyro-based refinement. Do not make a long,
stationary startup calibration mandatory: the user may power it on while moving.

### Squinting expression

Gentle wobble reduces both eyes to a 12 px squint, preserving the narrow rounded
shape. Glances use one third of default travel; blinks still close to 2 px.
A 180 ms ordered dissolve transitions between states. These initial dimensions
and transition timing can be adjusted after looking at the OLED.

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

**Register reads now implemented:** `i2c_read_regs()` supports a register-pointer
write, repeated START, ACK/NACK and STOP for both drivers. STM32F1 hardware receive
uses separate one-, two-, and last-three-byte sequences (RM0008). Sensor identity,
configuration read-back and sampling exercise lengths 1, 2, 3 and 7 on the board.
The bit-bang driver now honors clock stretching with bounded waits.

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
   1.2 s squint / 2 s confused quiet returns are agreed; sensitivity needs real
   incidental-handling validation.
2. **Bring-up only**: verify sensor address and WHO_AM_I, configure accelerometer,
   log stationary/tilted/shaken readings while the existing animation keeps running.
3. **Detector**: host tests using stationary noise, orientation changes, gentle
   wobble, deliberate shake, isolated impacts, escalation and no downgrade during
   weaker movement. Check quiet recovery at 1199/1200 and 1999/2000 ms, timer
   reset on renewed motion, missing samples/faults and time wraparound.
4. **Reaction renderers**: approve distinct squinting and confused sequences; test
   all poses against the partial-update window, then connect them to the detector.
5. **Board validation**: check state transitions, false triggers, FPS, I2C errors,
   unplugged-sensor behavior and FIFO overflow if FIFO is enabled. Build both drivers.

## Questions to resolve

- Resolve remaining electrical unknowns: pull-up voltage/resistance and existing
  NCS/AD0 straps. WHO_AM_I matches MPU9250, but the low measured gravity magnitude
  needs multi-orientation verification before calibration or threshold tuning.
- Live visual tuning: initial 12 px squint, 180 ms transitions, two-pixel spiral
  strokes and one revolution/second. Confirm appearance and sensitivity on the OLED.
- Confirm the implemented confusion latch feels right during weaker movement.
- Verify normal lifting/carrying and isolated impacts stay default, while
  repeated gentle wobbling still triggers squinting reliably.
- Choose numeric sensitivities from logs on the shared breadboard; mounting is
  agreed, but the module should be secured before calibration.
