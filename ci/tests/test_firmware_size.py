"""Memory regressions and unreadable reports must fail the build."""
from pathlib import Path
import tempfile
import unittest
from ci.check_firmware_size import check


class FirmwareSizeTests(unittest.TestCase):
    def report(self, directory, flash=25000, xram=860, stack=172):
        (directory / "bbs-fw.mem").write_text(
            f"Stack starts at: 0x65 (sp set to 0x64) with {stack} bytes available.\n"
            f"EXTERNAL RAM     0x0001   0x0388     {xram}     1792\n"
            f"ROM/EPROM/FLASH  0x0000   0x64cc   {flash}    65536\n")

    def test_bbs_regressions(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            self.report(directory)
            self.assertEqual(check("BBS02", directory)["flash"], 25000)
            for parameters in ({"flash": 28769}, {"xram": 861}, {"stack": 171}):
                self.report(directory, **parameters)
                with self.assertRaises(ValueError):
                    check("BBS02", directory)

    def test_stm8_areas(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            report = "CODE 000082E5 0000398F = 14735. bytes\nDATA 00000001 00000245 = 581. bytes\nINITIALIZED 00000246 0000003D = 61. bytes\n"
            (directory / "bbs-fw.hex").write_text(":01800000017E\n:00000001FF\n")
            (directory / "bbs-fw.map").write_text(report)
            self.assertEqual(check("TSDZ2", directory)["code"], 14735)
            for original, excess in (("14735", "16289"), ("581", "582"), ("61.", "62.")):
                (directory / "bbs-fw.map").write_text(report.replace(original, excess))
                with self.assertRaises(ValueError):
                    check("TSDZ2", directory)

    def test_stm8_constants_cannot_escape_code_budget(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / "bbs-fw.map").write_text(
                "CODE 000082E5 00002710 = 10000. bytes\nDATA 00000001 00000245 = 581. bytes\nINITIALIZED 00000246 0000003D = 61. bytes\n")
            # Valid HEX with a separate byte just beyond the complete-image budget.
            (directory / "bbs-fw.hex").write_text(":01800000017E\n:01C2800001BC\n:00000001FF\n")
            with self.assertRaisesRegex(ValueError, "image_span"):
                check("TSDZ2", directory)

    def test_unknown_report_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            directory = Path(temporary)
            (directory / "bbs-fw.mem").write_text("different compiler output")
            with self.assertRaises(ValueError):
                check("BBS02", directory)
