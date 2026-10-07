# Script to compile all .slang shaders in a base directory to SPIR-V using the specified SPIR-V version.
# Outputs mirror the source tree under <base_directory>/bin; files with no entry points are imported modules and are skipped.
# Usage: python CompileShaders.py <base_directory> [--spirv-version spirv_1_6]

import os
import shutil
import subprocess
import sys
import argparse

VULKAN_SDK_URL = "https://vulkan.lunarg.com/sdk/home"
DEFAULT_SPIRV_VERSION = "spirv_1_6"

# slangc's "SPIR-V output contains no exported symbols", which is how a module with no [shader] entries fails.
NO_ENTRY_POINTS_DIAGNOSTIC = "E57004"

def find_slangc():
    """Locates slangc in the SDK named by VULKAN_SDK, which the engine builds against, then falls back to PATH."""
    sdk_root = os.environ.get("VULKAN_SDK")
    if sdk_root:
        candidate = os.path.join(sdk_root, "Bin", "slangc.exe")
        if os.path.isfile(candidate):
            return candidate
    return shutil.which("slangc")

def compile_shaders(slangc, base_directory, spirv_version):
    """Compiles every .slang file under base_directory and returns the number that failed."""
    bin_directory = os.path.join(base_directory, "bin")
    compiled, skipped, failed = 0, 0, 0

    for root, dirs, files in os.walk(base_directory):
        if root == base_directory and "bin" in dirs:
            dirs.remove("bin")  # Never walk our own output.

        for file in files:
            if not file.endswith(".slang"):
                continue

            shader_path = os.path.join(root, file)
            relative_path = os.path.relpath(shader_path, base_directory)
            
            # If the relative starting dir is "src", don't include it in the output path.
            if relative_path.startswith("src" + os.sep):
                relative_path = relative_path[len("src" + os.sep):]

            output_path = os.path.join(bin_directory, os.path.splitext(relative_path)[0] + ".spv")
            os.makedirs(os.path.dirname(output_path), exist_ok=True)

            result = subprocess.run([
                slangc,
                shader_path,
                "-target", "spirv",
                "-profile", spirv_version,
                "-emit-spirv-directly",
                "-fvk-use-entrypoint-name",
                "-o", output_path
            ], capture_output=True, encoding="utf-8", errors="replace")
            diagnostics = (result.stdout + result.stderr).strip()

            if result.returncode != 0 and NO_ENTRY_POINTS_DIAGNOSTIC in diagnostics:
                if os.path.isfile(output_path):
                    os.remove(output_path)
            elif result.returncode != 0:
                print(f"FAILED   {relative_path}", file=sys.stderr)
                print(diagnostics, file=sys.stderr)
                failed += 1
            else:
                print(f"Compiled {relative_path} -> {os.path.relpath(output_path, base_directory)}")
                if diagnostics:
                    print(diagnostics)  # Warnings.
                compiled += 1

    print(f"\n{compiled} compiled, {skipped} skipped, {failed} failed.")
    return failed

if __name__ == "__main__":
    parser = argparse.ArgumentParser(description="Compile all .slang shaders in a base directory to SPIR-V.")
    parser.add_argument("base_directory", help="The base directory containing shader files.")
    parser.add_argument("--spirv-version", default=DEFAULT_SPIRV_VERSION, help=f"The SPIR-V version to compile to (default: {DEFAULT_SPIRV_VERSION}).")
    args = parser.parse_args()

    if not os.path.isdir(args.base_directory):
        print(f"ERROR: Shader directory not found: {args.base_directory}", file=sys.stderr)
        sys.exit(1)

    slangc = find_slangc()
    if not slangc:
        print(
            "ERROR: Could not find slangc. Install a recent Vulkan SDK (it ships Slang) from "
            f"{VULKAN_SDK_URL} and open a new terminal, or put slangc on PATH.",
            file=sys.stderr,
        )
        sys.exit(1)

    print(f"Using {slangc}")
    sys.exit(1 if compile_shaders(slangc, args.base_directory, args.spirv_version) else 0)
