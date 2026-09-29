import subprocess
import textwrap
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]


def test_antcore_logic_pure_functions(tmp_path):
    test_cpp = tmp_path / "antcore_logic_test.cpp"
    test_exe = tmp_path / "antcore_logic_test.exe"
    test_cpp.write_text(
        textwrap.dedent(
            r"""
            #include "antcore_logic.h"

            #include <cmath>
            #include <cstdlib>

            static void near(float actual, float expected, float tolerance = 0.001f) {
              if (std::fabs(actual - expected) > tolerance) std::abort();
            }

            int main() {
              using namespace antcore;

              near(clampFloat(2.0f, -1.0f, 1.0f), 1.0f);
              near(applyDeadbandExpo(0.02f, 0.07f, 0.25f), 0.0f);

              DriveInputs arcade;
              arcade.leftY = 1.0f;
              arcade.leftX = 0.5f;
              DriveMixConfig drive;
              drive.deadband = 0.0f;
              drive.expo = 0.0f;
              DriveMixResult mixed = computeDriveMix(arcade, drive);
              near(mixed.left, 1.0f);
              near(mixed.right, 0.333333f, 0.002f);

              arcade.turnCorrection = -0.25f;
              mixed = computeDriveMix(arcade, drive);
              near(mixed.left, 1.0f);
              near(mixed.right, 0.6f, 0.002f);
              arcade.turnCorrection = 0.0f;

              drive.driveInverted = true;
              mixed = computeDriveMix(arcade, drive);
              near(mixed.left, -0.333333f, 0.002f);
              near(mixed.right, -1.0f);

              DriveInputs tank;
              tank.leftY = 0.8f;
              tank.rightY = -0.4f;
              tank.precision = true;
              drive.tankMode = true;
              drive.driveInverted = false;
              drive.precisionScale = 0.5f;
              mixed = computeDriveMix(tank, drive);
              near(mixed.left, 0.4f);
              near(mixed.right, -0.2f);

              BatterySafetyResult battery = evaluateBatterySafety(6.6f, true, 7.0f, 6.4f, true, 6.8f);
              if (!battery.warn || battery.critical || !battery.derating) std::abort();

              battery = evaluateBatterySafety(6.2f, true, 7.0f, 6.4f, true, 6.8f);
              if (!battery.critical || battery.derating) std::abort();

              near(scaleBatteryVoltage(2.5f, 3.0f, 1.0f), 7.5f);
              near(scaleBatteryVoltage(2.5f, 3.0f, 1.1f), 8.25f);
              near(scaleBatteryVoltage(-1.0f, 3.0f, 1.0f), 0.0f);

              AxisCalibration cal;
              cal.minValue = -0.8f;
              cal.centerValue = 0.1f;
              cal.maxValue = 0.9f;
              cal.deadband = 0.05f;
              near(applyAxisCalibration(0.1f, cal, false), 0.0f);
              near(applyAxisCalibration(0.9f, cal, false), 1.0f);
              near(applyAxisCalibration(-0.8f, cal, false), -1.0f);
              cal.invert = true;
              near(applyAxisCalibration(0.9f, cal, false), -1.0f);

              AxisCalibration trigger;
              trigger.minValue = 0.0f;
              trigger.centerValue = 0.0f;
              trigger.maxValue = 1.0f;
              near(applyAxisCalibration(0.5f, trigger, true), 0.484536f, 0.002f);

              if (!shouldAutoInvertDrive(true, true, true, -0.55f, -0.45f)) std::abort();
              if (shouldAutoInvertDrive(true, true, false, -0.55f, -0.45f)) std::abort();
              if (shouldAutoInvertDrive(true, false, true, -0.55f, -0.45f)) std::abort();
              if (shouldAutoInvertDrive(true, true, true, -0.10f, -0.45f)) std::abort();

              if (!timedActionActive(1000, 1200, true)) std::abort();
              if (timedActionActive(1200, 1200, true)) std::abort();
              if (timedActionActive(1000, 1200, false)) std::abort();
              if (!cooldownReady(1000, 0, 1500)) std::abort();
              if (cooldownReady(1200, 1000, 1500)) std::abort();
              if (!cooldownReady(2500, 1000, 1500)) std::abort();
              if (cooldownRemainingMs(1200, 1000, 1500) != 1300) std::abort();
              if (cooldownRemainingMs(2600, 1000, 1500) != 0) std::abort();

              WeaponInputResult weapon = resolveWeaponInput(0.35f, false, false, false, false, 0.8f);
              near(weapon.value, 0.35f);
              if (weapon.inputWasPressed || weapon.toggleLatched) std::abort();

              weapon = resolveWeaponInput(0.0f, true, false, false, false, 0.8f);
              near(weapon.value, 0.8f);
              if (!weapon.inputWasPressed) std::abort();

              weapon = resolveWeaponInput(0.0f, true, true, false, false, 0.75f);
              near(weapon.value, 0.75f);
              if (!weapon.toggleLatched || !weapon.inputWasPressed) std::abort();

              weapon = resolveWeaponInput(0.0f, true, true, weapon.toggleLatched, weapon.inputWasPressed, 0.75f);
              near(weapon.value, 0.75f);
              if (!weapon.toggleLatched) std::abort();

              weapon = resolveWeaponInput(0.0f, false, true, weapon.toggleLatched, weapon.inputWasPressed, 0.75f);
              if (!weapon.toggleLatched || weapon.inputWasPressed) std::abort();

              weapon = resolveWeaponInput(0.0f, true, true, weapon.toggleLatched, weapon.inputWasPressed, 0.75f);
              near(weapon.value, 0.0f);
              if (weapon.toggleLatched) std::abort();

              weapon = resolveWeaponInput(0.7f, false, true, false, false, 0.9f);
              near(weapon.value, 0.9f);
              if (!weapon.toggleLatched) std::abort();

              if (evaluateArmBlock(true, false, false, true, false, false) != ArmBlockReason::OtaActive) std::abort();
              if (evaluateArmBlock(false, true, false, true, false, false) != ArmBlockReason::BenchMode) std::abort();
              if (evaluateArmBlock(false, false, true, true, false, false) != ArmBlockReason::BatteryCritical) std::abort();
              if (evaluateArmBlock(false, false, false, false, false, false) != ArmBlockReason::NoControlSource) std::abort();
              if (evaluateArmBlock(false, false, false, false, true, false) != ArmBlockReason::NoControlSource) std::abort();
              if (evaluateArmBlock(false, false, false, false, false, true) != ArmBlockReason::None) std::abort();
            }
            """
        ),
        encoding="utf-8",
    )
    subprocess.run(
        [
            "g++",
            "-std=c++17",
            "-I",
            str(ROOT / "include"),
            str(ROOT / "src" / "antcore_logic.cpp"),
            str(test_cpp),
            "-o",
            str(test_exe),
        ],
        check=True,
        cwd=ROOT,
    )
    subprocess.run([str(test_exe)], check=True)
