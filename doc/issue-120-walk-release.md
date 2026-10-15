# Issue #120: unexpected acceleration after releasing walk assist

The issue reports a BBS02B accelerating after the minus button is released,
especially when walk mode is used to display temperature after power-on.

## Reproduced software defects

The current source already contains a one-second post-walk power block from
issue #126. That block alone leaves two gaps:

1. `app_process()` sets the requested current to zero but still applies its normal
   ramp-down. A nonzero walk current can therefore survive while the target
   cadence changes from walk's 15% to the restored PAS level's higher value.
2. After the second expires, ordinary PAS can accept pulses from pedals still
   driven by the motor. A timeout does not establish that the rider started
   pedaling deliberately.

The old absolute-deadline comparison also releases the block too early if it
straddles the 32-bit millisecond clock wrap.

The production-code host tests fail on the pre-patch immediate-stop,
wait-for-pedal-stop and clock-wrap scenarios. This establishes software defects;
it does not prove that every physical report in #120 has the same cause.

## Change

- Leaving enabled motor walk assist sets both the existing one-second power
  block and a PAS re-arm latch.
- A power block now requests zero current and zero cadence and bypasses the
  normal current ramp-down, resetting that ramp's stored request too. The motor
  command is stopped at the next application cycle (normally 5 ms); physical
  response also depends on the controller and motor.
- PAS and pretension remain inhibited until neither forward nor backward
  pedaling is detected. A later fresh pedaling session can request assist again.
- Old walk pulses cannot engage cruise. After pedal stop, the normal deliberate
  pedal/throttle cruise activation remains available.
- The throttle retains its existing behavior after the one-second block, even
  if the PAS re-arm latch has not yet cleared.
- Elapsed-time subtraction makes the one-second block work across clock wrap.

No configuration or EEPROM layout changes are required. The latch applies only
after exiting enabled motor walk assist. If `Use Push Walk` is disabled, the
minus-button temperature display retains its existing behavior and does not
start a new post-walk block.

## Validation and hardware check

Run `sh tests/run.sh`. There are six new walk scenarios on each of BBS02, BBSHD
and TSDZ2: immediate release, waiting for pedal stop in both directions, throttle
use, clock wrap, display-only walk and cruise re-activation. The existing PAS
voltage-compensation, speed-sensor and display-current tests also run.

All three firmware builds and HEX checksums were validated. The BBS02 image
includes the earlier speed-filtering and optional voltage-compensation changes.
The Windows configuration tool is unchanged from the previous feature build.

Physical validation is still required. With the bicycle safely supported and
the driven wheel clear, reproduce the cold-start sequence: enable temperature
display and motor walk, power on, hold minus, then release. Check for immediate
loss of drive, allow the pedals to stop, then check normal PAS restart. Repeat
with motor walk disabled to confirm temperature-only use. Host tests do not
verify controller command timing or mechanical stopping on a real BBS02B.
