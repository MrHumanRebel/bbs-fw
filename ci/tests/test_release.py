import json
import os
import struct
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path
from unittest.mock import patch
from urllib.parse import urlparse, parse_qs

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
                git("push", "origin", "--delete", "v2.0.0", "v2.0.1", "v2.1.0")
                self.assertEqual(release_version.remote_tags("origin"), {})
                self.assertEqual(release_version.reserve("origin", "HEAD"), (2, 0, 0))
                git("commit", "--allow-empty", "-m", "after reset")
                self.assertEqual(release_version.reserve("origin", "HEAD"), (2, 0, 1))
            finally:
                os.chdir(original)

    def test_release_without_tag_keeps_version_reserved(self):
        with patch.object(release_version.subprocess, "check_output", return_value="a" * 40), patch.object(release_version, "remote_tags", return_value={}), patch.object(release_version, "release_versions", return_value=[(2, 0, 4)]), patch.object(release_version.subprocess, "run", return_value=subprocess.CompletedProcess([], 0)):
            self.assertEqual(release_version.reserve("origin", "HEAD", "MrHumanRebel/bbs-fw"), (2, 0, 5))

    def test_concurrent_reservation_retries_without_moving_tag(self):
        a, b = "a" * 40, "b" * 40
        with patch.object(release_version.subprocess, "check_output", return_value=b + "\n"), \
             patch.object(release_version, "remote_tags", side_effect=[{}, {"v2.0.0": a}, {"v2.0.0": a}]), \
             patch.object(release_version.subprocess, "run", side_effect=[subprocess.CompletedProcess([], 1), subprocess.CompletedProcess([], 0)]) as push:
            self.assertEqual(release_version.reserve("origin", "HEAD"), (2, 0, 1))
            self.assertEqual(push.call_args.args[0][-1], b + ":refs/tags/v2.0.1")


class PayloadTests(unittest.TestCase):
    def setUp(self):
        tags = patch.object(publish_release, "remote_tags", return_value={"v2.0.0": "a" * 40})
        self.tag_lookup = tags.start()
        self.addCleanup(tags.stop)
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
        (self.directory / "BBSFWTool-v2.0.0-win-x64-compact.exe").write_bytes(pe)

    def test_complete_payload_and_checksums(self):
        validate(self.directory, "2.0.0")
        self.assertEqual(len((self.directory / "SHA256SUMS").read_text().splitlines()), 5)
        validate(self.directory, "2.0.0")

    def test_missing_controller_rejected(self):
        (self.directory / "bbs-fw-v2.0.0-BBS02.hex").unlink()
        with self.assertRaises(ValueError):
            validate(self.directory, "2.0.0")

    def test_missing_compact_app_rejected(self):
        (self.directory / "BBSFWTool-v2.0.0-win-x64-compact.exe").unlink()
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
        self.tag_lookup.return_value = {"v2.0.0": "b" * 40}
        with patch.object(publish_release, "gh", return_value="b" * 40), \
             patch.object(publish_release.subprocess, "run") as api:
            with self.assertRaises(RuntimeError):
                publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw", "a" * 40)
            api.assert_not_called()

    def simulate_publication(self, existing=False, fail_upload=False, wrong_size=False, delete_during_upload=False):
        validate(self.directory, "2.0.0")
        release = {"id": 12, "tag_name": "v2.0.0", "draft": True,
                   "upload_url": "https://uploads.github.com/repos/MrHumanRebel/bbs-fw/releases/12/assets{?name,label}",
                   "assets": []}
        if existing:
            release["assets"] = [{"id": 99, "name": "SHA256SUMS", "size": 0}]
        calls = []
        def api(*args):
            calls.append(args)
            if args[:2] == ("api", "repos/MrHumanRebel/bbs-fw/commits/v2.0.0"):
                return "a" * 40
            if args[:2] == ("api", "repos/MrHumanRebel/bbs-fw/releases"):
                self.assertEqual(args[2:], ("--paginate", "--slurp"))
                return json.dumps([[], [release]] if existing else [[]])
            if args[:4] == ("api", "--method", "POST", "repos/MrHumanRebel/bbs-fw/releases"):
                self.assertIn("draft=true", args)
                return json.dumps(release)
            if args[:3] == ("api", "--method", "DELETE"):
                self.assertEqual(args[3], "repos/MrHumanRebel/bbs-fw/releases/assets/99")
                release["assets"] = [a for a in release["assets"] if a["id"] != 99]
                return ""
            if args[:3] == ("api", "--method", "POST") and args[3].startswith("https://uploads.github.com/"):
                if fail_upload:
                    raise subprocess.CalledProcessError(1, ["gh", "api"])
                self.assertIn("/releases/12/assets?", args[3])
                name = parse_qs(urlparse(args[3]).query)["name"][0]
                path = Path(args[args.index("--input") + 1])
                self.assertEqual(name, path.name)
                release["assets"].append({"id": 100 + len(release["assets"]), "name": name,
                                          "size": path.stat().st_size + int(wrong_size)})
                return "{}"
            if args[:2] == ("api", "repos/MrHumanRebel/bbs-fw/releases/12"):
                return json.dumps(release)
            if args[:3] == ("api", "--method", "PATCH"):
                self.assertEqual(args[3], "repos/MrHumanRebel/bbs-fw/releases/12")
                self.assertEqual(len(release["assets"]), 6)
                self.assertIn("draft=false", args)
                return "{}"
            self.fail(f"Unexpected command: {args}")
        self.tag_lookup.side_effect = [{"v2.0.0": "a" * 40}, {} if delete_during_upload else {"v2.0.0": "a" * 40}]
        with patch.object(publish_release, "gh", side_effect=api):
            if fail_upload:
                with self.assertRaises(subprocess.CalledProcessError):
                    publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw")
            elif wrong_size:
                with self.assertRaises(RuntimeError):
                    publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw")
            else:
                publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw")
        return calls

    def test_created_draft_uses_post_response_without_rediscovery(self):
        calls = self.simulate_publication()
        self.assertEqual(sum(c[:2] == ("api", "repos/MrHumanRebel/bbs-fw/releases") for c in calls), 1)
        self.assertIn("PATCH", calls[-1])

    def test_existing_paginated_draft_resumes_by_id(self):
        calls = self.simulate_publication(existing=True)
        self.assertTrue(any("DELETE" in c for c in calls))
        self.assertFalse(any(c[:4] == ("api", "--method", "POST", "repos/MrHumanRebel/bbs-fw/releases") for c in calls))
        self.assertIn("PATCH", calls[-1])

    def test_tag_deleted_during_upload_never_publishes(self):
        self.assertFalse(any("PATCH" in c for c in self.simulate_publication(delete_during_upload=True)))

    def test_deleted_tag_skips_obsolete_build_without_api_mutations(self):
        self.tag_lookup.return_value = {}
        with patch.object(publish_release, "gh") as api:
            publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw", "a" * 40)
            api.assert_not_called()

    def test_failed_upload_never_publishes(self):
        self.assertFalse(any("PATCH" in c for c in self.simulate_publication(fail_upload=True)))

    def test_incomplete_upload_never_publishes(self):
        self.assertFalse(any("PATCH" in c for c in self.simulate_publication(wrong_size=True)))

    def test_wrong_creation_response_never_uploads(self):
        response = {"id": 12, "tag_name": "v2.0.1", "draft": True}
        with patch.object(publish_release, "find_release", return_value=None), \
             patch.object(publish_release, "gh", side_effect=[json.dumps(response)]) as gh:
            with self.assertRaises(RuntimeError):
                publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw")
            self.assertEqual(gh.call_count, 1)

    def test_release_listing_failure_does_not_create_or_publish(self):
        with patch.object(publish_release, "gh", side_effect=[subprocess.CalledProcessError(1, ["gh", "api"])]) as gh:
            with self.assertRaises(subprocess.CalledProcessError):
                publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw")
            self.assertEqual(gh.call_count, 1)

    def test_published_release_rerun_does_not_replace_assets(self):
        validate(self.directory, "2.0.0")
        release = {"id": 12, "draft": False, "assets": [{"name": p.name} for p in self.directory.iterdir()]}
        with patch.object(publish_release, "find_release", return_value=release), \
             patch.object(publish_release, "gh", return_value="a" * 40) as gh:
            publish_release.publish("v2.0.0", self.directory, "MrHumanRebel/bbs-fw")
            gh.assert_not_called()


if __name__ == "__main__":
    unittest.main()
