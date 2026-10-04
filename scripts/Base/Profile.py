"""Builds Profile, opens the Tracy viewer and runs an app connected to it.

Usage: Profile.py [App] [--no-build]    App is a project name under bin/, default Sandbox.
"""
import os
import subprocess
import sys

from Build import PROJECT_ROOT, build
from InitializeRepo import TRACY_DIR, TRACY_STAMP, submodule_tracy_version

CONFIGURATION = "Profile"
DEFAULT_APP = "Sandbox"


def main() -> int:
    app = DEFAULT_APP
    do_build = True
    for arg in sys.argv[1:]:
        if arg == "--no-build":
            do_build = False
        elif arg.startswith("--"):
            print(f"Unknown argument: '{arg}'")
            return 1
        else:
            app = arg

    viewer = TRACY_DIR / "tracy-profiler.exe"
    installed = TRACY_STAMP.read_text().strip() if TRACY_STAMP.is_file() else None
    if not viewer.is_file() or installed != submodule_tracy_version():
        print(
            "ERROR: The Tracy viewer is missing or does not match the tracy submodule. "
            "Run scripts/Base/InitializeRepo.py.",
            file=sys.stderr,
        )
        return 1

    # Building first means a forgotten rebuild can't profile stale code.
    if do_build:
        code = build(CONFIGURATION)
        if code != 0:
            return code

    exe = PROJECT_ROOT / "bin" / f"{CONFIGURATION}-windows-x86_64" / app / f"{app}.exe"
    if not exe.is_file():
        print(f"ERROR: Executable not found: {exe}", file=sys.stderr)
        return 1

    # The viewer retries its connection every 10 ms, so it can open before the app starts listening.
    subprocess.Popen([str(viewer), "-a", "127.0.0.1"])

    # TRACY_NO_EXIT holds the app at shutdown until the viewer has every event, so short runs are
    # not truncated. Closing the viewer before the app finishes therefore leaves the app waiting.
    env = {**os.environ, "TRACY_NO_EXIT": "1"}
    return subprocess.run([str(exe)], cwd=PROJECT_ROOT, env=env, check=False).returncode


if __name__ == "__main__":
    raise SystemExit(main())
