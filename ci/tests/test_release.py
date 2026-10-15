import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch

sys.path.insert(0, str(Path(__file__).resolve().parents[1]))
import publish_release
import release_version
from validate_release import validate, validate_hex


class VersionTests(unittest.TestCase):
    def test_initial_and_increment(self):
        self.assertEqual(release_version.next_version([]), (2, 0, 0))
        self.assertEqual(release_version.next_version([(1, 5, 99)]), (2, 0, 0))
        self.assertEqual(release_version.next_version([(2, 0, 0), (2, 0, 3)]), (2, 0, 4))
        self.assertIsNone(release_version.parse_tag("v2.0.0-rc1"))
        self.assertIsNone(release_version.parse_tag("v02.0.0"))

    def test_firmware_byte_rollover(self):
        self.assertEqual(release_version.next_version([(2, 0, 255)]), (2, 1, 0))
        self.assertEqual(release_version.next_version([(2, 255, 255)]), (3, 0, 0))
        with self.assertRaises(ValueError):
            release_version.next_version([(255, 255, 255)])

    def test_real_git_reservation_and_rerun(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            remote, work = root / "remote.git", root / "work"
            subprocess.run(["git", "init", "--bare", str(remote)], check=True, capture_output=True)
            subprocess.run(["git", "init", str(work)], check=True, capture_output=True)
            def git(*args):
                return subprocess.check_output(["git", "-C", str(work), *args], text=True)
            git("config", "user.name", "CI test")
            git("config", "user.email", "ci-test@example.invalid")
            git("remote", "add", "origin", str(remote))
            git("commit", "--allow-empty", "-m", "first")
            original = Path.cwd()
            try:
                os.chdir(work)
                self.assertEqual(release_version.reserve("origin", "HEAD"), (2, 0, 0))
                self.assertEqual(release_version.reserve("origin", "HEAD"), (2, 0, 0))
                first = git("rev-parse", "HEAD").strip()
                git("commit", "--allow-empty", "-m", "second")
                self.assertEqual(release_version.reserve("origin", "HEAD"), (2, 0, 1))
                self.assertEqual(release_version.reserve("origin", first), (2, 0, 0))
                git("tag", "-a", "v2.1.0", "-m", "annotated")
                git("push", "origin", "refs/tags/v2.1.0")
                self.assertEqual(release_version.reserve("origin", "HEAD"), (2, 1, 0))
            finally:
                os.chdir(original)

    def test_concurrent_reservation_retries_without_moving_tag(self):
        a, b = "a" * 40, "b" * 40
        with patch.object(release_version.subprocess, "check_output", return_value=b + "\n"), \
             patch.object(release_version, "remote_tags", side_effect=[{}, {"v2.0.0": a}, {"v2.0.0": a}]), \
             patch.object(release_version.subprocess, "run", side_effect=[subprocess.CompletedProcess([], 1), subprocess.CompletedProcess([], 0)]) as push:
            self.assertEqual(release_version.reserve("origin", "HEAD"), (2, 0, 1))
            self.assertEqual(push.call_args.args[0][-1], b + ":refs/tags/v2.0.1")


class PayloadTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        for controller in ("BBS02", "BBSHD", "TSDZ2"):
            (self.directory / f"bbs-fw-v2.0.0-{controller}.hex").write_text(":0100000001FE\n:00000001FF\n")
        pe = bytearray(128)
        pe[:2] = b"MZ"
        struct.pack_into("<I", pe, 60, 64)
        pe[64:68] = b"PE\0\0"
        struct.pack_into("<H", pe, 68, 0x8664)
        (self.directory / "BBSFWTool-v2.0.0-win-x64.exe").write_bytes(pe)

    def test_complete_payload_and_checksums(self):
        validate(self.directory, "2.0.0")
        self.assertEqual(len((self.directory / "SHA256SUMS").read_text().splitlines()), 4)
        validate(self.directory, "2.0.0")

    def test_missing_controller_rejected(self):
        (self.directory / "bbs-fw-v2.0.0-BBS02.hex").unlink()
        with self.assertRaises(ValueError):
            validate(self.directory, "2.0.0")

    def test_corrupt_hex_rejected(self):
        path = self.directory / "bbs-fw-v2.0.0-BBS02.hex"
        path.write_text(":0100000001FF\n:00000001FF\n")
        with self.assertRaises(ValueError):
            validate_hex(path)

    def test_wrong_windows_architecture_rejected(self):
        path = self.directory / "BBSFWTool-v2.0.0-win-x64.exe"
        pe = bytearray(path.read_bytes())
        struct.pack_into("<H", pe, 68, 0x14c)
        path.write_bytes(pe)
        with self.assertRaises(ValueError):
            validate(self.directory, "2.0.0")

    def test_changed_tag_cannot_publish_wrong_commit(self):
        with patch.object(publish_release, "gh", return_value="b" * 40), \
             patch.object(publish_release.subprocess, "run") as api:
            with self.assertRaises(RuntimeError):
                publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw", "a" * 40)
            api.assert_not_called()

    def test_no_publish_before_all_uploads_verified(self):
        release = {"id": 12, "draft": True, "assets": []}
        lookup = subprocess.CompletedProcess([], 0, json.dumps(release), "")
        with patch.object(publish_release.subprocess, "run", return_value=lookup), \
             patch.object(publish_release, "gh", side_effect=["", json.dumps(release)]) as gh:
            with self.assertRaises(RuntimeError):
                publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw")
            self.assertFalse(any("PATCH" in call.args for call in gh.call_args_list))

    def test_successful_upload_then_publish(self):
        validate(self.directory, "2.0.0")
        release = {"id": 12, "draft": True, "assets": [
            {"name": path.name, "size": path.stat().st_size} for path in self.directory.iterdir()]}
        lookup = subprocess.CompletedProcess([], 0, json.dumps(release), "")
        with patch.object(publish_release.subprocess, "run", return_value=lookup), \
             patch.object(publish_release, "gh", side_effect=["", json.dumps(release), ""]) as gh:
            publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw")
            self.assertIn("PATCH", gh.call_args.args)
            self.assertIn("make_latest=legacy", gh.call_args.args)

    def test_published_release_rerun_does_not_replace_assets(self):
        validate(self.directory, "2.0.0")
        release = {"id": 12, "draft": False, "assets": [{"name": p.name} for p in self.directory.iterdir()]}
        with patch.object(publish_release.subprocess, "run", return_value=subprocess.CompletedProcess([], 0, json.dumps(release), "")), \
             patch.object(publish_release, "gh") as gh:
            publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw")
            gh.assert_not_called()


if __name__ == "__main__":
    unittest.main()
