# BBS-FW

**Open-source motor controller firmware for Bafang BBSHD, BBS02B and TongSheng TSDZ2.**

A configurable alternative to stock firmware, with refined pedal assist and throttle behavior, adjustable riding profiles, battery and motor protections, and a companion Windows application for configuration and live diagnostics.

[Download releases](https://github.com/MrHumanRebel/bbs-fw/releases) · [Flashing guide](https://github.com/danielnilsson9/bbs-fw/wiki/Flash-Firmware-%28BBS02-%26-BBSHD%29) · [Configuration guide](https://github.com/danielnilsson9/bbs-fw/wiki/Configuration-Tool) · [Release documentation](doc/automatic-releases.md)

> **Before you begin:** Flashing third-party firmware can permanently damage an incompatible controller and may void its warranty. **Never flash or configure the controller while the e-bike battery is charging.** Verify the controller hardware before proceeding.

## Why BBS-FW?

### Ride control

- **Custom assist profiles** — Configure current, speed and cadence limits for individual assist levels, with separate Standard and Sport profiles.
- **Refined throttle and PAS** — Smooth transitions, configurable throttle response and pedal-assist behavior, plus cruise and walk-assist modes.
- **Predictable speed limiting** — Adjustable limits per assist level, with gradual power reduction ending at the configured limit.
- **More flexible setups** — Optional wheel speed sensor and support for lower-voltage battery configurations.

### Protection and diagnostics

- **Battery management** — Adjustable low-voltage cutoff, voltage calibration and gradual current reduction as the battery approaches its limit.
- **Thermal protection** — Progressive power limiting using available motor or controller temperature sensors.
- **Adaptive Power PAS** — Select a per-level cadence-aware watt budget with unchanged current and safety ceilings. [Behavior and limitations](doc/adaptive-power-pas.md).
- **Voltage-compensated PAS** — Optional global compensation to help maintain more consistent electrical input power as battery voltage drops.
- **Live telemetry** — Monitor wheel speed, battery voltage, current, electrical power and controller status through the Windows tool.

### Improvements in this fork

- **Virtual Display** with live telemetry and optional, safety-interlocked bench controls.
- **Control refinements** covering walk-assist release, speed-limit enforcement and wheel speed sensor signal filtering.
- **Firmware and protocol tests** for critical control paths, telemetry and configuration compatibility.
- **Automated, versioned releases** with independently built firmware targets, a standalone Windows tool and SHA-256 checksums.

## Downloads

Releases for this fork are published at **[MrHumanRebel/bbs-fw/releases](https://github.com/MrHumanRebel/bbs-fw/releases)**. The automated release series starts at **v2.0.0** once the release workflow runs successfully on `master` or `main`.

| Release file | Purpose |
| --- | --- |
| `bbs-fw-vX.Y.Z-BBSHD.hex` | Bafang BBSHD firmware |
| `bbs-fw-vX.Y.Z-BBS02.hex` | Bafang BBS02/BBS02B firmware |
| `bbs-fw-vX.Y.Z-TSDZ2.hex` | TongSheng TSDZ2 firmware |
| `BBSFWTool-vX.Y.Z-win-x64.exe` | Standalone Windows x64 tool, runtime included |
| `BBSFWTool-vX.Y.Z-win-x64-compact.exe` | Small Windows x64 tool; requires .NET 10 Desktop Runtime (x64) |
| `SHA256SUMS` | Download integrity checksums |

Both Windows downloads are single-file executables with the same functionality. The **compact** download requires the [.NET 10 Desktop Runtime (x64)](https://dotnet.microsoft.com/download/dotnet/10.0), installed once. Choose the **standalone** download to run without a separate .NET installation. Select the firmware file for your **actual controller**, not the display or motor label alone.

## Hardware compatibility

| Controller | Supported hardware | Notes |
| --- | --- | --- |
| **BBSHD V1.4** | `STC15W4K56S4` | PCB may be marked V1.3; check the controller sticker. |
| **BBSHD V1.5** | `IAP15W4K61S4` | PCB may be marked V1.4; check the controller sticker. |
| **BBS02B** | `STC15F2K60S2`, `IAP15F2K61S2` | **Compatibility issues have been reported**, particularly on older controllers. |
| **TSDZ2A / TSDZ2B** | STM8-based original controllers | Requires a custom cable to connect a Bafang-protocol display. |

**Important compatibility notes**

- **BBS02A is untested and not recommended.** Do not assume that support for BBS02B extends to BBS02A.
- Do not flash a BBSHD revision or MCU that is not listed above without separately verifying compatibility.
- Displays must support the **Bafang display protocol**. Third-party or replacement controllers sold in some kits may not be compatible.
- The standard Bafang configuration utility is **not compatible** with this firmware. Use the included BBS-FW configuration tool.

## Getting started

1. **Identify the controller.** Confirm its revision and MCU against the compatibility table.
2. **Download the matching firmware** from [Releases](https://github.com/MrHumanRebel/bbs-fw/releases). Keep a backup of the original firmware and settings if available.
3. **Flash the controller** using the appropriate procedure. For BBS02/BBSHD, follow the [original flashing guide](https://github.com/danielnilsson9/bbs-fw/wiki/Flash-Firmware-%28BBS02-%26-BBSHD%29) and its MCU-specific settings. TSDZ2 requires its own target-specific flashing method.
4. **Connect the Windows configuration tool** using the supported programming adapter, then read and review the controller settings. See the [configuration guide](https://github.com/danielnilsson9/bbs-fw/wiki/Configuration-Tool).
5. **Verify operation safely** with the driven wheel off the ground before riding. Check the brake cutoffs, PAS, throttle, speed limits and motor stopping behavior.

Use a **matching firmware and tool release**, especially when using newer functions such as Virtual Display.

## Windows configuration tool

The companion Windows application provides a graphical interface for managing firmware settings and examining controller behavior.

- Connection management and firmware identification
- System settings, assist levels and calibration
- Read, write and reset of stored configuration
- Event log inspection
- **Virtual Display:** live speed, voltage, current, power, PAS level and controller status
- **Bench controls:** optional virtual PAS and throttle input for controlled testing

Bench controls must only be used with the **driven wheel clear of the ground**. Releasing the throttle sends a stop command; the firmware also expires unrefreshed control commands after **1.5 seconds**. These safeguards do not replace physical isolation or hardware testing.

Read the [Virtual Display and bench-control documentation](doc/issue-207-virtual-display.md) before using this feature. The automated tests do not constitute physical controller or on-bike validation.

![BBS-FW Windows configuration tool](https://i.postimg.cc/zGjMnCG3/Untitled.jpg)

## Build and test

Firmware builds require **SDCC**, `make` and `packihx`. Build one controller target at a time from the repository root:

```bash
make -C src/firmware clean
make -C src/firmware -j1 all TARGET_CONTROLLER=BBSHD
```

Replace `BBSHD` with `BBS02` or `TSDZ2` as needed. The output is `src/firmware/bbs-fw.hex`; clean between target builds to avoid mixing object files.

Install the .NET 10 SDK to build the Windows tool and run configuration tests.
Run the firmware regression tests and .NET configuration tests:

```bash
sh tests/run.sh
dotnet run --project tests/ConfigurationTests --configuration Release
```

The Windows tool is built from `src/tool/bbs-fw-tool.csproj`. See [automatic releases](doc/automatic-releases.md) for the reproducible CI toolchain, Windows publishing steps, versioning and failure recovery.

## Documentation

| Guide | Description |
| --- | --- |
| [Flashing firmware](https://github.com/danielnilsson9/bbs-fw/wiki/Flash-Firmware-%28BBS02-%26-BBSHD%29) | BBS02/BBSHD programming procedure and hardware checks |
| [Configuration tool](https://github.com/danielnilsson9/bbs-fw/wiki/Configuration-Tool) | Connecting and tuning the controller |
| [Automatic releases](doc/automatic-releases.md) | Build pipeline, versions, release assets and troubleshooting |
| [Virtual Display](doc/issue-207-virtual-display.md) | Live telemetry, bench controls and safety interlocks |
| [Regression tests](tests/README.md) | Test coverage and local execution |

For technical investigations and implementation notes, see the other documents in [`doc/`](doc/).

## Known limitations

- **BBS02 compatibility is not fully established.** Older BBS02 controllers may be unstable or incompatible.
- **BBS02A is not validated.** Do not use this firmware on an unverified controller.
- The reported **999 W display limitation** on some 800S displays has not been resolved by the new Virtual Display feature. See the [investigation](doc/issue-123-display-power.md).
- CI and host-side regression tests do **not** replace testing on real controllers, displays and motors.

## Project and license

This project builds on the original [BBS-FW by Daniel Nilsson](https://github.com/danielnilsson9/bbs-fw) and the [wischbgr/bbs-fw fork](https://github.com/wischbgr/bbs-fw). Thanks to the original author and contributors for their work.

Distributed under the **[GNU General Public License v3.0](LICENSE)**. Use and modifications are at your own risk; always comply with local e-bike laws and equipment limits.

If you would like to support the original development, you can [donate to Daniel Nilsson](https://www.paypal.com/donate/?business=LVAYFCMQYN8F4&no_recurring=0&item_name=BBSHD-FW&currency_code=USD).
