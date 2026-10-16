#!/usr/bin/env python3
"""Prepare/build an isolated, pinned Muse StopWatch SDK without changing other projects."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

HERE = Path(__file__).resolve().parent
REVISION = "fa073045a5572a8e3367fae142c1b7a7c554f222"
URL = "https://github.com/facebookincubator/muse-gadget-sdk.git"


def run(args, **kwargs):
    return subprocess.run(args, check=True, **kwargs)


def prepare(source, destination):
    """Always copy, never patch the caller's SDK checkout."""
    destination = Path(destination).resolve()
    if destination.exists():
        raise ValueError(f"Output already exists; choose a new directory: {destination}")
    if source is None:
        source = Path(tempfile.mkdtemp(prefix="muse-upstream-")) / "sdk"
        run(["git", "clone", "--filter=blob:none", "--no-checkout", URL, str(source)])
        run(["git", "-C", str(source), "checkout", "--detach", REVISION])
    else:
        source = Path(source).resolve()
    if source == destination or source in destination.parents:
        raise ValueError("Output must be outside the source SDK to avoid recursive copying")
    # Hash every upstream file touched by our patch, including a connector-fetched
    # source snapshot that has no .git. Reject modifications before copying.
    expected = json.loads((HERE / "upstream-hashes.json").read_text())
    for name, digest in expected.items():
        path = source / name
        if not path.is_file() or hashlib.sha256(path.read_bytes()).hexdigest() != digest:
            raise ValueError(f"Expected unmodified upstream {REVISION}: {name}")
    if (source / ".git").exists():
        actual = subprocess.check_output(["git", "-C", str(source), "rev-parse", "HEAD"], text=True).strip()
        if actual != REVISION:
            raise ValueError(f"SDK revision mismatch: {actual}")
    shutil.copytree(source / "esp32", destination / "esp32", ignore=shutil.ignore_patterns("build*", "managed_components", "dependencies.lock", "__pycache__"))
    if (source / "LICENSE").is_file():
        shutil.copy2(source / "LICENSE", destination / "LICENSE")
    run(["git", "apply", "--check", str(HERE / "upstream.patch")], cwd=destination)
    run(["git", "apply", str(HERE / "upstream.patch")], cwd=destination)
    for file in (HERE / "src").iterdir():
        shutil.copy2(file, destination / "esp32/components/muse" / file.name)
    shutil.copy2(HERE / "sdkconfig.stopwatch", destination / "esp32/devices/sdkconfig.stopwatch-local")
    shutil.copy2(HERE / "partitions.stopwatch.csv", destination / "esp32/partitions.stopwatch.csv")
    if (HERE / "dependencies.lock").is_file():
        shutil.copy2(HERE / "dependencies.lock", destination / "esp32/dependencies.lock")
    (destination / "stopwatch-source.json").write_text(json.dumps({"upstream": URL, "revision": REVISION}, indent=2) + "\n")
    return destination / "esp32"


def build(project):
    idf = os.environ.get("IDF_PATH")
    if not idf:
        raise RuntimeError("Activate ESP-IDF 6.0.1 for the Muse SDK.")
    tool = Path(idf) / "tools/idf.py"
    version = subprocess.check_output([sys.executable, str(tool), "--version"], text=True)
    if "v6.0.1" not in version:
        raise RuntimeError("Muse StopWatch requires ESP-IDF v6.0.1; found " + version.strip())
    defaults = ";".join(("sdkconfig.defaults", "devices/sdkconfig.muse", "devices/sdkconfig.muse-m5stack-stopwatch", "devices/sdkconfig.stopwatch-local"))
    run([sys.executable, str(tool), "-C", str(project), "-B", str(project / "build-stopwatch"),
         "-DIDF_TARGET=esp32s3", "-DSDKCONFIG_DEFAULTS=" + defaults, "build"])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("action", choices=("prepare", "build"))
    parser.add_argument("--sdk-source", type=Path, help="Unmodified pinned Muse SDK; copied before applying changes")
    parser.add_argument("--out", type=Path, required=True, help="New staging directory outside this repository")
    args = parser.parse_args()
    project = prepare(args.sdk_source, args.out)
    print("Prepared:", project, flush=True)
    if args.action == "build":
        build(project)


if __name__ == "__main__":
    try:
        main()
    except (ValueError, RuntimeError, subprocess.CalledProcessError) as error:
        print(str(error), file=sys.stderr)
        sys.exit(1)
