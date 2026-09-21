from pathlib import Path
import subprocess
import sys


def main() -> int:
	project_root = Path(__file__).resolve().parents[2]
	premake = project_root / "vendor" / "premake" / "bin" / "premake5.exe"

	version = sys.argv[1] if len(sys.argv) > 1 else "vs2026"

	if not premake.is_file():
		print(f"ERROR: Premake executable not found: {premake}", file=sys.stderr)
		return 1

	result = subprocess.run([str(premake), version], cwd=project_root, check=False)

	if result.returncode != 0:
		print(f"\nERROR: premake5 failed with exit code {result.returncode}.\n", file=sys.stderr)

	return result.returncode


if __name__ == "__main__":
	raise SystemExit(main())
