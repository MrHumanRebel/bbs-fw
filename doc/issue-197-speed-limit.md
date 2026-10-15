# Issue #197: low throttle speed limits overshoot by several km/h

## Cause reproduced

The old limiter places its ramp symmetrically around the configured limit.
With a 6 km/h setting and a 3 km/h interval it starts reducing current near
3 km/h, still requests about half the original current at 6 km/h, and reaches
its minimum only near 9 km/h. That minimum was 1%, not zero.

The normal temporal current ramp-down can additionally retain an earlier, higher
current even after the speed limiter computes a lower demand. This must also be
addressed; moving the ramp endpoint alone is insufficient.

The pre-patch code fails tests for zero current at the limit, a ramp ending at
zero, immediate cutoff through `app_process()`, the global throttle limit and
the PAS throttle speed override.

## Change

The ramp now ends at the selected limit. Its width is the smaller of the
configured interval and half the limit. This preserves starting current at very
low speed settings instead of spreading a fixed 3 km/h ramp over nearly their
entire usable range.

| Configured limit | Full current until approximately | Zero current at measured speed |
| --- | --- | --- |
| 1 km/h | 0.5 km/h | 1 km/h |
| 4 km/h | 2 km/h | 4 km/h |
| 6 km/h | 3 km/h | 6 km/h |
| 25 km/h | 22 km/h | 25 km/h |
| 50 km/h | 47 km/h | 50 km/h |

Thresholds use the existing wheel-RPM conversion, so finite RPM resolution can
shift these approximate values slightly. The current falls linearly to zero in
the ramp interval. At or above the selected threshold, the limiter always
requests zero and reports active limiting, including when the incoming demand
is already zero. This resets the temporal current ramp-down instead of allowing
its old state to restore drive current. The ramp-up remains in place.

Unsigned speed readings are compared without conversion to signed 16-bit values:
a very high reported speed must trigger limiting rather than wrap negative and
bypass it. A ramp width rounded to zero gives a hard cutoff without division by
zero.

Selection of the applicable limit is unchanged: ordinary assist-level limits,
the global throttle limit and the PAS throttle override all use the corrected
endpoint. Existing settings where zero means no configured limit retain that
meaning. If the speed sensor is disabled, speed limiting remains disabled.
This change does not attempt to resolve the separate limit-selection report
in #195.

## Verification and limits

`sh tests/run.sh` runs seven new speed-limit scenarios for each of BBS02, BBSHD
and TSDZ2. They cover the endpoint and extreme readings, monotonic ramping,
immediate cutoff and restart through `app_process()`, global throttle selection,
PAS throttle override, disabled limits, and limits from 1 to 50 km/h including
the zero-width boundary. Previous regression suites remain included.

All three firmware targets build and their Intel HEX checksums validate. The
current BBS02 image includes the earlier #114 and #120 patches and optional
#104 voltage compensation. No new configuration format or UI is needed.

Physical speed is not actively braked. Wheel-sensor pulse spacing, the motor
controller's response and inertia can still cause transient overshoot; downhill
coasting can exceed the setting with zero motor current. This patch eliminates
the deliberate drive above the configured measured-speed limit. It is not a
certification of a precise legal maximum speed. Hardware testing is still needed
with the correct wheel size and sensor pulse count, at different loads and
battery voltages, particularly for a 6 km/h throttle setting.
