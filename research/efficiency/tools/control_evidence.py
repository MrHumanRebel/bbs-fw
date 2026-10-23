#!/usr/bin/env python3
"""Offline source-qualified summary; never emit production control tables."""
import csv
import json
from pathlib import Path

DATA = Path(__file__).resolve().parents[1] / 'data'


def rows(name):
    with (DATA / name).open(encoding='utf-8-sig', newline='') as stream:
        return list(csv.DictReader(stream))


def summary():
    aggregate = json.loads((DATA / 'research_dataset.json').read_text())
    tables = {name: rows(name) for name in aggregate['tables']}
    if tables != aggregate['tables']:
        raise ValueError('Aggregated data diverged from source CSV')
    profiles = aggregate['manifest']['targets']
    if profiles != ['BBS02B', 'BBSHD', 'TSDZ2']:
        raise ValueError('Unexpected optimization profile')
    bbs = tables['bbs02b_factory_reference_93points.csv']
    hd = tables['bbshd_factory_4_landmarks.csv']
    ts = tables['tsdz2_external_bench_9points.csv']
    result = {
        'production_lookup_tables': False,
        'verified_efficiency_gain': False,
        'sources': tables['sources.csv'],
        'profiles': {}
    }
    for profile, points, evidence in (
        ('BBS02B', bbs, 'unversioned correlated factory sweep; accepted engineering proxy'),
        ('BBSHD', hd, 'four factory landmarks; incomplete surface'),
        ('TSDZ2', ts, 'external TSDZ2B observations; not TSDZ2 calibration'),
    ):
        result['profiles'][profile] = {
            'evidence': evidence, 'rows_used': len(points),
            'parameters': [row for row in tables['motor_parameters_3_profiles.csv'] if row['motor_family'] == profile],
            'complete_efficiency_map': False,
        }
    result['profiles']['BBS02B']['published_eta_range_percent'] = [
        min(float(row['efficiency_pct_reported']) for row in bbs),
        max(float(row['efficiency_pct_reported']) for row in bbs)]
    result['profiles']['BBS02B']['total_drive_loss_range_W'] = [
        min(float(row['loss_total_W_derived']) for row in bbs),
        max(float(row['loss_total_W_derived']) for row in bbs)]
    result['profiles']['BBSHD']['landmarks'] = hd  # Empty measurements remain empty.
    result['profiles']['TSDZ2']['comparisons'] = ts  # Cooling/firmware interventions stay separate.
    result['policy'] = 'Opt-in bounded current trim with cadence/load hysteresis; thresholds are experimental policy, not map-derived cutoffs.'
    return result


if __name__ == '__main__':
    print(json.dumps(summary(), indent=2, ensure_ascii=False))
