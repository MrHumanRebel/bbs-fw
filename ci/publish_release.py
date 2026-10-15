"""Upload a complete payload to a draft, then publish it in one API update."""
import argparse
import json
import os
import subprocess
from pathlib import Path

from validate_release import validate


def gh(*args):
    return subprocess.check_output(["gh", *args], text=True)


def publish(tag, directory, repository, expected_commit=None):
    validate(directory, tag.removeprefix("v"))
    if expected_commit:
        tagged_commit = gh("api", f"repos/{repository}/commits/{tag}", "--jq", ".sha").strip()
        if tagged_commit != expected_commit:
            raise RuntimeError("Release tag no longer points to the commit that was built")
    assets = sorted(path for path in directory.iterdir() if path.is_file())
    endpoint = f"repos/{repository}/releases/tags/{tag}"
    lookup = subprocess.run(["gh", "api", endpoint], text=True, capture_output=True)
    if lookup.returncode == 0:
        release = json.loads(lookup.stdout)
        existing_names = {asset["name"] for asset in release["assets"]}
        if not release["draft"]:
            if not {asset.name for asset in assets} <= existing_names:
                raise RuntimeError("Published release has missing assets; refusing to mutate a published release")
            print(f"{tag} is already published with all assets; nothing to change")
            return
    else:
        if "404" not in lookup.stderr:
            raise RuntimeError(f"Cannot inspect release: {lookup.stderr}")
        notes = (
            "Firmware downloads: select the HEX matching your controller (BBS02, BBSHD, or TSDZ2).\n\n"
            "The Windows x64 EXE is self-contained; no separate .NET installation is required.\n\n"
            "SHA256SUMS contains download checksums. This automated build is not on-device certification."
        )
        gh("release", "create", tag, "--repo", repository, "--verify-tag", "--draft",
           "--title", f"BBS-FW {tag}", "--notes", notes, "--generate-notes")
        release = json.loads(gh("api", endpoint))
    gh("release", "upload", tag, "--repo", repository, "--clobber", *map(str, assets))
    uploaded = json.loads(gh("api", endpoint))
    by_name = {asset["name"]: asset for asset in uploaded["assets"]}
    for asset in assets:
        if asset.name not in by_name or by_name[asset.name]["size"] != asset.stat().st_size:
            raise RuntimeError(f"Release upload missing or incomplete: {asset.name}")
    # Let GitHub determine latest by its normal version/date rules rather than
    # marking an older concurrent run latest just because it finished last.
    gh("api", "--method", "PATCH", f"repos/{repository}/releases/{release['id']}",
       "-F", "draft=false", "-f", "make_latest=legacy")
    print(f"Published https://github.com/{repository}/releases/tag/{tag}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("tag")
    parser.add_argument("directory", type=Path)
    args = parser.parse_args()
    repository = os.environ["GH_REPO"]
    if repository != "MrHumanRebel/bbs-fw":
        raise SystemExit("Automatic publication is restricted to MrHumanRebel/bbs-fw")
    publish(args.tag, args.directory, repository, os.environ.get("GITHUB_SHA"))
