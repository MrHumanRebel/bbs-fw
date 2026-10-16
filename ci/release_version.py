"""Reserve an immutable release tag using Git's atomic ref creation.

No version-bump commit is needed. Re-running the same commit reuses its tag;
different concurrent commits retry against the newly reserved version.
"""
import argparse
import os
import re
import subprocess
from pathlib import Path

FIRST = (2, 0, 0)
PATTERN = re.compile(r"v(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)$")


def parse_tag(tag):
    match = PATTERN.fullmatch(tag)
    return tuple(map(int, match.groups())) if match else None


def next_version(versions):
    versions = [version for version in versions if version >= FIRST]
    if not versions:
        return FIRST
    major, minor, patch = max(versions)
    if max(major, minor, patch) > 255:
        raise ValueError("Release versions must fit the firmware's three byte fields")
    if patch < 255:
        return major, minor, patch + 1
    if minor < 255:
        return major, minor + 1, 0
    if major < 255:
        return major + 1, 0, 0
    raise ValueError("Firmware version space exhausted")


def remote_tags(remote):
    output = subprocess.check_output(["git", "ls-remote", "--tags", remote], text=True)
    tags, peeled = {}, {}
    for line in output.splitlines():
        sha, ref = line.split()
        name = ref.removeprefix("refs/tags/")
        if name.endswith("^{}"):
            peeled[name[:-3]] = sha
        else:
            tags[name] = sha
    tags.update(peeled)
    return {tag: sha for tag, sha in tags.items() if parse_tag(tag) is not None}


def release_versions(repository):
    if not repository:
        return []
    output = subprocess.check_output(
        ["gh", "api", f"repos/{repository}/releases", "--paginate", "--jq", ".[].tag_name"], text=True)
    return [version for tag in output.splitlines() if (version := parse_tag(tag)) is not None]


def reserve(remote, commit, repository=None):
    commit = subprocess.check_output(
        ["git", "rev-parse", "--verify", f"{commit}^{{commit}}"], text=True
    ).strip()
    for _ in range(20):
        tags = remote_tags(remote)
        existing = [parse_tag(tag) for tag, sha in tags.items()
                    if sha == commit and parse_tag(tag) >= FIRST]
        if existing:
            version = max(existing)
            if max(version) > 255:
                raise ValueError("Existing release tag exceeds firmware version limits")
            return version
        version = next_version([parse_tag(tag) for tag in tags] + release_versions(repository))
        tag = "v" + ".".join(map(str, version))
        push = subprocess.run(["git", "push", remote, f"{commit}:refs/tags/{tag}"])
        if push.returncode == 0:
            return version
        # Retry only a proven tag race, not a permission or connectivity failure.
        actual = remote_tags(remote).get(tag)
        if actual == commit:
            return version
        if actual is None:
            raise RuntimeError("Tag push failed without a competing tag; check write access")
    raise RuntimeError("Too many concurrent tag reservations")


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--remote", default="origin")
    parser.add_argument("--commit", default="HEAD")
    parser.add_argument("--repository")
    args = parser.parse_args()
    version = ".".join(map(str, reserve(args.remote, args.commit, args.repository)))
    values = f"version={version}\ntag=v{version}\n"
    print(values, end="")
    if os.environ.get("GITHUB_OUTPUT"):
        with Path(os.environ["GITHUB_OUTPUT"]).open("a", encoding="utf-8") as output:
            output.write(values)


if __name__ == "__main__":
    main()
