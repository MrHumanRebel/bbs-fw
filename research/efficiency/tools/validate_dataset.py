#!/usr/bin/env python3
"""Offline, deterministic physical consistency + provenance for EXACT 3 target profiles."""
from pathlib import Path
import csv, math, json, sys
R=Path(__file__).resolve().parents[1];D=R/'data'
def rows(name):
 with (D/name).open(encoding='utf-8-sig',newline='') as f:return list(csv.DictReader(f))
def v(r,k):return None if r[k]=='' else float(r[k])
manifest=json.loads((D/'dataset_manifest.json').read_text(encoding='utf-8'))
assert manifest['targets']==['BBS02B','BBSHD','TSDZ2']
assert len(manifest['targets'])==3
assert all(not x for x in manifest['evidence_limits'].values())
names={'bbs02b_factory_reference_93points.csv':93,'bbshd_factory_4_landmarks.csv':4,'tsdz2_external_bench_9points.csv':9,'motor_parameters_3_profiles.csv':29}
assert set(p.name for p in D.glob('*.csv'))==set(names)|{'sources.csv'},'Unexpected fourth profile CSV or missing profile file'
sources=rows('sources.csv');ids={x['source_id'] for x in sources}
assert len(sources)==len(ids)==manifest['counts']['source_entries']
assert all(s['url'].startswith('https://') for s in sources)
for f,c in names.items():
 xx=rows(f);assert len(xx)==c,(f,len(xx),c)
 assert all(x['source_id'] in ids for x in xx),f
 print('PASS',f,c)
bbs=rows('bbs02b_factory_reference_93points.csv')
for i,r in enumerate(bbs,1):
 assert r['row']==str(i) and r['source_row']==str(i)
 assert r['motor_family']=='BBS02B' and r['source_id']=='BBS750_FACTORY'
 assert r['calibration_status']=='accepted_BBS02B_reference_not_direct_revision_measurement'
 assert r['origin_csv']=='external_factory_load_sweep_BBS750_FACTORY'
 assert r['original_source_motor_family']=='BBS_48V_750W_unversioned'
 pin,pout,volts,amps,torque,rpm,eta=(v(r,k) for k in ('battery_input_W','mechanical_output_W','battery_voltage_V','battery_current_A','torque_Nm','crank_output_rpm','efficiency_pct_reported'))
 assert all(math.isfinite(x) for x in (pin,pout,volts,amps,torque,rpm,eta))
 assert 0<pout<pin and 0<eta<=100 and 0<volts<100 and rpm>0 and torque>0
 assert abs(pin-volts*amps)<0.50,(i,'electrical')
 assert abs(pout-torque*rpm*math.pi/30)<max(2,0.035*pout),(i,'mechanical')
 assert abs(eta-(pout/pin)*100)<0.21,(i,'efficiency')
 assert abs(v(r,'loss_total_W_derived')-(pin-pout))<0.026,(i,'loss')
assert bbs[49]['efficiency_pct_reported']=='85.8'
assert bbs[92]['torque_Nm']=='90.4'
print('PASS factory 93-row physics, provenance and exact anchors')
bh=rows('bbshd_factory_4_landmarks.csv')
assert all(r['motor_family']=='BBSHD' and r['source_id']=='BBSHD_FACTORY' for r in bh)
assert set(r['landmark'] for r in bh)=={'start_load','max_efficiency','max_torque_point','max_mech_power'}
assert next(r for r in bh if r['landmark']=='max_efficiency')['efficiency_pct_reported']=='85.6'
assert next(r for r in bh if r['landmark']=='start_load')['efficiency_pct_reported']==''
print('PASS BBSHD four source-labelled points and preserved missing values')
tsd=rows('tsdz2_external_bench_9points.csv')
assert all(r['motor_family']=='TSDZ2B' and r['target_profile']=='TSDZ2' and r['source_id']=='TSDZ2B_BENCH' for r in tsd)
assert {r['crank_rpm'] for r in tsd}=={'60','90'}
assert next(r for r in tsd if r['crank_rpm']=='90' and r['cooling']=='premium')['time_to_70C_seconds']==''
assert all(0<v(r,'efficiency_pct_reported_approx')<100 for r in tsd)
print('PASS TSDZ2 source-variant honesty, workshop test values')
pars=rows('motor_parameters_3_profiles.csv')
assert set(p['motor_family'] for p in pars)=={'BBS02B','BBSHD','TSDZ2'}
related=[p for p in pars if p['motor_family']=='BBS02B' and p['source_id']=='GRIN_2019']
assert len(related)==5 and all(p['evidence_type']=='related_family_reference_only' and 'NOT verified BBS02B' in p['variant'] for p in related)
print('PASS 3-profile motor parameter variants and Grin transfer caveat')
obj=json.loads((D/'research_dataset.json').read_text(encoding='utf-8'))
assert obj['manifest']==manifest
assert set(obj['tables'])==set(names)|{'sources.csv'}
for name in names|{'sources.csv':len(sources)}:
 assert obj['tables'][name]==rows(name)
assert manifest['counts']=={'bbs02b_factory_reference_rows':93,'bbshd_factory_anchor_rows':4,'tsdz2_external_bench_rows':9,'motor_parameter_rows':29,'source_entries':len(sources),'distinct_motor_profiles':3}
print('PASS aggregate JSON mirror and counts')
print('FINAL THREE-PROFILE DATASET: ALL CHECKS PASSED')
