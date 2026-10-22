# Windows UI

BBS-FW Studio uses the existing WPF application and view models with a shared,
vector-based visual layer. No UI packages, web runtime or embedded
fonts are added. The generated app emblem is included as a multi-resolution ICO
and a small PNG header image. The six workspaces, existing commands, legacy configuration import, telemetry
and virtual-throttle handlers are retained. Current writes use schema 6.

## Appearance

- Dark and light palettes are switched live with the header button. The choice
  is saved in `%LOCALAPPDATA%\BBSFWTool\theme.txt`; an unwritable preference file
  does not prevent use of the app. The initial theme is dark.
- Sidebar navigation, rounded cards and inputs, telemetry tiles, connection
  status, and severity colors share dynamic theme resources.
- A 160 ms navigation fade respects Windows' client-area animation preference.
  It changes presentation opacity only. Input release, STOP and firmware command
  expiry remain independent of animation.
- Cards adapt to the available width. System cards use balanced columns and
  never require horizontal scrolling. Vertical scrollbars appear only when
  content exceeds the viewport. Initial window dimensions fit the work area.
- Standard window chrome, menu access keys, tab navigation, checkboxes, drop-down
  selection and validation adorners remain WPF controls.

## Distribution size

Single-file compression is enabled and ReadyToRun is disabled. The standalone release
contains the .NET runtime and works without a separately installed runtime.
The compact release uses the same code and UI but omits the runtime; install
the .NET 10 Desktop Runtime (x64) once to use it. Single-file compression is
disabled for compact builds because .NET only supports it for self-contained
bundles.
The release workflow inherits the compression setting from the project.
The local Windows x64 publish decreased from approximately 148 MiB to 66 MiB.
Compression trades some launch/extraction work for a smaller download; it does
not remove controller functionality. WPF reflection and XAML prevent safe use
of indiscriminate trimming, so trimming is not enabled.

## Validation

Cross-publish on Linux with Windows targeting enabled checks XAML and C#
compilation. Host tests check configuration migration and curve arithmetic.
The Windows-only layout suite renders the actual WPF content offscreen, avoiding
CI desktop window-size caps. It covers all six pages in both palettes at three
window sizes and uploads preview screenshots through `windows-ui.yml`.

Interactive appearance, physical multi-monitor DPI behavior, saved-theme
restart and real mouse/Space throttle release must additionally be checked on
Windows. Cross-compilation cannot verify those behaviors or hardware I/O.
See [adaptive controls](adaptive-studio-controls.md) for schema compatibility,
ramp semantics and remaining validation limits.
