# Issue #123: 800S power display stops at 999 W

Status: investigated, not fixed. The affected 800S and a stock controller trace
are not available in the cloud workspace.

## Verified in the current source

The display requests current with `11 0A`. In
`src/firmware/extcom.c`, `process_bafang_display_read_current()` replies with a
one-byte current in 0.5 A units followed by the same byte as its checksum:

```c
uint8_t amp_x2 = (uint8_t)((motor_get_battery_current_x10() * 2) / 10);
uart_write(amp_x2);
uart_write(amp_x2);
```

This response contains current, not watts. It has no discontinuity corresponding
to 999 W. The production-handler tests verify, among other cases:

| Measured current | Response | Example voltage | Corresponding power |
| --- | --- | --- | --- |
| 20 A | `28 28` | 56 V | 1120 W |
| 20.5 A | `29 29` | 48 V | 984 W |
| 21 A | `2A 2A` | 48 V | 1008 W |
| 26 A | `34 34` | 48 V | 1248 W |
| 33 A | `42 42` | 48 V | 1584 W |

The voltages in this table illustrate the arithmetic; they are not transmitted
in this response. The test does not establish the 800S calculation or rendering.
The issue reports that the same 800S shows four digits with stock firmware,
while other displays exceed 1500 W with this firmware. This makes a missing or
different display-protocol transaction a plausible explanation, not a proven
cause.

The ordinary BBS02 motor-current conversion was also inspected: its 16-bit
`data * 100` intermediate does not overflow within the supported 0..33 A range
with the configured 5.6 ADC steps/A. Neither that conversion nor the outgoing
current encoding explains a 999 W ceiling at normal operating currents.

## Evidence needed to implement a verified fix

Either supply the exact original Bafang firmware image used successfully on this
BBS02B, or capture both directions of its controller/display UART. The stock
image allows static investigation; paired traces provide the most direct
comparison without guessing undocumented response fields.

For a trace, use a passive logic analyzer with the appropriate input ratings.
Capture controller TX, display TX and their common ground separately. Do not
connect the battery-voltage conductor to an analyzer input. Decode the display
UART as 1200 baud, 8 data bits, no parity, 1 stop bit. Preserve raw bytes,
timestamps and direction, including transactions the decoder does not recognize.
The Arduino logger in this repository actively proxies traffic and can inject
event-log requests; prefer a passive capture for this comparison.

Capture stock firmware from power-on, including a controlled transition from
below 999 W to a displayed four-digit value. Record the displayed value and its
approximate time in the trace. Repeat with bbs-fw using the same display and
settings. Compare all requests and responses, including startup, unknown
opcodes and message timing, rather than only the `11 0A` response.

Do not change current scaling, checksum, response length or unknown response
fields without evidence of what this 800S expects. Such guesses could produce
an inaccurate power reading or break working displays. Once a difference is
identified, add the actual captured request/response as a regression fixture and
verify the patched firmware on the affected 800S.
