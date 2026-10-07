import os
import shutil
import subprocess
import sys
from fnmatch import fnmatch
from pathlib import Path

from CompileShaders import DEFAULT_SPIRV_VERSION, VULKAN_SDK_URL, compile_shaders, find_slangc

PROJECT_ROOT = Path(__file__).resolve().parents[2]
SOLUTION = PROJECT_ROOT / "Candle.slnx"

CONFIGURATIONS = ["Debug", "Release", "Profile", "Dist"]
DEFAULT_CONFIGURATION = "Debug"

# Directories removed by clean(), relative to PROJECT_ROOT. Every bin/ goes too -- see _iter_bin_dirs().
CLEAN_DIRS = [".vs"]

# Where each project keeps its Slang sources, relative to the project directory.
SHADER_DIR = Path("res") / "shaders"

# Generated Premake/Visual Studio files removed by clean(). Matched anywhere under
# PROJECT_ROOT except inside vendor submodules -- see _iter_generated_files().
CLEAN_FILE_PATTERNS = ["*.sln", "*.slnx", "*.vcxproj", "*.vcxproj.filters", "*.vcxproj.user"]


def generate_projects():
    """Generates the necessary project files."""
    result = subprocess.run(
        [sys.executable, str(Path(__file__).with_name("GenerateProject.py"))],
        check=False,
    )
    if result.returncode != 0:
        sys.exit(result.returncode)


def _iter_generated_files():
    """Yields generated project files under PROJECT_ROOT, skipping nested git repos.

    A plain rglob() descends into the vendor submodules, which ship their own
    project files -- SDL alone has ~90 under VisualC/ and VisualC-GDK/, and
    Candle/vendor/SDL3/premake5.lua reads VisualC/SDL/SDL.vcxproj to derive its
    source list. Deleting those breaks project generation and dirties the
    submodules, so prune at every submodule boundary.
    """
    for dirpath, dirnames, filenames in os.walk(PROJECT_ROOT):
        dirnames[:] = [
            d for d in dirnames
            if d != ".git" and not (Path(dirpath) / d / ".git").exists()
        ]
        for name in filenames:
            if any(fnmatch(name, pattern) for pattern in CLEAN_FILE_PATTERNS):
                yield Path(dirpath) / name


def _iter_bin_dirs():
    """Yields every bin/ directory under PROJECT_ROOT outside vendor code.

    vendor/premake/bin and vendor/tracy/bin hold the tools InitializeRepo.py installs, and the
    default command sequence runs 'generate' straight after 'clean', so vendor/ is never entered.
    """
    for dirpath, dirnames, _ in os.walk(PROJECT_ROOT):
        dirnames[:] = [
            d for d in dirnames
            if d not in (".git", "vendor") and not (Path(dirpath) / d / ".git").exists()
        ]
        if "bin" in dirnames:
            dirnames.remove("bin")
            yield Path(dirpath) / "bin"


def clean():
    """Cleans all Premake/Visual Studio generated files and build output from every sub project."""
    targets = [PROJECT_ROOT / dir_name for dir_name in CLEAN_DIRS] + list(_iter_bin_dirs())
    for target in targets:
        if target.is_dir():
            print(f"Removing {target}...")
            shutil.rmtree(target, ignore_errors=True)

    for file in _iter_generated_files():
        print(f"Removing {file}...")
        file.unlink(missing_ok=True)

    print("Clean complete!")


def _find_msbuild() -> str | None:
    """Locates MSBuild.exe via the MSBUILD_PATH env var or vswhere."""
    env_path = os.environ.get("MSBUILD_PATH")
    if env_path and Path(env_path).is_file():
        return env_path

    for program_files in (os.environ.get("ProgramFiles(x86)"), os.environ.get("ProgramFiles")):
        if not program_files:
            continue
        vswhere = Path(program_files) / "Microsoft Visual Studio" / "Installer" / "vswhere.exe"
        if vswhere.is_file():
            result = subprocess.run(
                [
                    str(vswhere),
                    "-latest",
                    "-prerelease",
                    "-products", "*",
                    "-requires", "Microsoft.Component.MSBuild",
                    "-find", r"MSBuild\**\Bin\MSBuild.exe",
                ],
                check=False,
                capture_output=True,
                text=True,
            )
            paths = [line.strip() for line in result.stdout.splitlines() if line.strip()]
            if paths:
                return paths[0]

    return None


def build(configuration: str = DEFAULT_CONFIGURATION) -> int:
    """Builds the Candle solution with the given configuration."""
    if not SOLUTION.is_file():
        print(f"ERROR: Solution not found: {SOLUTION}. Run 'generate' first.", file=sys.stderr)
        return 1

    msbuild = _find_msbuild()
    if not msbuild:
        print(
            "ERROR: Could not find MSBuild.exe. Install Visual Studio with the "
            "\"Desktop development with C++\" workload, or set MSBUILD_PATH.",
            file=sys.stderr,
        )
        return 1

    result = subprocess.run(
        [msbuild, str(SOLUTION), f"/p:Configuration={configuration}", "/m"],
        cwd=PROJECT_ROOT,
        check=False,
    )

    if result.returncode != 0:
        print(f"\nERROR: MSBuild failed with exit code {result.returncode}.\n", file=sys.stderr)

    return result.returncode


def run_tests(configuration: str = DEFAULT_CONFIGURATION, test_args: list[str] | None = None) -> int:
    """Runs Candle-Test for the given configuration. Never builds first -- use 'build test'."""
    test_exe = PROJECT_ROOT / "bin" / f"{configuration}-windows-x86_64" / "Candle-Test" / "Candle-Test.exe"
    if not test_exe.is_file():
        print(f"ERROR: Test executable not found: {test_exe}. Run 'build test' first.", file=sys.stderr)
        return 1

    # Run from the repo root to match the project's debugdir, so script and debugger runs agree.
    result = subprocess.run([str(test_exe), *(test_args or [])], cwd=PROJECT_ROOT, check=False)
    return result.returncode

def _iter_projects():
    """Yields Candle's own projects: top-level directories with a premake5.lua. Vendors sit under vendor/ and never match."""
    for child in sorted(PROJECT_ROOT.iterdir()):
        if child.is_dir() and (child / "premake5.lua").is_file():
            yield child


def compile_project_shaders() -> int:
    """Compiles <project>/res/shaders for every Candle project that has one."""
    slangc = find_slangc()
    if not slangc:
        print(
            f"ERROR: Could not find slangc. Install a recent Vulkan SDK from {VULKAN_SDK_URL}, or put slangc on PATH.",
            file=sys.stderr,
        )
        return 1

    print(f"Using {slangc}")
    failed = 0
    for project in _iter_projects():
        shader_dir = project / SHADER_DIR
        if shader_dir.is_dir():
            print(f"\n{project.name}: {shader_dir.relative_to(PROJECT_ROOT)}")
            # A str, not a Path: compile_shaders compares os.walk's str roots against it to skip bin/.
            failed += compile_shaders(slangc, str(shader_dir), DEFAULT_SPIRV_VERSION)

    if failed:
        print(f"\nERROR: {failed} shader(s) failed to compile.\n", file=sys.stderr)
    return 1 if failed else 0


VALID_COMMANDS = ("clean", "generate", "shaders", "build", "test")

def main():
    commands = []
    configuration = DEFAULT_CONFIGURATION
    test_args = []

    for arg in sys.argv[1:]:
        if arg in CONFIGURATIONS:
            configuration = arg
        elif arg in VALID_COMMANDS:
            commands.append(arg)
        elif arg.startswith("--"):
            # Candle-Test's own options (--filter=, --run=, --list) pass straight through.
            test_args.append(arg)
        else:
            print(f"Unknown argument: '{arg}'")
            sys.exit(1)

    if test_args and "test" not in commands:
        print(f"Test arguments {test_args} given without the 'test' command.")
        sys.exit(1)

    if not commands:
        commands = ["clean", "generate", "shaders", "build"]

    for command in commands:
        if command == "clean":
            clean()
        elif command == "generate":
            generate_projects()
        elif command == "shaders":
            code = compile_project_shaders()
            if code != 0:
                sys.exit(code)
        elif command == "build":
            code = build(configuration)
            if code != 0:
                sys.exit(code)
        elif command == "test":
            code = run_tests(configuration, test_args)
            if code != 0:
                sys.exit(code)


if __name__ == "__main__":
    main()
