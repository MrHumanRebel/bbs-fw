# Additional RAM optimization after #86

The original #86 optimization is already in master: commit `6ebc943` reduced
UART ring buffers by 896 bytes. Today's BBS rings total 128 bytes, not 1024.
Assist-level throttle enable is already a bit in the existing flags byte; there
is no separate 20-byte array to eliminate.

## Additional changes

1. Size the external-command assembly buffer from the largest supported request:
   `sizeof(config_t) + 5`, or 159 bytes for version 5, instead of 192. This saves
   33 bytes on every target without reducing any supported packet size.
2. Dispatch each complete command before reading its successor. A maximum-size
   configuration followed immediately by another request no longer requires
   space for both. The parser therefore needs no leftover-buffer compaction.
   Timestamps are read where used instead of retaining a 32-bit local across
   every nested handler call; this avoids extra scarce 8051 direct-RAM spills.
3. Store 17 private scalar flags in 8051 bit-addressable storage rather than
   XRAM bytes. These are application, battery, throttle and timer states, not
   fields of a transmitted or persisted structure. `RAM_BOOL` falls back to
   ordinary `bool` on STM8 and host builds.
4. Store the two volatile 8051 UART sending flags as bits instead of direct-RAM
   bytes. The two freed direct-RAM bytes offset the expanded bit-storage area,
   preserving the available stack region.

UART ring sizes and usable capacity remain unchanged: main RX 64, main TX 32,
motor RX 16, motor TX 16 (each ring reserves one slot). RX/TX algorithms, baud
rates and the STM8 driver are unchanged. Configuration version 5 remains 154
bytes. No EEPROM migration, checkbox removal or feature removal is required.

## Measured before and after

Both builds use SDCC 4.5.0 and this workspace's complete prior feature set,
including #104, #114, #120, #197 and #207. These numbers are not a comparison to
unmodified upstream master. Every target was cleaned before building.

| Target | RAM area | Before | After | Saved |
| --- | --- | ---: | ---: | ---: |
| BBS02 | XRAM (`XSEG + XISEG`) | 953 B | 903 B | 50 B |
| BBSHD | XRAM (`XSEG + XISEG`) | 967 B | 917 B | 50 B |
| TSDZ2 | static DATA | 616 B | 583 B | 33 B |

The 8051 bit-storage area grows from 2 to 4 bytes while the UART direct-byte
flags disappear. Available stack space remains 147 bytes on BBS02 and 135 bytes
on BBSHD. This is linker-reserved stack capacity, not a runtime stack high-water
measurement. Free XRAM is now 889/1792 bytes on BBS02 and 2923/3840 on BBSHD.

Linked code/constant sizes also decrease: BBS02 24335 -> 23934 bytes, BBSHD
25891 -> 25490 bytes, TSDZ2 15049 -> 15008 bytes. No functionality is disabled to
achieve these figures. Raw before/after map reports are retained with the
generated build artifacts for review.

## Validation

Run `sh tests/run.sh`. The new tests compile the actual BBS UART driver against
mock registers and exercise both channels for 100 full-buffer RX/TX rounds,
including FIFO ordering, ring wrap, receive overflow rejection, TX completion,
simultaneous receive/transmit interrupts and close/flush behavior. They do not
claim to simulate real interrupt timing during a blocked full-TX write.

The real external-command parser is also exercised with:

- a complete 159-byte configuration immediately followed by telemetry;
- the same maximum-size configuration arriving one byte at a time;
- corrupt configuration rejection without saving or mutating configuration;
- coalesced Bafang PAS/mode writes and current reads;
- the existing virtual-control, timeout and telemetry checks.

The earlier application/sensor regression tests and C# binary/XML configuration
round trips pass. All three firmware targets build and the generated Intel HEX
checksums validate. No new Windows tool build is needed because its protocol and
configuration layout are unchanged; the #207 tool is included in the bundle.

Physical UART timing and controller operation still require hardware regression
testing. Host tests and a successful linker report do not establish on-bike
behavior or the runtime stack high-water mark.
