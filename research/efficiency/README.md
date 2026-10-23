# Adaptive Efficiency — FINAL research package (3 motor profiles)

**Scope: BBS02B · BBSHD · Tongsheng TSDZ2 ONLY.** No standalone BBS02 or TSDZ2B motor target / data profile is included. TSDZ2B bench observations are retained solely as an explicitly labeled external reference for TSDZ2; older-family BBS measurements are similarly provenance-labeled as BBS02B engineering references. **Firmware compile targets:** BBS02 (for hardware-verified compatible BBS02B), BBSHD, TSDZ2. Compilation for BBS02 does **not** imply that every BBS02B revision is compatible.

## Instructions
1. Extract this archive into the repository root (ZIP contains `research/efficiency/`).
2. Read `research/efficiency/CODEX_TASK.md` and `docs/IMPLEMENTATION_SPEC.md`.
3. Run `python research/efficiency/tools/validate_dataset.py` or `py -3 ...` on Windows.
4. Develop against the CURRENT working tree, not an outdated uploaded repository snapshot. Inspect [PR #9](https://github.com/MrHumanRebel/bbs-fw/pull/9) if needed.

## Exact three profile tables

| Profile | Table | Evidence |
|---|---|---|
| BBS02B | `data/bbs02b_factory_reference_93points.csv` | 93 factory 48 V/750 W load-sweep points, accepted **engineering proxy**, revision unknown |
| BBSHD | `data/bbshd_factory_4_landmarks.csv` | Four manufacturer-labeled points in 48 V/1000 W plot |
| TSDZ2 | `data/tsdz2_external_bench_9points.csv` | Nine **TSDZ2B** comparative shop test runs; **TSDZ2 external reference only** |

`data/motor_parameters_3_profiles.csv` contains properties assigned to these same **three target profiles**, with indirect family measurements explicitly flagged. `data/research_dataset.json` is the machine-readable aggregation; `data/sources.csv` has citation URLs; `data/dataset_manifest.json` has data lineage. Supporting documentation: `docs/PROFILE_BBS02B.md`, `PROFILE_BBSHD.md`, `PROFILE_TSDZ2.md`, `RESEARCH_REPORT_HU.md`, `IMPLEMENTATION_SPEC.md`, `TEST_MATRIX.md`, and `data/DATA_DICTIONARY.md`.

### Interpretation that must NOT be lost
- BBS02B proxy data are **not** independently verified on a BBS02B controller/motor revision. The 93 rows are **one correlated load sweep**, NOT a torque × rpm 2D efficiency map. Accept the user's decision to USE the 93 rows for BBS02B, but preserve provenance flags.
- The BBSHD curve has only 4 labeled numerical anchors; missing mechanical power is unknown, not zero.
- The TSDZ2 external bench table is based on a **TSDZ2B**, a distinct revision and stock/OSF/cooling comparison, not a direct test of this repo's TSDZ2 firmware. Its ~70 °C endpoint is a *test marker*, never a safety setting.
- No credible Wh/km range gain is known yet. Never substitute lower electric power, lower vehicle speed or increased rider effort for equal-condition motor-efficiency improvement.
- No FOC/PWM modification on BBS02B/BBSHD from the STC firmware: the NEC motor drive is separate. Low-level TSDZ2 FOC changes require separate measured hardware validation.

All numeric values are linked to source IDs. Source transcriptions and illustrations are derived summaries, not reproductions of whole copyrighted documents. The complete vendor PDFs remain accessible at the URLs in `data/sources.csv`.
