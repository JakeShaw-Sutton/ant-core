"""Compile production runtime/output/storage code with deterministic hardware doubles.

These tests exercise arm/disarm and loop transitions; they do not claim electrical
or flash-device validation. The native Unity suite tests pure ownership/mixing.
"""
import subprocess
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]

@pytest.fixture(scope="module", params=["s3", "c3"])
def runtime_executable(tmp_path_factory, request):
    executable = tmp_path_factory.mktemp("runtime") / "runtime.exe"
    sources = ["antcore_app_robot", "antcore_outputs", "antcore_logic", "antcore_controls",
               "antcore_output_test", "antcore_safety", "antcore_control_owner", "antcore_file_store", "antcore_sensors"]
    command = ["g++", "-std=c++17", "-Wall", "-Wextra", "-Itest/support", "-Iinclude", "-Isrc",
               "test/support/runtime_regression.cpp", *[f"src/{name}.cpp" for name in sources],
               "-o", str(executable)]
    if request.param == "c3":
        command.extend(["-DCONFIG_IDF_TARGET_ESP32C3=1", "src/camera_stream.cpp"])
    subprocess.run(command, cwd=ROOT, check=True)
    return executable

@pytest.mark.parametrize("scenario", ["servo", "trim", "cancel", "timeout", "reconnect", "storage", "tank", "arrival", "pwm", "capabilities", "battery"])
def test_runtime_transition(runtime_executable, scenario):
    subprocess.run([str(runtime_executable), scenario], check=True, timeout=10)
