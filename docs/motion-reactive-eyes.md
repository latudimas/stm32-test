# Motion-reactive robot eyes implementation

Branch: `feat/motion-reactive-eyes`. The acquisition checkpoint is `cbf4e9b`;
recording findings were committed as `5cb2660` before this implementation.

## Current behavior

| Input | State | Display |
|---|---|---|
| Quiet / stationary | Default | Existing blink, left/right glance, double-blink loop |
| Confirmed gentle wobble | Squinting | Both eyes narrow to 12 px high, with small glances and blinks |
| Confirmed deliberate shake | Confused | Two spinning spirals completely replace the rounded eyes |
| 2 continuous seconds of classified quiet | Default | Return to centered open eyes, restart the default timeline |
| Sensor unavailable/stale | Default | Fault fallback, not a claim of physical quiet |

Shake can escalate from either default or squinting. Confusion remains latched
through weaker motion; it does not downgrade to squinting until after a quiet
return to default. Renewed motion resets the quiet timer. These are prototype
sensitivity settings, not universally calibrated definitions of wobble/shake.

## Classifier (`src/motion.c`)

Hardware-independent, sample-driven integer math, with timestamps rather than
frame counts. First sample seeds a baseline immediately, so no stationary startup
calibration is required. Raw inputs are converted to nominal mg (16384 counts/g).
An 80 ms per-axis low-pass estimates the gravity/bias baseline; squared residual
magnitude is the motion energy. No square root or floating-point library is used.

Initial thresholds, tuned against the preserved operator recordings:

| Level | Residual magnitude (nominal) | Evidence |
|---|---:|---|
| Quiet | <=25 mg | 2000 ms continuously quiet |
| Wobble | >=70 mg | 100 ms accumulated active evidence |
| Shake | >=420 mg | 160 ms accumulated active evidence |

Wobble/shake evidence is a leaky accumulator: add active elapsed milliseconds,
subtract inactive milliseconds, clamp to the confirmation limit. Entry also needs
at least three consecutive changing samples. A normalized L1 input-change gate
(12 mg per 17 ms) rejects the filter tail of an isolated impact; a single large
peak is not enough to select confusion. Shake has priority over wobble.

Timing/health details:
- Duplicate timestamps cannot add evidence.
- Integration uses actual sample intervals; an interval >100 ms resets baseline,
  entry evidence and quiet proof rather than treating an unseen gap as motion.
- Explicit missing/read-error samples break continuous quiet and entry evidence.
- At 500 ms stale, polling falls back to default without asserting physical quiet.
- Driver failure also explicitly resets the detector; reinitialization is still
  retried after 5 seconds by the acquisition code.
- The two-second return begins when **filtered motion** falls below the quiet
  level, not necessarily on the exact instant a hand stops moving. Filter settling
  and the 180 ms visual transition can add a short visible delay.
- uint32_t timestamp wrap is supported. At bit-bang's ~12 Hz, timing remains
  elapsed-time-based but fast motion can alias; hardware I2C is recommended.

## Renderer (`src/robot_eyes.c`)

Default rendering keeps the original narrow/tall eye shapes and eight-second
loop. Squinting uses the same pose timeline at 12 px open / 2 px blink height,
with glance displacement reduced to one third.

Confusion draws two two-turn Archimedean spirals, radius 1–18 px and a two-pixel
stroke. Left and right rotate in opposite directions at one revolution per second,
using 128 angle phases, a 33-entry quarter-wave integer sine table and joined
line segments. The spirals replace the eyes entirely: no pill-shaped background
or rounded outline is drawn behind them. Phase follows elapsed time, not the
number of completed transfers.

State changes use a 180 ms ordered monochrome cross-dissolve from a framebuffer
snapshot. Escalation can interrupt an in-progress dissolve smoothly. Returning
to default resets its pose timeline to centered/open. The extra snapshot uses
1 KB RAM; current hardware firmware links at about 7 KB text and 3.7 KB static RAM,
well within 64 KB flash / 20 KB RAM (with additional stack headroom required).

All poses, spiral rotations and transition frames remain inside x=13–114,
y=8–55. The OLED still sends **612 bytes per update** at 400 kHz with DMA.
No larger window or bus overclock was needed.

## Validation

Run all host tests with address/undefined-behavior sanitizers:

```sh
make test
```

Replay results from committed CSV fixtures in `tests/data/`:

| Recording | Default samples | Squinting samples | Confused samples |
|---|---:|---:|---:|
| Quiet | 587 | 0 | 0 |
| Gentle wobble | 112 | 783 | 0 |
| Deliberate shake | 8 | 52 | 830 |

These recordings were used for tuning, so this is regression coverage, **not an
independent live validation set**. Synthetic tests cover escalation, no downgrade,
exact 1999/2000 ms quiet boundaries, renewed motion, missing samples, long gaps,
duplicates, stale fallback, timestamp wrap, isolated full-range spikes at 60/12 Hz,
slow baseline changes and signed input extremes.

Renderer tests cover all states across eight seconds at 8 ms steps, including
transitions; verify the partial-update bounds, spiral periodicity/opposite rotation,
many distinct phases, no old rounded background, interrupted transitions and wrap.
Existing sensor, UART, scheduling and region-DMA tests remain enabled.

Both hardware and bit-bang builds compile warning-free with no C library. Header
dependency files are generated so header changes correctly rebuild consumers.

### First reactive firmware hardware check

Hardware I2C reactive firmware was flashed and read for 12 seconds while the
operator was asked to leave the breadboard still:
- 707 valid samples, 60.00 samples/s, 16/17 ms intervals.
- 60.0 FPS; zero reported sensor/display errors and no not-ready reads.
- State stayed default, filtered energy around 0–5 mg^2 in status lines.
- Maximum observed flush 15 ms, sensor read 1 ms on the coarse millisecond clock.
- Saved locally at `build/motion-stationary.csv` / `.log`.

The acquisition-only flash image was backed up to `build/pre-motion-flash.bin`
before flashing. Hardware is left running the reactive hardware-I2C build.

### First live reaction test

Four serial captures were made while cueing wobble/rest/shake/rest. All status
lines reported **60.0 FPS**, zero sensor/display errors, no not-ready reads and
15 ms maximum observed flush, including lines in squinting and confused states.

| Capture | Valid samples | Timestamp range (ms) | Logged behavior |
|---|---:|---|---|
| `build/live-wobble.csv` | 359 | 2814691–2820658 | Entered squinting at 2816858 |
| `build/live-wobble-rest.csv` | 293 | 2828191–2833161 | Initially confused; returned default at 2832475 |
| `build/live-shake.csv` | 360 | 2854708–2860691 | Data looked stationary; remained default |
| `build/live-shake-rest.csv` | 291 | 2870975–2875941 | Movement/initial confusion; returned default at 2874775 |

**Timing limitation:** there are gaps between captures, and actual movement
appeared in nominal rest windows. The short chat-cued recordings did not reliably
line up with operator actions. Do not label these as clean wobble/shake calibration
fixtures or conclude that confusion in the first rest capture was a false wobble
trigger. Both reaction states and default recovery were logged on the board, but
clean trigger discrimination and exact physical-stop recovery timing remain
unverified. Some malformed serial output was excluded from the valid CSVs.

For the next test, prefer one longer, already-armed continuous recording spanning
all cues, with operator acknowledgement of start/stop and visual feedback.

**Operator visual feedback:** after the live test, the user confirmed seeing
both squinting eyes and spinning spirals, and that the return to normal felt right.
This confirms the initial visual behavior, not a measured two-second physical-stop
interval or a clean live wobble-versus-shake discrimination test.

**Still needs testing:** clean live sensitivity/discrimination, exact quiet
recovery timing, slow-tilt rejection and optional fault injection. The reactive
bit-bang build has compiled but has not been flashed in this step. Analog
wiring/pull-ups and absolute sensor bias/gain remain unverified.

## Logs and next live test

Raw `accel,ms,x_raw,y_raw,z_raw` lines remain unchanged. State changes add:

```text
motion,<ms>,<default|squinting|confused>,<energy_mg2>
```

Four-second status lines include current state and energy. Close other serial
readers before using `tools/capture_accel.py`.

For the next coordinated live test, gently wobble and observe squinting, set down
and observe the quiet return, then deliberately shake and observe spirals. Keep
all components secured; a violent shake is neither necessary nor safe. Capture
these as independent live validation before adjusting sensitivity or calling the
visual design final. Slow rotation can be tested afterward for false triggers.
