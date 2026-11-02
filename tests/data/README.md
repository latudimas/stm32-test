# Prototype motion recordings

These are the actual, syntactically valid raw samples used for initial classifier
tuning and regression replay. Captured from the user's breadboard-mounted MPU9250
(WHO_AM_I 0x71), +/-2 g nominal range, 41 Hz DLPF, 100 Hz internal sensor rate,
latest-only firmware acquisition at about 60 Hz. Units: timestamp milliseconds,
signed raw axis counts; nominal sensitivity 16384 counts/g. Absolute bias/gain
is uncalibrated. CSV headers are `ms,x_raw,y_raw,z_raw`.

- `quiet.csv`: 587 samples from the final 10-second logging validation after
  requesting that the breadboard be left still.
- `wobble.csv`: 895 samples, operator cued for gentle wobble over 15 seconds.
- `shake.csv`: 890 samples, operator cued for controlled deliberate shake over
  15 seconds. Cues/capture startup are not synchronized exactly to physical onset.

Wobble/shake captures have a few missing rows after malformed serial lines were
excluded; preserve their real timestamps rather than assuming uniform sampling.
No near-saturation samples were observed. See `docs/mpu9250-acquisition.md` for
capture summaries and limitations. Complete original serial logs remain local
under `build/`; these CSV copies keep the replay tests independent of that folder.

These are **tuning fixtures**, not an independent validation dataset or universal
shake definitions. `tests/motion_test.c` checks that quiet never reacts, wobble
never confuses, and deliberate shake eventually confuses. Independent live tests
and sensitivity adjustment on other mounting arrangements are still needed.
