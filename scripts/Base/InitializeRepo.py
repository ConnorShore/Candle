from hashlib import sha256
from io import BytesIO
from pathlib import Path
import os
import sys
from tempfile import NamedTemporaryFile
from urllib.request import urlopen
from zipfile import BadZipFile, ZipFile


PROJECT_ROOT = Path(__file__).resolve().parents[2]
PREMAKE_EXECUTABLE = PROJECT_ROOT / "vendor" / "premake" / "bin" / "premake5.exe"
PREMAKE_URL = (
    "https://github.com/premake/premake-core/releases/download/"
    "v5.0.0-beta8/premake-5.0.0-beta8-windows.zip"
)
PREMAKE_SHA256 = "e64ce2ed8778e0098f63674cca61fe33941b5f0c8d9a4afd651152bdea3758ab"
VULKAN_SDK_URL = "https://vulkan.lunarg.com/sdk/home"


def main() -> int:
    premake_result = install_premake()
    if premake_result != 0:
        return premake_result
    return check_vulkan_sdk()


def check_vulkan_sdk() -> int:
    # The SDK is a system-wide install, so it is verified here rather than installed.
    sdk_root = os.environ.get("VULKAN_SDK")
    if not sdk_root:
        print(
            "ERROR: VULKAN_SDK is not set. Install the Vulkan SDK from "
            f"{VULKAN_SDK_URL}, then open a new terminal so the variable is visible.",
            file=sys.stderr,
        )
        return 1

    header = Path(sdk_root) / "Include" / "vulkan" / "vulkan.h"
    if not header.is_file():
        print(
            f"ERROR: VULKAN_SDK points to {sdk_root}, but {header} does not exist. "
            f"Reinstall the Vulkan SDK from {VULKAN_SDK_URL}.",
            file=sys.stderr,
        )
        return 1

    print(f"Vulkan SDK found: {sdk_root}")
    return 0


def install_premake() -> int:
    if PREMAKE_EXECUTABLE.is_file():
        print(f"Premake already exists: {PREMAKE_EXECUTABLE}")
        return 0

    try:
        with urlopen(PREMAKE_URL, timeout=60) as response:
            archive_data = response.read()
    except OSError as error:
        print(f"ERROR: Could not download Premake: {error}", file=sys.stderr)
        return 1

    actual_hash = sha256(archive_data).hexdigest()
    if actual_hash != PREMAKE_SHA256:
        print(
            f"ERROR: Premake archive checksum mismatch: {actual_hash}",
            file=sys.stderr,
        )
        return 1

    try:
        with ZipFile(BytesIO(archive_data)) as archive:
            executables = [
                item for item in archive.infolist()
                if not item.is_dir() and Path(item.filename).name.lower() == "premake5.exe"
            ]
            if len(executables) != 1:
                print(
                    "ERROR: Expected exactly one premake5.exe in the verified release archive.",
                    file=sys.stderr,
                )
                return 1
            executable_data = archive.read(executables[0])
    except (BadZipFile, OSError, KeyError) as error:
        print(f"ERROR: Could not read the Premake release archive: {error}", file=sys.stderr)
        return 1

    PREMAKE_EXECUTABLE.parent.mkdir(parents=True, exist_ok=True)
    temporary_path = None
    try:
        with NamedTemporaryFile(dir=PREMAKE_EXECUTABLE.parent, delete=False) as temporary_file:
            temporary_path = Path(temporary_file.name)
            temporary_file.write(executable_data)
        os.replace(temporary_path, PREMAKE_EXECUTABLE)
    except OSError as error:
        if temporary_path is not None:
            temporary_path.unlink(missing_ok=True)
        print(f"ERROR: Could not install Premake: {error}", file=sys.stderr)
        return 1

    print(f"Installed verified Premake at {PREMAKE_EXECUTABLE}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())