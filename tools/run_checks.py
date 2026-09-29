#!/usr/bin/env python3
"""Run repeatable Ant Core validation checks."""

from __future__ import annotations

import argparse
import importlib.util
import os
import subprocess
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def run(command: list[str]) -> None:
  print("$ " + " ".join(command), flush=True)
  subprocess.run(command, cwd=ROOT, check=True)


def platformio_command(override: str | None = None) -> list[str]:
  if override:
    return [override]
  # VS Code's PlatformIO environment avoids accidentally using a second global Core.
  core = Path(os.environ.get("PLATFORMIO_CORE_DIR", str(Path.home() / ".platformio")))
  executable = core / "penv" / ("Scripts/pio.exe" if os.name == "nt" else "bin/pio")
  if executable.is_file():
    return [str(executable)]
  if importlib.util.find_spec("platformio"):
    return [sys.executable, "-m", "platformio"]
  raise SystemExit("PlatformIO is missing. Install requirements.txt or pass --pio PATH.")


def main() -> int:
  parser = argparse.ArgumentParser(description=__doc__)
  parser.add_argument("--skip-build", action="store_true", help="Skip PlatformIO firmware/filesystem builds.")
  parser.add_argument("--skip-ui", action="store_true", help="Skip the headless mock UI audit.")
  parser.add_argument("--pio", help="Explicit PlatformIO executable path.")
  args = parser.parse_args()
  pio = platformio_command(args.pio) if not args.skip_build else []

  if not args.skip_build:
    run([*pio, "run", "-e", "seeed_xiao_esp32s3", "-e", "seeed_xiao_esp32c3"])
    run([*pio, "run", "-e", "boot_probe"])
    run([*pio, "run", "-e", "seeed_xiao_esp32s3", "-e", "seeed_xiao_esp32c3", "--target", "buildfs"])
    run([*pio, "test", "-e", "native", "-f", "test_antcore_logic_native"])
  run([sys.executable, "-m", "pytest", "-q", "--tb=short"])
  if not args.skip_ui:
    run([sys.executable, "tools/antcore_ui_audit.py"])
  return 0


if __name__ == "__main__":
  raise SystemExit(main())
