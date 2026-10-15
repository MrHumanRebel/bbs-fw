# Automatic builds and releases

`.github/workflows/release.yml` publishes releases only in **MrHumanRebel/bbs-fw**.
It runs after every push/merge to `main` or `master` (the repository currently
uses `master`), and can be manually re-run with **Actions → Build and release →
Run workflow** on either branch. It does not publish from feature branches,
other forks or upstream repositories.

## Versions

The first reservation is **v2.0.0**, followed by v2.0.1, v2.0.2, etc. The same
version is embedded in the firmware responses and Windows assembly/file
metadata, and used for asset filenames and the GitHub tag/release. Source
defaults start at 2.0.0; subsequent CI versions are injected at build time.
Configuration schema version 5 is independent and remains unchanged.

After host tests pass, `ci/release_version.py` atomically reserves a lightweight
tag on the exact triggering commit. Concurrent commits retry tag collisions;
they do not overwrite a tag or cancel each other's release builds. Re-running
the same commit reuses its existing release tag. Existing stable v2+ tags are
respected, so a manually created higher version becomes the next baseline.
Pre-release tags and old v1 releases do not alter the 2.0.0 starting floor.

The firmware protocol stores each version component in one byte. Patch 255
therefore rolls over to the next minor version, and minor 255 to the next major.
No extra version-bump commits are pushed to the main branch, avoiding workflow
loops. To reproduce a tagged firmware build locally, use
`make -j1 all TARGET_CONTROLLER=BBS02 VERSION=2.0.1` (substitute the tag version).
Use matching `Version`, `AssemblyVersion` and `FileVersion` properties for a
manual Windows publish, as shown in the workflow.

## Pipeline and downloads

1. Run the release-automation unit/integration tests, C firmware host regression
   tests and .NET configuration/telemetry tests.
2. Reserve/reuse the tag for that commit.
3. Build BBS02, BBSHD and TSDZ2 independently in Debian 13 with SDCC
   `4.5.0+dfsg-1`. Each target has a clean build and HEX validation.
4. Build the Windows x64 tool on `windows-2022`, using .NET SDK 8 to publish the
   existing .NET 6 Windows project as a **self-contained single-file EXE**.
   Downloaders need no separately installed .NET runtime. Native libraries may
   be extracted automatically by the single-file host at application startup.
5. Download all four build artifacts, validate every HEX checksum and the
   Windows x64 PE header, and generate `SHA256SUMS`.
6. Create/resume a draft GitHub Release and upload all assets. Verify uploaded
   filenames/sizes and check that the tag still references the built commit.
   Only then publish the complete release with generated change notes. GitHub's
   normal version/date rules determine the latest release.

For v2.0.0 the public Release contains:

- `bbs-fw-v2.0.0-BBS02.hex`
- `bbs-fw-v2.0.0-BBSHD.hex`
- `bbs-fw-v2.0.0-TSDZ2.hex`
- `BBSFWTool-v2.0.0-win-x64.exe`
- `SHA256SUMS`

Build artifacts are also downloadable from their Actions run. Always choose
the HEX matching the controller; no automatic flashing is performed.

## Failure recovery and permissions

A failed test does not allocate a tag. A later build failure leaves the reserved
tag but creates no public Release. An upload failure can leave a draft Release.
Re-run the failed workflow/jobs to rebuild the same version and resume its
draft. A published release with all expected assets is left untouched on rerun;
a published release missing assets fails for investigation rather than silently
replacing already public downloads.

The workflow uses the automatically supplied `GITHUB_TOKEN`; no personal token
or repository secret is needed. Only version reservation and publication have
`contents: write`. The other jobs have read-only contents access. The repository
must allow GitHub Actions and the requested token write permissions. Immutable
action commit SHAs are pinned in the workflow; update them deliberately.

Merging this workflow to the repository's primary branch activates automation.
No remote release/tag is created merely by running the local tests. The tests
reserve tags only in temporary local bare repositories. Successful automated
builds do not establish physical controller behavior or interactive Windows
operation; those checks remain separate.
