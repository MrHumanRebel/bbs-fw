"""Research stays offline and retains measurement/proxy distinctions."""
import importlib.util
from pathlib import Path
import subprocess
import sys
import unittest

ROOT = Path(__file__).resolve().parents[2]
SPEC = importlib.util.spec_from_file_location('control_evidence', ROOT / 'research/efficiency/tools/control_evidence.py')
EVIDENCE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(EVIDENCE)


class EfficiencyResearchTests(unittest.TestCase):
    def test_supplied_dataset_physics_and_provenance(self):
        subprocess.run([sys.executable, str(ROOT / 'research/efficiency/tools/validate_dataset.py')], check=True, capture_output=True)

    def test_all_datasets_and_parameters_are_used_without_fabricating_calibration(self):
        result = EVIDENCE.summary()
        profiles = result['profiles']
        self.assertEqual(list(profiles), ['BBS02B', 'BBSHD', 'TSDZ2'])
        self.assertEqual([p['rows_used'] for p in profiles.values()], [93, 4, 9])
        self.assertEqual(sum(len(p['parameters']) for p in profiles.values()), 29)
        self.assertEqual(len(result['sources']), 14)
        self.assertFalse(result['production_lookup_tables'])
        self.assertFalse(result['verified_efficiency_gain'])
        self.assertTrue(all(not p['complete_efficiency_map'] for p in profiles.values()))
        self.assertEqual(profiles['BBSHD']['landmarks'][0]['mechanical_output_W'], '')
        self.assertTrue(all(p['motor_family'] == 'TSDZ2B' and p['target_profile'] == 'TSDZ2'
                            for p in profiles['TSDZ2']['comparisons']))
