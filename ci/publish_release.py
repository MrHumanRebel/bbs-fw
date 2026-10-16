"""Upload a complete payload to a draft, then publish it in one API update."""
import argparse
import json
import os
import subprocess
from pathlib import Path
from urllib.parse import urlencode

from validate_release import validate
from release_version import remote_tags


def gh(*args):
    return subprocess.check_output(["gh", *args], text=True)


def find_release(repository, tag):
    # The tag endpoint does not reliably expose unpublished drafts. The
    # authenticated list includes them; paginate so old drafts can be resumed.
    pages = json.loads(gh("api", f"repos/{repository}/releases", "--paginate", "--slurp"))
    return next((release for page in pages for release in page
                 if release["tag_name"] == tag), None)


def check_tag(tag, expected_commit, remote):
    # Git is the source of truth; the REST commit lookup can lag tag creation.
    commit = remote_tags(remote).get(tag)
    if commit is None:
        print(f"::notice::{tag} was deleted. Skipping this obsolete build. "
              "Start a new workflow run or choose Re-run all jobs to reserve "
              "the current version and rebuild every asset.")
        return None
    if expected_commit and commit != expected_commit:
        raise RuntimeError("Release tag no longer points to the commit that was built")
    return commit


def publish(tag, directory, repository, expected_commit=None):
    validate(directory, tag.removeprefix("v"))
    remote = f"https://github.com/{repository}.git"
    tagged_commit = check_tag(tag, expected_commit, remote)
    if tagged_commit is None:
        return
    assets = sorted(path for path in directory.iterdir() if path.is_file())
    release = find_release(repository, tag)
    if release is not None:
        existing_names = {asset["name"] for asset in release["assets"]}
        if not release["draft"]:
            if not {asset.name for asset in assets} <= existing_names:
                raise RuntimeError("Published release has missing assets; refusing to mutate a published release")
            print(f"{tag} is already published with all assets; nothing to change")
            return
    else:
        notes = (
            "Firmware downloads: select the HEX matching your controller (BBS02, BBSHD, or TSDZ2).\n\n"
            "Windows: the compact EXE requires the .NET 10 Desktop Runtime (x64). "
            "The other EXE is standalone and includes the runtime. Both have identical features.\n\n"
            "Runtime download: https://dotnet.microsoft.com/download/dotnet/10.0\n\n"
            "SHA256SUMS contains download checksums. This automated build is not on-device certification."
        )
        # Use the POST response directly: a new draft may not yet appear in
        # list/tag lookups. Every subsequent operation uses its immutable ID.
        release = json.loads(gh("api", "--method", "POST", f"repos/{repository}/releases",
                                "-f", f"tag_name={tag}", "-f", f"target_commitish={tagged_commit}",
                                "-f", f"name=BBS-FW {tag}",
                                "-f", f"body={notes}", "-F", "draft=true",
                                "-F", "generate_release_notes=true"))
    if release.get("tag_name") != tag or not release.get("draft") or not release.get("id"):
        raise RuntimeError("Unexpected draft release response; refusing to upload")
    endpoint = f"repos/{repository}/releases/{release['id']}"
    upload_endpoint = release["upload_url"].split("{", 1)[0]
    for asset in assets:
        # Resume interrupted uploads by replacing only assets on this draft.
        for previous in release["assets"]:
            if previous["name"] == asset.name:
                gh("api", "--method", "DELETE", f"repos/{repository}/releases/assets/{previous['id']}")
        gh("api", "--method", "POST", upload_endpoint + "?" + urlencode({"name": asset.name}),
           "-H", "Content-Type: application/octet-stream", "--input", str(asset))
    uploaded = json.loads(gh("api", endpoint))
    by_name = {asset["name"]: asset for asset in uploaded["assets"]}
    for asset in assets:
        if asset.name not in by_name or by_name[asset.name]["size"] != asset.stat().st_size:
            raise RuntimeError(f"Release upload missing or incomplete: {asset.name}")
    if check_tag(tag, tagged_commit, remote) is None:
        return
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
