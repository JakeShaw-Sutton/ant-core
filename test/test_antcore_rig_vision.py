import json
from argparse import Namespace
from pathlib import Path

import numpy as np
import pytest

from tools.antcore_rig_vision_test import (
    HsvRange,
    LIVE_CONFIRMATION,
    Roi,
    RunContext,
    auto_calibrate_from_frame,
    boot_mode_from_text,
    build_motor_sweep,
    build_servo_sweep,
    camera_frame_summary,
    classify_motion,
    detect_orange_tape,
    ensure_out_dir,
    fake_calibration,
    orange_line_frame,
    placeholder_calibration_from_args,
    prompt_live_confirmation,
    save_camera_frame_artifact,
    write_blocked_reports,
)


def test_orange_tape_detection_finds_centroid_and_angle():
    calibration = fake_calibration()
    frame = orange_line_frame(calibration, {"motor1": 26.0})
    detection = detect_orange_tape(frame, calibration.rois["motor1"], HsvRange())

    assert detection.found
    assert detection.confidence > 0.4
    assert detection.centroid is not None
    assert detection.angle_deg is not None
    assert 15.0 <= detection.angle_deg <= 40.0


def test_motion_classification_detects_synthetic_actuator_change():
    calibration = fake_calibration()
    before = detect_orange_tape(orange_line_frame(calibration, {"servo1": 0.0}), calibration.rois["servo1"])
    after = detect_orange_tape(orange_line_frame(calibration, {"servo1": 35.0}), calibration.rois["servo1"])

    status, observed = classify_motion(before, after)

    assert status == "pass"
    assert "angle" in observed


def test_orange_detector_returns_not_found_for_non_orange_roi():
    frame = np.zeros((120, 120, 3), dtype=np.uint8)
    detection = detect_orange_tape(frame, Roi(0, 0, 120, 120))

    assert not detection.found
    assert detection.confidence == 0


def test_camera_frame_summary_identifies_black_and_orange_frames():
    assert "black frame" in camera_frame_summary(np.zeros((80, 120, 3), dtype=np.uint8))

    calibration = fake_calibration()
    summary = camera_frame_summary(orange_line_frame(calibration))
    assert "orange threshold pixels" in summary


def test_boot_mode_parser_extracts_download_mode():
    text = "rst:0x15 (USB_UART_CHIP_RESET),boot:0x0 (DOWNLOAD(USB/UART0))"

    assert boot_mode_from_text(text) == "DOWNLOAD(USB/UART0)"


def test_auto_roi_calibration_assigns_three_orange_markers_left_to_right():
    calibration = fake_calibration()
    frame = orange_line_frame(calibration, {"motor1": 0.0, "motor2": 20.0, "servo1": -20.0})

    auto = auto_calibrate_from_frame(frame, "dry-run", roi_order="motor1,motor2,servo1", padding=20)

    assert list(auto.rois) == ["motor1", "motor2", "servo1"]
    assert auto.rois["motor1"].x < auto.rois["motor2"].x < auto.rois["servo1"].x
    assert all(det.found for det in auto.baselines.values())


def test_motor_and_servo_commands_are_bounded():
    assert build_motor_sweep("full", "-2,-0.25,0.25,2") == [-1.0, -0.25, 0.25, 1.0]
    assert build_motor_sweep("safe") == [-0.35, 0.35]

    config = {"servos": [{"index": 1, "minUs": 900, "neutralUs": 1500, "maxUs": 2100}]}
    assert build_servo_sweep(config, "200,1500,3000") == [500, 1500, 2500]
    assert build_servo_sweep(config) == [900, 1500, 2100, 1500]


def test_live_output_requires_flag_and_confirmation():
    args = Namespace(dry_run=False, live_output=False, confirm_live_output=False)
    with pytest.raises(RuntimeError, match="--live-output"):
        prompt_live_confirmation(args)
    assert LIVE_CONFIRMATION == "LIVE OUTPUTS ARE CLEAR"


def test_report_output_directory_uses_rig_test_prefix(tmp_path):
    output = ensure_out_dir(str(tmp_path / "reports" / "rig-test-unit"))

    assert output.name == "rig-test-unit"
    assert (output / "stills").exists()


def test_blocked_report_writes_results_and_placeholder_videos(tmp_path):
    args = Namespace(
        camera_index="0",
        camera_width=640,
        camera_height=480,
        hsv_lower=list((0, 80, 60)),
        hsv_upper=list((35, 255, 255)),
        video_fps=1,
        combine_video=True,
    )
    ctx = RunContext(ensure_out_dir(str(tmp_path / "blocked")))
    calibration = placeholder_calibration_from_args(args)
    artifact = save_camera_frame_artifact(ctx, np.zeros((80, 120, 3), dtype=np.uint8), "candidate.png", "camera candidate")
    ctx.event("vision", "candidate frame", "fail", "visible tape", "none", "saved camera frame", artifact)

    write_blocked_reports(ctx, args, calibration, "camera calibration blocked", "no orange tape detected")

    results = json.loads((ctx.out_dir / "results.json").read_text(encoding="utf-8"))
    assert (ctx.out_dir / "report.html").exists()
    assert (ctx.out_dir / "report.md").exists()
    assert (ctx.out_dir / "events.csv").exists()
    assert results["events"][0]["status"] == "fail"
    assert Path(results["events"][0]["artifact"]).exists()
    assert "No live motor or servo movement was attempted" in (ctx.out_dir / "report.md").read_text(encoding="utf-8")
    assert "candidate.png" in (ctx.out_dir / "report.md").read_text(encoding="utf-8")
    assert Path(results["videos"]["rig_validation"]).exists()
    assert Path(results["videos"]["dashboard_tutorial"]).exists()
    assert Path(results["videos"]["combined"]).exists()
