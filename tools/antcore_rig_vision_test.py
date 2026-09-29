#!/usr/bin/env python3
"""Hardware-in-the-loop vision tester for the Ant Core rig.

The live motion path is intentionally conservative:
- movement requires --live-output and a typed runtime confirmation
- battery and arming policy stay inside the firmware
- the tool never disables battery safety or pit/mapping safety
- the robot is disarmed before and after each movement group

Use --dry-run for report, video, and detector validation without hardware.
"""

from __future__ import annotations

import argparse
import base64
import concurrent.futures
import csv
import getpass
import io
import json
import math
import os
import socket
import sys
import tempfile
import time
import urllib.error
import urllib.parse
import urllib.request
from dataclasses import asdict, dataclass, field
from datetime import datetime
from pathlib import Path
from typing import Any, Iterable

import numpy as np

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:  # pragma: no cover - Pillow is available in the Codex app runtime.
    raise SystemExit("Pillow is required for annotation frames") from exc


ROOT = Path(__file__).resolve().parents[1]
if str(ROOT) not in sys.path:
    sys.path.insert(0, str(ROOT))
if str(ROOT / "tools") not in sys.path:
    sys.path.insert(0, str(ROOT / "tools"))

try:
    from antcore_serial_bridge import BAUD, SerialBridge, choose_port
except Exception:  # pragma: no cover - handled at runtime for dry-run/no pyserial cases.
    BAUD = 115200
    SerialBridge = None  # type: ignore[assignment]

    def choose_port(preferred: str | None = None) -> str:  # type: ignore[no-redef]
        if preferred:
            return preferred
        raise RuntimeError("pyserial port discovery unavailable; pass --port")


DEFAULT_CALIBRATION = ROOT / "tools" / "antcore_rig_calibration.json"
DEFAULT_HSV_LOWER = (0, 80, 60)
DEFAULT_HSV_UPPER = (35, 255, 255)
DEFAULT_MOTOR_POWERS = (-1.0, -0.5, 0.5, 1.0)
DEFAULT_SERVO_US = (1000, 1500, 2000, 1500)
LIVE_CONFIRMATION = "LIVE OUTPUTS ARE CLEAR"
FRAME_SIZE = (1280, 720)


@dataclass
class Roi:
    x: int
    y: int
    w: int
    h: int

    def clamp(self, width: int, height: int) -> "Roi":
        x = max(0, min(self.x, max(0, width - 1)))
        y = max(0, min(self.y, max(0, height - 1)))
        w = max(1, min(self.w, width - x))
        h = max(1, min(self.h, height - y))
        return Roi(x, y, w, h)

    @property
    def area(self) -> int:
        return self.w * self.h


@dataclass
class HsvRange:
    lower: tuple[int, int, int] = DEFAULT_HSV_LOWER
    upper: tuple[int, int, int] = DEFAULT_HSV_UPPER


@dataclass
class Detection:
    found: bool
    centroid: tuple[float, float] | None
    angle_deg: float | None
    area: int
    confidence: float
    bbox: tuple[int, int, int, int] | None = None


@dataclass
class VisionCalibration:
    camera_index: str = "0"
    resolution: tuple[int, int] = FRAME_SIZE
    hsv: HsvRange = field(default_factory=HsvRange)
    rois: dict[str, Roi] = field(default_factory=dict)
    baselines: dict[str, Detection] = field(default_factory=dict)


@dataclass
class ApiResponse:
    ok: bool
    status: int
    body: Any
    error: str = ""


@dataclass
class RigEvent:
    timestamp: str
    category: str
    name: str
    status: str
    expected: str = ""
    observed: str = ""
    details: str = ""
    artifact: str = ""


@dataclass
class RunContext:
    out_dir: Path
    events: list[RigEvent] = field(default_factory=list)
    validation_frames: list[Image.Image] = field(default_factory=list)
    tutorial_frames: list[Image.Image] = field(default_factory=list)
    board: dict[str, Any] = field(default_factory=dict)
    camera: dict[str, Any] = field(default_factory=dict)
    skipped: list[str] = field(default_factory=list)

    def event(
        self,
        category: str,
        name: str,
        status: str,
        expected: str = "",
        observed: str = "",
        details: str = "",
        artifact: str = "",
    ) -> RigEvent:
        item = RigEvent(
            timestamp=datetime.now().isoformat(timespec="seconds"),
            category=category,
            name=name,
            status=status,
            expected=expected,
            observed=observed,
            details=details,
            artifact=artifact,
        )
        self.events.append(item)
        return item


def now_stamp() -> str:
    return datetime.now().strftime("%Y%m%d-%H%M%S")


def ensure_out_dir(path: str | None) -> Path:
    out = Path(path) if path else ROOT / "reports" / f"rig-test-{now_stamp()}"
    out.mkdir(parents=True, exist_ok=True)
    (out / "stills").mkdir(exist_ok=True)
    return out.resolve()


def rgb_to_hsv_np(frame_rgb: np.ndarray) -> np.ndarray:
    """Convert uint8 RGB to OpenCV-style HSV (H 0..179, S/V 0..255)."""
    rgb = frame_rgb.astype(np.float32) / 255.0
    r = rgb[..., 0]
    g = rgb[..., 1]
    b = rgb[..., 2]
    maxc = np.max(rgb, axis=-1)
    minc = np.min(rgb, axis=-1)
    delta = maxc - minc
    hue = np.zeros_like(maxc)
    mask = delta > 1e-6
    rmask = mask & (maxc == r)
    gmask = mask & (maxc == g)
    bmask = mask & (maxc == b)
    hue[rmask] = ((g[rmask] - b[rmask]) / delta[rmask]) % 6
    hue[gmask] = ((b[gmask] - r[gmask]) / delta[gmask]) + 2
    hue[bmask] = ((r[bmask] - g[bmask]) / delta[bmask]) + 4
    hue = hue * 30.0
    sat = np.zeros_like(maxc)
    np.divide(delta, maxc, out=sat, where=maxc > 1e-6)
    sat *= 255.0
    val = maxc * 255.0
    return np.stack([hue, sat, val], axis=-1).astype(np.uint8)


def detect_orange_tape(frame_rgb: np.ndarray, roi: Roi, hsv: HsvRange | None = None) -> Detection:
    hsv = hsv or HsvRange()
    height, width = frame_rgb.shape[:2]
    roi = roi.clamp(width, height)
    crop = frame_rgb[roi.y : roi.y + roi.h, roi.x : roi.x + roi.w]
    hsv_crop = rgb_to_hsv_np(crop)
    lower = np.array(hsv.lower, dtype=np.uint8)
    upper = np.array(hsv.upper, dtype=np.uint8)
    if lower[0] <= upper[0]:
        hue_mask = (hsv_crop[..., 0] >= lower[0]) & (hsv_crop[..., 0] <= upper[0])
    else:
        hue_mask = (hsv_crop[..., 0] >= lower[0]) | (hsv_crop[..., 0] <= upper[0])
    mask = hue_mask & (hsv_crop[..., 1] >= lower[1]) & (hsv_crop[..., 1] <= upper[1])
    mask &= (hsv_crop[..., 2] >= lower[2]) & (hsv_crop[..., 2] <= upper[2])
    ys, xs = np.nonzero(mask)
    area = int(xs.size)
    if area < max(8, int(roi.area * 0.002)):
        return Detection(False, None, None, area, 0.0, None)

    cx = float(xs.mean() + roi.x)
    cy = float(ys.mean() + roi.y)
    bbox = (int(xs.min() + roi.x), int(ys.min() + roi.y), int(xs.max() + roi.x), int(ys.max() + roi.y))
    angle: float | None = None
    if area >= 20:
        points = np.column_stack((xs.astype(np.float32), ys.astype(np.float32)))
        centered = points - points.mean(axis=0)
        cov = np.cov(centered, rowvar=False)
        try:
            values, vectors = np.linalg.eigh(cov)
            major = vectors[:, int(np.argmax(values))]
            angle = math.degrees(math.atan2(float(major[1]), float(major[0])))
            while angle <= -90:
                angle += 180
            while angle > 90:
                angle -= 180
        except np.linalg.LinAlgError:
            angle = None
    confidence = min(1.0, area / max(1.0, roi.area * 0.035))
    return Detection(True, (cx, cy), angle, area, confidence, bbox)


def signed_axis_delta(before: float | None, after: float | None) -> float | None:
    if before is None or after is None:
        return None
    return ((after - before + 90.0) % 180.0) - 90.0


def classify_motion(
    before: Detection,
    after: Detection,
    expected_sign: int | None = None,
    min_centroid_px: float = 3.5,
    min_angle_deg: float = 7.0,
) -> tuple[str, str]:
    if not before.found:
        return "inconclusive", "baseline orange tape was not detected"
    if not after.found:
        return "fail", "orange tape was not detected after command"
    centroid_delta = 0.0
    if before.centroid and after.centroid:
        centroid_delta = math.dist(before.centroid, after.centroid)
    angle_delta = signed_axis_delta(before.angle_deg, after.angle_deg)
    moved = centroid_delta >= min_centroid_px or (angle_delta is not None and abs(angle_delta) >= min_angle_deg)
    summary = f"centroid {centroid_delta:.1f}px"
    if angle_delta is not None:
        summary += f", angle {angle_delta:+.1f}deg"
    summary += f", confidence {after.confidence:.2f}"
    if not moved:
        return "fail", summary
    if expected_sign and angle_delta is not None and abs(angle_delta) >= min_angle_deg:
        if math.copysign(1, angle_delta) != math.copysign(1, expected_sign):
            return "fail", summary + ", direction opposite expected sign"
        return "pass", summary + ", direction matched expected sign"
    if expected_sign:
        return "inconclusive", summary + ", direction not reliable"
    return "pass", summary


def serialize_detection(det: Detection) -> dict[str, Any]:
    return asdict(det)


def calibration_to_json(calibration: VisionCalibration) -> dict[str, Any]:
    return {
        "camera_index": calibration.camera_index,
        "resolution": list(calibration.resolution),
        "hsv": {"lower": list(calibration.hsv.lower), "upper": list(calibration.hsv.upper)},
        "rois": {name: asdict(roi) for name, roi in calibration.rois.items()},
        "baselines": {name: serialize_detection(det) for name, det in calibration.baselines.items()},
    }


def detection_from_json(data: dict[str, Any]) -> Detection:
    centroid = data.get("centroid")
    bbox = data.get("bbox")
    return Detection(
        bool(data.get("found")),
        tuple(centroid) if centroid else None,
        data.get("angle_deg"),
        int(data.get("area", 0)),
        float(data.get("confidence", 0)),
        tuple(bbox) if bbox else None,
    )


def calibration_from_json(data: dict[str, Any]) -> VisionCalibration:
    hsv = data.get("hsv", {})
    return VisionCalibration(
        camera_index=str(data.get("camera_index", "0")),
        resolution=tuple(data.get("resolution", FRAME_SIZE)),  # type: ignore[arg-type]
        hsv=HsvRange(tuple(hsv.get("lower", DEFAULT_HSV_LOWER)), tuple(hsv.get("upper", DEFAULT_HSV_UPPER))),  # type: ignore[arg-type]
        rois={name: Roi(**roi) for name, roi in data.get("rois", {}).items()},
        baselines={name: detection_from_json(det) for name, det in data.get("baselines", {}).items()},
    )


def save_calibration(path: Path, calibration: VisionCalibration) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(calibration_to_json(calibration), indent=2), encoding="utf-8")


def load_calibration(path: Path) -> VisionCalibration:
    return calibration_from_json(json.loads(path.read_text(encoding="utf-8")))


def fake_calibration() -> VisionCalibration:
    return VisionCalibration(
        camera_index="dry-run",
        resolution=FRAME_SIZE,
        hsv=HsvRange(),
        rois={
            "motor1": Roi(110, 240, 260, 220),
            "motor2": Roi(470, 240, 260, 220),
            "servo1": Roi(830, 240, 260, 220),
        },
    )


def expand_roi(box: tuple[int, int, int, int], width: int, height: int, padding: int) -> Roi:
    x, y, w, h = box
    x0 = max(0, x - padding)
    y0 = max(0, y - padding)
    x1 = min(width, x + w + padding)
    y1 = min(height, y + h + padding)
    return Roi(x0, y0, max(1, x1 - x0), max(1, y1 - y0))


def auto_calibrate_from_frame(
    frame_rgb: np.ndarray,
    camera_index: str,
    hsv: HsvRange | None = None,
    roi_order: str = "motor1,motor2,servo1",
    padding: int = 70,
    min_area: int = 60,
) -> VisionCalibration:
    """Find orange tape components and assign ROIs left-to-right by default."""
    hsv = hsv or HsvRange()
    height, width = frame_rgb.shape[:2]
    hsv_frame = rgb_to_hsv_np(frame_rgb)
    lower = np.array(hsv.lower, dtype=np.uint8)
    upper = np.array(hsv.upper, dtype=np.uint8)
    if lower[0] <= upper[0]:
        hue_mask = (hsv_frame[..., 0] >= lower[0]) & (hsv_frame[..., 0] <= upper[0])
    else:
        hue_mask = (hsv_frame[..., 0] >= lower[0]) | (hsv_frame[..., 0] <= upper[0])
    mask = hue_mask & (hsv_frame[..., 1] >= lower[1]) & (hsv_frame[..., 1] <= upper[1])
    mask &= (hsv_frame[..., 2] >= lower[2]) & (hsv_frame[..., 2] <= upper[2])
    try:
        import cv2  # type: ignore

        count, labels, stats, centroids = cv2.connectedComponentsWithStats(mask.astype(np.uint8), 8)
        components: list[tuple[int, int, int, int, int, float, float]] = []
        for label in range(1, count):
            x, y, w, h, area = [int(value) for value in stats[label]]
            if area >= min_area:
                cx, cy = [float(value) for value in centroids[label]]
                components.append((x, y, w, h, area, cx, cy))
    except Exception:
        ys, xs = np.nonzero(mask)
        components = []
        if xs.size >= min_area:
            components.append((int(xs.min()), int(ys.min()), int(xs.max() - xs.min() + 1), int(ys.max() - ys.min() + 1), int(xs.size), float(xs.mean()), float(ys.mean())))

    names = [part.strip() for part in roi_order.split(",") if part.strip()]
    components = sorted(components, key=lambda item: item[4], reverse=True)[: len(names)]
    components = sorted(components, key=lambda item: item[5])
    if len(components) < len(names):
        raise RuntimeError(
            f"auto ROI calibration found {len(components)} orange components; expected {len(names)}. "
            "Improve lighting/tape visibility or use manual calibration."
        )
    calibration = VisionCalibration(camera_index=str(camera_index), resolution=(width, height), hsv=hsv)
    for name, component in zip(names, components):
        x, y, w, h, *_ = component
        calibration.rois[name] = expand_roi((x, y, w, h), width, height, padding)
    calibration.baselines = {
        name: detect_orange_tape(frame_rgb, roi, calibration.hsv) for name, roi in calibration.rois.items()
    }
    return calibration


def draw_calibration_preview(frame_rgb: np.ndarray, calibration: VisionCalibration) -> Image.Image:
    image = Image.fromarray(frame_rgb).convert("RGB")
    draw = ImageDraw.Draw(image, "RGBA")
    font = load_font(22)
    colors = [(255, 202, 79), (81, 213, 132), (110, 180, 255), (255, 93, 93)]
    for index, (name, roi) in enumerate(calibration.rois.items()):
        color = colors[index % len(colors)]
        draw.rectangle((roi.x, roi.y, roi.x + roi.w, roi.y + roi.h), outline=color + (255,), width=4)
        detection = calibration.baselines.get(name)
        label = name
        if detection:
            label += f" conf {detection.confidence:.2f}"
            if detection.centroid:
                cx, cy = detection.centroid
                draw.ellipse((cx - 7, cy - 7, cx + 7, cy + 7), fill=color + (230,))
        draw.text((roi.x + 8, max(0, roi.y - 30)), label, fill=color, font=font)
    return overlay_text(image, ["Ant Core automatic camera calibration", "Check each box encloses one orange tape marker."])


def save_camera_frame_artifact(ctx: RunContext | None, frame_rgb: np.ndarray, name: str, caption: str) -> str:
    if ctx is None:
        return ""
    safe_name = "".join(ch if ch.isalnum() or ch in ("-", "_", ".") else "_" for ch in name)
    path = ctx.out_dir / "stills" / safe_name
    image = overlay_text(Image.fromarray(frame_rgb).convert("RGB"), [caption])
    image.save(path)
    return str(path)


def orange_pixel_count(frame_rgb: np.ndarray, hsv: HsvRange | None = None) -> int:
    hsv = hsv or HsvRange()
    hsv_frame = rgb_to_hsv_np(frame_rgb)
    lower = np.array(hsv.lower, dtype=np.uint8)
    upper = np.array(hsv.upper, dtype=np.uint8)
    if lower[0] <= upper[0]:
        hue_mask = (hsv_frame[..., 0] >= lower[0]) & (hsv_frame[..., 0] <= upper[0])
    else:
        hue_mask = (hsv_frame[..., 0] >= lower[0]) | (hsv_frame[..., 0] <= upper[0])
    mask = hue_mask & (hsv_frame[..., 1] >= lower[1]) & (hsv_frame[..., 1] <= upper[1])
    mask &= (hsv_frame[..., 2] >= lower[2]) & (hsv_frame[..., 2] <= upper[2])
    return int(np.count_nonzero(mask))


def camera_frame_summary(frame_rgb: np.ndarray, hsv: HsvRange | None = None) -> str:
    hsv = hsv or HsvRange()
    gray = frame_rgb.astype(np.float32).mean(axis=2)
    mean = float(gray.mean())
    std = float(gray.std())
    orange_pixels = orange_pixel_count(frame_rgb, hsv)
    if mean < 4.0 and std < 4.0:
        return f"black frame, mean brightness {mean:.1f}, orange pixels {orange_pixels}"
    if orange_pixels == 0:
        return f"no orange threshold pixels, mean brightness {mean:.1f}, contrast {std:.1f}"
    return f"{orange_pixels} orange threshold pixels, mean brightness {mean:.1f}, contrast {std:.1f}"


def orange_line_frame(
    calibration: VisionCalibration,
    offsets: dict[str, float] | None = None,
    size: tuple[int, int] = FRAME_SIZE,
) -> np.ndarray:
    offsets = offsets or {}
    width, height = size
    img = Image.new("RGB", (width, height), (12, 14, 18))
    draw = ImageDraw.Draw(img)
    for name, roi in calibration.rois.items():
        draw.rectangle((roi.x, roi.y, roi.x + roi.w, roi.y + roi.h), outline=(58, 69, 78), width=2)
        center = (roi.x + roi.w / 2, roi.y + roi.h / 2)
        angle = offsets.get(name, 0.0)
        length = min(roi.w, roi.h) * 0.7
        dx = math.cos(math.radians(angle)) * length / 2
        dy = math.sin(math.radians(angle)) * length / 2
        draw.line((center[0] - dx, center[1] - dy, center[0] + dx, center[1] + dy), fill=(255, 106, 0), width=18)
    return np.array(img)


def load_font(size: int = 20) -> ImageFont.ImageFont:
    for candidate in [
        Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts" / "segoeui.ttf",
        Path(os.environ.get("WINDIR", "C:/Windows")) / "Fonts" / "arial.ttf",
    ]:
        if candidate.exists():
            return ImageFont.truetype(str(candidate), size=size)
    return ImageFont.load_default()


def overlay_text(frame: Image.Image, lines: Iterable[str], accent: tuple[int, int, int] = (255, 202, 79)) -> Image.Image:
    out = frame.convert("RGB").copy()
    draw = ImageDraw.Draw(out, "RGBA")
    font = load_font(22)
    small = load_font(16)
    lines = [line for line in lines if line]
    if not lines:
        return out
    width = out.width
    pad = 18
    box_h = 34 + 28 * len(lines)
    draw.rectangle((0, out.height - box_h, width, out.height), fill=(5, 7, 9, 218))
    draw.rectangle((0, out.height - box_h, width, out.height - box_h + 4), fill=accent + (255,))
    for i, line in enumerate(lines):
        draw.text((pad, out.height - box_h + 18 + i * 28), line, fill=(239, 245, 244), font=font if i == 0 else small)
    return out


def annotate_detection(
    frame_rgb: np.ndarray,
    roi: Roi,
    detection: Detection,
    caption: str,
    status: str = "info",
) -> Image.Image:
    image = Image.fromarray(frame_rgb).convert("RGB")
    draw = ImageDraw.Draw(image, "RGBA")
    color = {"pass": (81, 213, 132), "fail": (255, 93, 93), "inconclusive": (255, 202, 79)}.get(status, (110, 180, 255))
    draw.rectangle((roi.x, roi.y, roi.x + roi.w, roi.y + roi.h), outline=color + (255,), width=4)
    if detection.bbox:
        draw.rectangle(detection.bbox, outline=(255, 106, 0, 255), width=3)
    if detection.centroid:
        cx, cy = detection.centroid
        draw.ellipse((cx - 7, cy - 7, cx + 7, cy + 7), fill=color + (230,))
    lines = [caption, f"status {status}  area {detection.area}  confidence {detection.confidence:.2f}"]
    if detection.angle_deg is not None:
        lines[-1] += f"  angle {detection.angle_deg:+.1f}deg"
    return overlay_text(image, lines, color)


class CameraBase:
    def read_rgb(self) -> np.ndarray:
        raise NotImplementedError

    def close(self) -> None:
        return


class FakeCamera(CameraBase):
    def __init__(self, calibration: VisionCalibration) -> None:
        self.calibration = calibration
        self.offsets = {"motor1": 0.0, "motor2": 0.0, "servo1": 0.0}

    def apply_command(self, actuator: str, value: float) -> None:
        if actuator.startswith("motor"):
            self.offsets[actuator] += 28.0 * math.copysign(1, value or 1.0)
        elif actuator.startswith("servo"):
            self.offsets[actuator] = (value - 1500.0) / 500.0 * 38.0

    def read_rgb(self) -> np.ndarray:
        return orange_line_frame(self.calibration, self.offsets, self.calibration.resolution)


class UsbCamera(CameraBase):
    def __init__(self, camera_index: str, width: int | None = None, height: int | None = None, backend: str = "dshow") -> None:
        try:
            import cv2  # type: ignore
        except ImportError as exc:  # pragma: no cover
            raise RuntimeError("opencv-python is required for USB camera capture") from exc
        self.cv2 = cv2
        index: int | str = int(camera_index) if str(camera_index).isdigit() else camera_index
        self.cap = cv2.VideoCapture(index, cv2_backend(cv2, backend))
        if not self.cap.isOpened():
            raise RuntimeError(f"camera {camera_index} did not open")
        if width:
            self.cap.set(cv2.CAP_PROP_FRAME_WIDTH, width)
        if height:
            self.cap.set(cv2.CAP_PROP_FRAME_HEIGHT, height)
        for _ in range(5):
            self.read_rgb()

    def read_rgb(self) -> np.ndarray:
        ok, frame_bgr = self.cap.read()
        if not ok:
            raise RuntimeError("camera frame read failed")
        return self.cv2.cvtColor(frame_bgr, self.cv2.COLOR_BGR2RGB)

    def close(self) -> None:
        self.cap.release()


def cv2_backend(cv2: Any, backend: str) -> int:
    if os.name != "nt":
        return 0
    if backend == "msmf":
        return cv2.CAP_MSMF
    if backend == "any":
        return 0
    return cv2.CAP_DSHOW


def enumerate_cameras(max_index: int = 8, backend: str = "dshow") -> list[int]:
    try:
        import cv2  # type: ignore
    except ImportError as exc:  # pragma: no cover
        raise RuntimeError("opencv-python is required to enumerate cameras") from exc
    found: list[int] = []
    for index in range(max_index):
        cap = cv2.VideoCapture(index, cv2_backend(cv2, backend))
        if cap.isOpened():
            ok, _ = cap.read()
            if ok:
                found.append(index)
        cap.release()
    return found


class AntCoreClient:
    def __init__(self, base_url: str, admin_pin: str = "", timeout: float = 3.0) -> None:
        self.base_url = base_url.rstrip("/")
        self.admin_pin = admin_pin
        self.timeout = timeout
        self.token = ""

    def login(self) -> ApiResponse:
        if not self.admin_pin:
            return ApiResponse(True, 200, {"ok": True, "authSkipped": True})
        response = self.request("POST", "/api/auth/login", {"pin": self.admin_pin}, auth=False)
        if response.ok and isinstance(response.body, dict):
            self.token = str(response.body.get("token", ""))
        return response

    def get_status(self) -> dict[str, Any]:
        response = self.request("GET", "/api/status", auth=False)
        return response.body if response.ok and isinstance(response.body, dict) else {}

    def get_config(self) -> dict[str, Any]:
        response = self.request("GET", "/api/config")
        return response.body if response.ok and isinstance(response.body, dict) else {}

    def request(self, method: str, path: str, body: Any | None = None, auth: bool = True) -> ApiResponse:
        url = self.base_url + path
        headers = {"Accept": "application/json"}
        if auth and self.token:
            headers["X-AntCore-Token"] = self.token
        data = None
        if body is not None:
            data = json.dumps(body).encode("utf-8")
            headers["Content-Type"] = "application/json"
        req = urllib.request.Request(url, data=data, method=method.upper(), headers=headers)
        try:
            with urllib.request.urlopen(req, timeout=self.timeout) as response:
                raw = response.read().decode("utf-8", errors="replace")
                parsed = json.loads(raw) if raw.strip().startswith(("{", "[")) else raw
                ok = 200 <= response.status < 300 and (not isinstance(parsed, dict) or parsed.get("ok", True))
                return ApiResponse(ok, response.status, parsed)
        except urllib.error.HTTPError as exc:
            raw = exc.read().decode("utf-8", errors="replace")
            try:
                parsed = json.loads(raw)
            except json.JSONDecodeError:
                parsed = raw
            return ApiResponse(False, exc.code, parsed, str(exc))
        except Exception as exc:
            return ApiResponse(False, 0, {}, str(exc))


class FakeAntCoreClient(AntCoreClient):
    def __init__(self) -> None:
        super().__init__("http://127.0.0.1")
        self.token = "dry-run-token"
        self.armed = False
        self.live_output = False
        self.status = {
            "schema": 10,
            "firmwareVersion": "dry-run",
            "filesystemVersion": "dry-run",
            "armed": False,
            "battery": {"packVolts": 7.8, "cellVolts": 3.9, "critical": False, "warn": False, "benchMode": False},
            "camera": {"ready": True, "streamUrl": "/stream"},
            "safety": {"canArm": True, "armBlockReason": "", "pitMode": False, "batteryCritical": False},
            "wifi": {"sta": {"connected": True, "ip": "127.0.0.1"}, "ap": {"ip": "192.168.4.1"}},
            "controller": {"source": "none", "webClients": 1},
            "events": {"controlDisconnects": 0, "failsafes": 0, "weaponArms": 0, "disarms": 0},
            "system": {"resetReason": "dry-run"},
            "weapon": {"armed": False, "enabled": False},
            "logs": ["dry-run boot"],
        }

    def login(self) -> ApiResponse:
        return ApiResponse(True, 200, {"ok": True, "token": self.token})

    def get_status(self) -> dict[str, Any]:
        self.status["armed"] = self.armed
        self.status["liveOutputEnabled"] = self.live_output
        return self.status

    def get_config(self) -> dict[str, Any]:
        return {
            "schema": 10,
            "robotName": "Dry Run Ant",
            "activeProfile": "Default",
            "drive": {"mode": "arcade", "leftMotor": 1, "rightMotor": 2, "throttleAxis": "leftY", "turnAxis": "leftX"},
            "servos": [{"index": 1, "enabled": True, "minUs": 1000, "neutralUs": 1500, "maxUs": 2000}],
        }

    def request(self, method: str, path: str, body: Any | None = None, auth: bool = True) -> ApiResponse:
        if path == "/api/status":
            return ApiResponse(True, 200, self.get_status())
        if path in ("/api/config", "/api/config/export"):
            return ApiResponse(True, 200, self.get_config())
        if path == "/api/auth/login":
            return self.login()
        if path == "/api/arm":
            self.armed = True
            return ApiResponse(True, 200, {"ok": True, "armed": True})
        if path == "/api/disarm":
            self.armed = False
            return ApiResponse(True, 200, {"ok": True, "armed": False})
        if path == "/api/test/live-output":
            self.live_output = bool((body or {}).get("enabled"))
            return ApiResponse(True, 200, {"ok": True, "liveOutputEnabled": self.live_output})
        if path in ("/api/test/motor", "/api/test/servo", "/api/ble/scan", "/api/control/release", "/api/weapon/disarm"):
            return ApiResponse(True, 200, {"ok": True, "message": "dry-run"})
        if path.startswith("/api/ota"):
            return ApiResponse(True, 200, {"ok": True, "available": True})
        return ApiResponse(True, 200, {"ok": True})


def base_url_from_status(status: dict[str, Any]) -> str:
    wifi = status.get("wifi", {})
    sta = wifi.get("sta", {})
    ap = wifi.get("ap", {})
    for ip in [sta.get("ip"), wifi.get("ip"), ap.get("ip")]:
        if ip and ip != "0.0.0.0":
            return f"http://{ip}"
    return "http://192.168.4.1"


def discover_serial(port: str | None, baud: int, ctx: RunContext | None = None) -> dict[str, Any]:
    if SerialBridge is None:
        raise RuntimeError("pyserial is required for serial discovery")
    selected = choose_port(port)
    bridge = SerialBridge(selected, baud)
    commands: dict[str, list[str]] = {}
    try:
        bridge.connect()
        for command in ["PING", "STATUS", "CONFIG?", "DISARM"]:
            commands[command] = bridge.command(command, wait=1.5)
        snapshot = bridge.snapshot()
    finally:
        bridge.close()
    status = snapshot.get("status") or {}
    config = snapshot.get("config") or {}
    board = {
        "port": selected,
        "baud": baud,
        "serial_ok": any(commands.values()),
        "status": status,
        "config": config,
        "base_url": base_url_from_status(status),
        "serial_commands": commands,
        "raw": snapshot.get("raw", []),
    }
    if ctx:
        ctx.board = board
        ctx.event(
            "serial",
            "serial discovery",
            "pass" if status else "fail" if not board["serial_ok"] else "inconclusive",
            "PING STATUS CONFIG? DISARM",
            board["base_url"],
            f"port {selected}",
        )
        if not board["serial_ok"]:
            boot_probe = probe_serial_boot_mode(selected, baud)
            board["boot_probe"] = boot_probe
            mode = boot_probe.get("boot_mode") or "unknown"
            ctx.event(
                "serial",
                "boot mode probe",
                "fail" if boot_probe.get("download_mode") else "inconclusive",
                "application boot mode",
                mode,
                boot_probe.get("error", ""),
            )
    return board


def discover_dry_run(ctx: RunContext | None = None) -> dict[str, Any]:
    board = {
        "port": "dry-run",
        "baud": BAUD,
        "serial_ok": True,
        "status": FakeAntCoreClient().get_status(),
        "config": FakeAntCoreClient().get_config(),
        "base_url": "http://127.0.0.1",
        "serial_commands": {name: ["OK dry-run"] for name in ["PING", "STATUS", "CONFIG?", "DISARM"]},
        "raw": [],
    }
    if ctx:
        ctx.board = board
        ctx.event("serial", "serial discovery", "pass", "dry-run board", board["base_url"])
    return board


def verify_dashboard_assets(base_url: str, timeout: float = 2.0) -> dict[str, Any]:
    client = AntCoreClient(base_url, timeout=timeout)
    status = client.request("GET", "/api/status", auth=False)
    assets: dict[str, bool] = {}
    for path in ["/", "/app.js", "/app.css"]:
        url = base_url.rstrip("/") + path
        try:
            with urllib.request.urlopen(url, timeout=timeout) as response:
                assets[path] = 200 <= response.status < 300
        except Exception:
            assets[path] = False
    body = status.body if isinstance(status.body, dict) else {}
    return {
        "base_url": base_url,
        "status_ok": status.ok,
        "schema": body.get("schema"),
        "firmwareVersion": body.get("firmwareVersion"),
        "filesystemVersion": body.get("filesystemVersion"),
        "asset_ok": all(assets.values()),
        "assets": assets,
        "error": status.error,
    }


def local_ipv4_prefix() -> str:
    candidates: list[str] = []
    try:
        with socket.socket(socket.AF_INET, socket.SOCK_DGRAM) as sock:
            sock.settimeout(0.2)
            sock.connect(("8.8.8.8", 80))
            candidates.append(sock.getsockname()[0])
    except Exception:
        pass
    try:
        for info in socket.getaddrinfo(socket.gethostname(), None, socket.AF_INET):
            ip = info[4][0]
            if not ip.startswith("127."):
                candidates.append(ip)
    except Exception:
        pass
    for ip in candidates:
        parts = ip.split(".")
        if len(parts) == 4:
            return ".".join(parts[:3])
    return "192.168.0"


def scan_dashboard_lan(prefix: str = "", timeout: float = 0.45, workers: int = 64) -> list[dict[str, Any]]:
    prefix = prefix or local_ipv4_prefix()

    def probe(last_octet: int) -> dict[str, Any] | None:
        base_url = f"http://{prefix}.{last_octet}"
        result = verify_dashboard_assets(base_url, timeout=timeout)
        if result.get("status_ok") and result.get("schema") is not None:
            return result
        return None

    found: list[dict[str, Any]] = []
    with concurrent.futures.ThreadPoolExecutor(max_workers=workers) as executor:
        for result in executor.map(probe, range(1, 255)):
            if result:
                found.append(result)
    return found


def boot_mode_from_text(text: str) -> str:
    for line in text.splitlines():
        if "boot:0x" not in line:
            continue
        start = line.find("(", line.find("boot:0x"))
        end = line.rfind(")")
        if start >= 0 and end > start:
            return line[start + 1 : end]
    return ""


def probe_serial_boot_mode(port: str, baud: int = BAUD, read_seconds: float = 2.0) -> dict[str, Any]:
    try:
        import serial  # type: ignore
    except ImportError as exc:  # pragma: no cover
        return {"ok": False, "error": f"pyserial unavailable: {exc}", "raw": "", "boot_mode": ""}
    raw = b""
    try:
        with serial.Serial(port, baud, timeout=0.15, write_timeout=0.5) as ser:
            ser.dtr = False
            ser.rts = True
            time.sleep(0.15)
            ser.rts = False
            time.sleep(0.25)
            deadline = time.time() + read_seconds
            while time.time() < deadline:
                data = ser.read(512)
                if data:
                    raw += data
    except Exception as exc:
        return {"ok": False, "error": str(exc), "raw": raw.decode("utf-8", "replace"), "boot_mode": ""}
    text = raw.decode("utf-8", "replace")
    return {
        "ok": bool(text),
        "error": "",
        "raw": text,
        "boot_mode": boot_mode_from_text(text),
        "download_mode": "DOWNLOAD" in text,
    }


def prompt_live_confirmation(args: argparse.Namespace) -> None:
    if args.dry_run:
        return
    if not args.live_output:
        raise RuntimeError("live movement requires --live-output")
    if args.confirm_live_output:
        return
    print()
    print("Live output motion is about to run. Confirm the rig is suspended, clear, and powered safely.")
    typed = input(f"Type {LIVE_CONFIRMATION!r} to continue: ").strip()
    if typed != LIVE_CONFIRMATION:
        raise RuntimeError("live output confirmation was not entered")


def build_motor_sweep(motion: str, custom: str = "") -> list[float]:
    if custom:
        powers = [max(-1.0, min(1.0, float(part.strip()))) for part in custom.split(",") if part.strip()]
    elif motion == "safe":
        powers = [-0.35, 0.35]
    else:
        powers = list(DEFAULT_MOTOR_POWERS)
    return powers


def build_servo_sweep(config: dict[str, Any], custom: str = "") -> list[int]:
    if custom:
        return [max(500, min(2500, int(part.strip()))) for part in custom.split(",") if part.strip()]
    servo = {}
    for item in config.get("servos", []):
        if int(item.get("index", 0)) == 1:
            servo = item
            break
    return [
        int(servo.get("minUs", DEFAULT_SERVO_US[0])),
        int(servo.get("neutralUs", DEFAULT_SERVO_US[1])),
        int(servo.get("maxUs", DEFAULT_SERVO_US[2])),
        int(servo.get("neutralUs", DEFAULT_SERVO_US[3])),
    ]


def status_arm_block(status: dict[str, Any]) -> str:
    safety = status.get("safety", {})
    if safety.get("armBlockReason"):
        return str(safety.get("armBlockReason"))
    battery = status.get("battery", {})
    if battery.get("critical"):
        return "battery critical"
    if safety.get("pitMode"):
        return "pit mode active"
    return ""


def enable_live_outputs(client: AntCoreClient, ctx: RunContext) -> bool:
    status = client.get_status()
    blocker = status_arm_block(status)
    pack = status.get("battery", {}).get("packVolts")
    if blocker:
        ctx.event("safety", "arm pre-check", "fail", "can arm", blocker, "firmware blocked arming")
        return False
    if pack is not None and float(pack) < 7.0:
        ctx.event("safety", "bench supply check", "inconclusive", "2S supply around 7.4 V", f"{pack} V")

    login = client.login()
    ctx.event("api", "admin login", "pass" if login.ok else "fail", "valid admin session", str(login.body), login.error)
    if not login.ok:
        return False

    client.request("POST", "/api/disarm")
    live = client.request("POST", "/api/test/live-output", {"enabled": True})
    ctx.event("api", "enable live output test gate", "pass" if live.ok else "fail", "liveOutputEnabled true", str(live.body), live.error)
    if not live.ok:
        return False
    arm = client.request("POST", "/api/arm")
    reason = ""
    if isinstance(arm.body, dict):
        reason = str(arm.body.get("reason") or arm.body.get("armBlockReason") or "")
    ctx.event("api", "arm robot for rig pulse", "pass" if arm.ok else "fail", "armed true", str(arm.body), reason or arm.error)
    return arm.ok


def disable_live_outputs(client: AntCoreClient, ctx: RunContext) -> None:
    client.request("POST", "/api/disarm")
    client.request("POST", "/api/test/live-output", {"enabled": False})
    client.request("POST", "/api/control/release")
    client.request("POST", "/api/weapon/disarm")
    ctx.event("safety", "shutdown", "pass", "disarm and release driver lock", "complete")


def sample_detection(camera: CameraBase, calibration: VisionCalibration, actuator: str, samples: int = 4) -> tuple[np.ndarray, Detection]:
    frame = camera.read_rgb()
    roi = calibration.rois[actuator]
    detections = [detect_orange_tape(frame, roi, calibration.hsv)]
    for _ in range(max(0, samples - 1)):
        time.sleep(0.03)
        frame = camera.read_rgb()
        detections.append(detect_orange_tape(frame, roi, calibration.hsv))
    best = max(detections, key=lambda item: item.confidence)
    return frame, best


def record_frames(camera: CameraBase, seconds: float, fps: int, caption: str, ctx: RunContext) -> np.ndarray:
    deadline = time.time() + seconds
    last = camera.read_rgb()
    interval = 1.0 / max(1, fps)
    while time.time() < deadline:
        last = camera.read_rgb()
        ctx.validation_frames.append(overlay_text(Image.fromarray(last), [caption]))
        time.sleep(interval)
    return last


def run_api_motion_tests(
    client: AntCoreClient,
    camera: CameraBase,
    calibration: VisionCalibration,
    config: dict[str, Any],
    args: argparse.Namespace,
    ctx: RunContext,
) -> None:
    motor_powers = build_motor_sweep(args.motion, args.motor_powers)
    servo_us = build_servo_sweep(config, args.servo_positions)
    for motor in [1, 2]:
        actuator = f"motor{motor}"
        if actuator not in calibration.rois:
            ctx.event("physical", f"motor {motor}", "skipped", "ROI available", "missing ROI")
            continue
        client.request("POST", "/api/disarm")
        arm = client.request("POST", "/api/arm")
        if not arm.ok:
            ctx.event("physical", f"motor {motor}", "fail", "armed", str(arm.body), "arm failed before pulse")
            continue
        for power in motor_powers:
            before_frame, before = sample_detection(camera, calibration, actuator)
            if isinstance(camera, FakeCamera):
                camera.apply_command(actuator, power)
            command = client.request(
                "POST",
                "/api/test/motor",
                {"motor": motor, "power": power, "durationMs": args.motor_duration_ms},
            )
            last = record_frames(
                camera,
                max(0.05, args.motor_duration_ms / 1000.0 + args.rest_ms / 1000.0),
                args.video_fps,
                f"Motor {motor} API pulse {power:+.2f}",
                ctx,
            )
            after = detect_orange_tape(last, calibration.rois[actuator], calibration.hsv)
            if command.ok:
                status, observed = classify_motion(before, after, expected_sign=1 if power > 0 else -1)
            else:
                status, observed = "fail", str(command.body)
            still = ""
            if status != "pass":
                still_path = ctx.out_dir / "stills" / f"api_motor{motor}_{power:+.2f}.png".replace("+", "p").replace("-", "m")
                annotate_detection(last, calibration.rois[actuator], after, f"Motor {motor} API {power:+.2f}", status).save(still_path)
                still = str(still_path)
            ctx.validation_frames.append(annotate_detection(last, calibration.rois[actuator], after, f"Motor {motor} API {power:+.2f}", status))
            ctx.event(
                "physical",
                f"api motor {motor} {power:+.2f}",
                status,
                "orange tape motion and direction",
                observed,
                "dashboard live-output motor endpoint" if command.ok else command.error,
                still,
            )
        client.request("POST", "/api/disarm")
        time.sleep(args.rest_ms / 1000.0)

    if "servo1" not in calibration.rois:
        ctx.event("physical", "servo 1", "skipped", "ROI available", "missing ROI")
        return
    client.request("POST", "/api/disarm")
    arm = client.request("POST", "/api/arm")
    if not arm.ok:
        ctx.event("physical", "servo 1", "fail", "armed", str(arm.body), "arm failed before servo pulse")
        return
    for target_us in servo_us:
        before_frame, before = sample_detection(camera, calibration, "servo1")
        if isinstance(camera, FakeCamera):
            camera.apply_command("servo1", float(target_us))
        command = client.request(
            "POST",
            "/api/test/servo",
            {"servo": 1, "us": target_us, "durationMs": args.servo_duration_ms},
        )
        last = record_frames(
            camera,
            max(0.05, args.servo_duration_ms / 1000.0 + args.rest_ms / 1000.0),
            args.video_fps,
            f"Servo 1 API target {target_us} us",
            ctx,
        )
        after = detect_orange_tape(last, calibration.rois["servo1"], calibration.hsv)
        status, observed = classify_motion(before, after) if command.ok else ("fail", str(command.body))
        still = ""
        if status != "pass":
            still_path = ctx.out_dir / "stills" / f"api_servo1_{target_us}.png"
            annotate_detection(last, calibration.rois["servo1"], after, f"Servo 1 API {target_us} us", status).save(still_path)
            still = str(still_path)
        ctx.validation_frames.append(annotate_detection(last, calibration.rois["servo1"], after, f"Servo 1 API {target_us} us", status))
        ctx.event("physical", f"api servo 1 {target_us} us", status, "orange tape motion", observed, "dashboard live-output servo endpoint", still)
    client.request("POST", "/api/disarm")


def websocket_url(base_url: str, token: str) -> str:
    parsed = urllib.parse.urlparse(base_url)
    scheme = "wss" if parsed.scheme == "https" else "ws"
    host = parsed.netloc or parsed.path
    return f"{scheme}://{host}/ws"


def run_websocket_control_check(
    client: AntCoreClient,
    camera: CameraBase,
    calibration: VisionCalibration,
    args: argparse.Namespace,
    ctx: RunContext,
) -> None:
    if not args.live_output:
        ctx.event("physical", "websocket virtual controls", "skipped", "live output flag", "--live-output not set")
        return
    if args.dry_run:
        before1_frame, before1 = sample_detection(camera, calibration, "motor1")
        before2_frame, before2 = sample_detection(camera, calibration, "motor2")
        if isinstance(camera, FakeCamera):
            camera.apply_command("motor1", 0.45)
            camera.apply_command("motor2", 0.45)
        last = record_frames(camera, 0.35, args.video_fps, "Dry-run WebSocket FPV pad forward frame", ctx)
        after1 = detect_orange_tape(last, calibration.rois["motor1"], calibration.hsv)
        after2 = detect_orange_tape(last, calibration.rois["motor2"], calibration.hsv)
        status1, obs1 = classify_motion(before1, after1)
        status2, obs2 = classify_motion(before2, after2)
        status = "pass" if status1 == "pass" and status2 == "pass" else "inconclusive" if "pass" in (status1, status2) else "fail"
        ctx.validation_frames.append(annotate_detection(last, calibration.rois["motor1"], after1, "Dry-run WebSocket motor 1", status1))
        ctx.validation_frames.append(annotate_detection(last, calibration.rois["motor2"], after2, "Dry-run WebSocket motor 2", status2))
        ctx.event("physical", "websocket virtual drive", status, "both visible drive motors move", f"motor1 {obs1}; motor2 {obs2}", "dry-run claimControl/control/releaseControl")
        return
    try:
        import websocket  # type: ignore
    except ImportError:
        ctx.event("physical", "websocket virtual controls", "skipped", "websocket-client installed", "missing dependency")
        return
    if not client.token:
        client.login()
    if not client.token:
        ctx.event("physical", "websocket virtual controls", "skipped", "admin token", "no token")
        return
    if "motor1" not in calibration.rois or "motor2" not in calibration.rois:
        ctx.event("physical", "websocket virtual controls", "skipped", "motor ROIs", "missing motor ROI")
        return
    client_id = f"rig-{int(time.time())}"
    ws = None
    try:
        ws = websocket.create_connection(websocket_url(client.base_url, client.token), timeout=3)
        claim = {"type": "claimControl", "token": client.token, "clientId": client_id}
        ws.send(json.dumps(claim))
        client.request("POST", "/api/disarm")
        arm = client.request("POST", "/api/arm")
        if not arm.ok:
            ctx.event("physical", "websocket virtual controls", "fail", "armed", str(arm.body), "arm failed before websocket frame")
            return
        before1_frame, before1 = sample_detection(camera, calibration, "motor1")
        before2_frame, before2 = sample_detection(camera, calibration, "motor2")
        if isinstance(camera, FakeCamera):
            camera.apply_command("motor1", 0.45)
            camera.apply_command("motor2", 0.45)
        ws.send(
            json.dumps(
                {
                    "type": "control",
                    "token": client.token,
                    "clientId": client_id,
                    "leftX": 0,
                    "leftY": -0.55,
                    "rightX": 0,
                    "rightY": 0,
                    "leftTrigger": 0,
                    "rightTrigger": 0,
                    "buttons": {},
                }
            )
        )
        last = record_frames(camera, 0.35, args.video_fps, "WebSocket FPV pad forward frame", ctx)
        ws.send(json.dumps({"type": "control", "token": client.token, "clientId": client_id, "leftY": 0, "buttons": {}}))
        after1 = detect_orange_tape(last, calibration.rois["motor1"], calibration.hsv)
        after2 = detect_orange_tape(last, calibration.rois["motor2"], calibration.hsv)
        status1, obs1 = classify_motion(before1, after1)
        status2, obs2 = classify_motion(before2, after2)
        status = "pass" if status1 == "pass" and status2 == "pass" else "inconclusive" if "pass" in (status1, status2) else "fail"
        ctx.validation_frames.append(annotate_detection(last, calibration.rois["motor1"], after1, "WebSocket virtual drive motor 1", status1))
        ctx.validation_frames.append(annotate_detection(last, calibration.rois["motor2"], after2, "WebSocket virtual drive motor 2", status2))
        ctx.event("physical", "websocket virtual drive", status, "both visible drive motors move", f"motor1 {obs1}; motor2 {obs2}", "claimControl/control/releaseControl")
        ws.send(json.dumps({"type": "releaseControl", "token": client.token, "clientId": client_id}))
    except Exception as exc:
        ctx.event("physical", "websocket virtual controls", "fail", "control frame accepted", "", str(exc))
    finally:
        if ws is not None:
            try:
                ws.close()
            except Exception:
                pass
        client.request("POST", "/api/disarm")
        client.request("POST", "/api/control/release")


def run_dashboard_checks(client: AntCoreClient, args: argparse.Namespace, ctx: RunContext) -> None:
    status = client.get_status()
    config = client.get_config()
    ctx.event("api", "status endpoint", "pass" if status else "fail", "/api/status JSON", str(status.get("schema", "")))
    ctx.event("api", "config endpoint", "pass" if config else "fail", "/api/config JSON", str(config.get("schema", "")))
    for path, name in [
        ("/api/ble/scan", "BLE scan endpoint"),
        ("/api/disarm", "emergency disarm endpoint"),
        ("/api/control/release", "driver release endpoint"),
    ]:
        response = client.request("POST", path)
        ctx.event("api", name, "pass" if response.ok else "inconclusive", path, str(response.body), response.error)
    for name in [
        "Dashboard status/banners/output visualizers",
        "Controller pairing and live inspector",
        "Control overlay trainer",
        "Drive presets and direction wizard",
        "Servo mapping and presets",
        "FPV page virtual gamepad claim/release",
        "Fight Mode, pre-fight checklist, disarm, debrief",
        "Garage/avatar profiles",
        "Battery pack notes",
        "Spectator read-only page",
        "OTA version/route availability only",
    ]:
        ctx.event("dashboard", name, "pass" if args.dry_run or args.skip_browser else "inconclusive", "page reachable", "covered by tutorial/browser snapshot")
    if args.skip_browser:
        ctx.skipped.append("browser dashboard walkthrough skipped by --skip-browser")


def capture_dashboard_frames(client: AntCoreClient, args: argparse.Namespace, ctx: RunContext) -> None:
    captions = [
        ("dashboard", "Dashboard: arm state, battery, controller source, safety banners, and output visualizers."),
        ("controller", "Controller: pair Xbox pads, inspect live buttons, and learn control mappings."),
        ("drive", "Drive: choose presets, tune arcade/skid steer mixing, and use the direction wizard."),
        ("servos", "Servos: set safe travel limits, failsafe pulse widths, and button/axis actions."),
        ("fpv", "FPV: drive from the gameplay layout with the video in the middle and controls at the sides."),
        ("fight", "Fight Mode: full-screen combat view, pre-fight checklist, timer, and huge disarm."),
        ("garage", "Robot Garage: choose avatar, accent, bot type, weapon type, notes, and profile identity."),
        ("packs", "Battery Packs: track pack names, charge voltage, sag, notes, and weak-pack flags."),
        ("spectator", "Spectator: read-only pit display with camera, timer, battery, and status."),
        ("ota", "OTA: check versions and update firmware/filesystem only when the robot is safe."),
        ("diagnostics", "Diagnostics: serial logs, blackbox export, and gated live output checks."),
    ]
    if args.dry_run or args.skip_browser:
        for tab, caption in captions:
            ctx.tutorial_frames.append(make_tutorial_placeholder(tab, caption, client.get_status()))
        ctx.event("video", "dashboard tutorial frames", "pass", "silent captioned frames", f"{len(ctx.tutorial_frames)} generated")
        return
    try:
        from antcore_ui_audit import Browser, chrome_path
    except Exception as exc:
        ctx.event("video", "dashboard tutorial frames", "inconclusive", "Chrome DevTools available", "", str(exc))
        for tab, caption in captions:
            ctx.tutorial_frames.append(make_tutorial_placeholder(tab, caption, client.get_status()))
        return

    browser = Browser(chrome_path(), args.devtools_port)
    try:
        browser.cdp("Page.enable")
        browser.cdp("Runtime.enable")
        browser.cdp("Page.navigate", {"url": client.base_url + "/"})
        time.sleep(2.0)
        if client.admin_pin:
            browser.eval(
                f"""(async () => {{
                  localStorage.setItem('antcoreToken', {json.dumps(client.token)});
                  window.prompt = () => {json.dumps(client.admin_pin)};
                  if (document.getElementById('loginPin')) document.getElementById('loginPin').value = {json.dumps(client.admin_pin)};
                }})()""",
                await_promise=True,
            )
        for tab, caption in captions:
            browser.eval(
                f"""(() => {{
                  const button = document.querySelector('[data-tab="{tab}"]');
                  if (button) button.click();
                  const spectator = {json.dumps(tab == "spectator")};
                  document.body.classList.toggle('spectator-mode', spectator);
                }})()"""
            )
            time.sleep(0.7)
            png_b64 = browser.cdp("Page.captureScreenshot", {"format": "png", "captureBeyondViewport": False, "fromSurface": True})["data"]
            frame = Image.open(io.BytesIO(base64.b64decode(png_b64))).convert("RGB")
            ctx.tutorial_frames.append(overlay_text(frame, [caption]))
        ctx.event("video", "dashboard tutorial frames", "pass", "live dashboard screenshots", f"{len(ctx.tutorial_frames)} captured")
    except Exception as exc:
        ctx.event("video", "dashboard tutorial frames", "inconclusive", "live dashboard screenshots", "", str(exc))
    finally:
        browser.close()


def make_tutorial_placeholder(tab: str, caption: str, status: dict[str, Any]) -> Image.Image:
    image = Image.new("RGB", FRAME_SIZE, (10, 12, 15))
    draw = ImageDraw.Draw(image, "RGBA")
    font_big = load_font(54)
    font = load_font(24)
    accent = (255, 202, 79)
    draw.rectangle((0, 0, FRAME_SIZE[0], 82), fill=(18, 18, 13, 255))
    draw.rectangle((0, 80, FRAME_SIZE[0], 86), fill=accent + (255,))
    draw.text((34, 18), f"ANT CORE / {tab.upper()}", fill=(239, 245, 244), font=font_big)
    draw.text((42, 145), caption, fill=(239, 245, 244), font=font)
    battery = status.get("battery", {})
    safety = status.get("safety", {})
    cards = [
        ("ARM", "ARMED" if status.get("armed") else "DISARMED"),
        ("BATTERY", f"{battery.get('packVolts', 0):.2f} V" if isinstance(battery.get("packVolts"), (int, float)) else "unknown"),
        ("SAFETY", safety.get("armBlockReason") or "clear"),
        ("SOURCE", status.get("controller", {}).get("source", "none")),
    ]
    x = 42
    for label, value in cards:
        draw.rectangle((x, 250, x + 270, 390), fill=(24, 29, 34, 255), outline=(79, 91, 104, 255), width=2)
        draw.text((x + 18, 276), label, fill=accent, font=font)
        draw.text((x + 18, 325), str(value), fill=(239, 245, 244), font=font)
        x += 300
    return overlay_text(image, [caption])


def write_video(frames: list[Image.Image], path: Path, fps: int = 10) -> str:
    if not frames:
        return ""
    path.parent.mkdir(parents=True, exist_ok=True)
    target_size = frames[0].size
    arrays = [np.array(frame.convert("RGB").resize(target_size)) for frame in frames]
    try:
        import imageio.v2 as imageio  # type: ignore

        imageio.mimsave(path, arrays, fps=fps, macro_block_size=16)
        return str(path)
    except Exception:
        frames_dir = path.with_suffix("")
        frames_dir.mkdir(exist_ok=True)
        for index, frame in enumerate(frames):
            frame.convert("RGB").resize(target_size).save(frames_dir / f"frame_{index:04d}.png")
        return str(frames_dir)


def write_combined_video(ctx: RunContext, fps: int) -> str:
    if not ctx.validation_frames and not ctx.tutorial_frames:
        return ""
    spacer = [overlay_text(Image.new("RGB", FRAME_SIZE, (9, 10, 12)), ["Ant Core rig validation complete. Dashboard walkthrough follows."])] * fps
    frames = ctx.validation_frames + spacer + ctx.tutorial_frames
    return write_video(frames, ctx.out_dir / "antcore_test_and_walkthrough.mp4", fps=fps)


def placeholder_calibration_from_args(args: argparse.Namespace) -> VisionCalibration:
    return VisionCalibration(
        camera_index=str(getattr(args, "camera_index", "0")),
        resolution=(int(getattr(args, "camera_width", FRAME_SIZE[0])), int(getattr(args, "camera_height", FRAME_SIZE[1]))),
        hsv=HsvRange(tuple(getattr(args, "hsv_lower", DEFAULT_HSV_LOWER)), tuple(getattr(args, "hsv_upper", DEFAULT_HSV_UPPER))),  # type: ignore[arg-type]
        rois={},
        baselines={},
    )


def make_blocked_placeholder(title: str, lines: Iterable[str]) -> Image.Image:
    image = Image.new("RGB", FRAME_SIZE, (9, 10, 12))
    draw = ImageDraw.Draw(image, "RGBA")
    font_big = load_font(48)
    font = load_font(24)
    accent = (255, 202, 79)
    draw.rectangle((0, 0, FRAME_SIZE[0], 86), fill=(18, 18, 13, 255))
    draw.rectangle((0, 82, FRAME_SIZE[0], 88), fill=accent + (255,))
    draw.text((34, 20), "ANT CORE RIG TEST", fill=(239, 245, 244), font=font_big)
    draw.rectangle((42, 150, FRAME_SIZE[0] - 42, 500), fill=(20, 25, 31, 255), outline=(79, 91, 104, 255), width=2)
    draw.text((70, 182), title, fill=accent, font=font_big)
    y = 262
    for line in list(lines)[:7]:
        draw.text((74, y), str(line), fill=(239, 245, 244), font=font)
        y += 42
    return image


def write_reports(ctx: RunContext, calibration: VisionCalibration, videos: dict[str, str]) -> None:
    def artifact_link(path_text: str) -> str:
        if not path_text:
            return ""
        path = Path(path_text)
        try:
            return str(path.resolve().relative_to(ctx.out_dir))
        except Exception:
            return str(path_text)

    results = {
        "generated_at": datetime.now().isoformat(timespec="seconds"),
        "board": ctx.board,
        "camera": ctx.camera,
        "calibration": calibration_to_json(calibration),
        "events": [asdict(event) for event in ctx.events],
        "skipped": ctx.skipped,
        "videos": videos,
    }
    (ctx.out_dir / "results.json").write_text(json.dumps(results, indent=2), encoding="utf-8")

    with (ctx.out_dir / "events.csv").open("w", newline="", encoding="utf-8") as handle:
        writer = csv.DictWriter(handle, fieldnames=list(asdict(ctx.events[0]).keys()) if ctx.events else list(RigEvent("", "", "", "").__dict__.keys()))
        writer.writeheader()
        for event in ctx.events:
            writer.writerow(asdict(event))

    counts = {status: sum(1 for event in ctx.events if event.status == status) for status in ["pass", "fail", "inconclusive", "skipped"]}
    skipped_count = counts["skipped"] + len(ctx.skipped)
    lines = [
        "# Ant Core Rig Vision Test Report",
        "",
        f"Generated: {results['generated_at']}",
        f"Output directory: `{ctx.out_dir}`",
        "",
        "## Summary",
        "",
        f"- Pass: {counts['pass']}",
        f"- Fail: {counts['fail']}",
        f"- Inconclusive: {counts['inconclusive']}",
        f"- Skipped: {skipped_count}",
        f"- Board URL: {ctx.board.get('base_url', 'unknown')}",
        f"- Serial port: {ctx.board.get('port', 'unknown')}",
        f"- Firmware: {ctx.board.get('status', {}).get('firmwareVersion', 'unknown')}",
        f"- Filesystem: {ctx.board.get('status', {}).get('filesystemVersion', 'unknown')}",
        "",
        "## Camera Calibration",
        "",
        f"- Camera: {calibration.camera_index}",
        f"- Resolution: {calibration.resolution[0]} x {calibration.resolution[1]}",
        f"- HSV lower/upper: {calibration.hsv.lower} / {calibration.hsv.upper}",
        f"- ROIs: {', '.join(f'{name}=({roi.x},{roi.y},{roi.w},{roi.h})' for name, roi in calibration.rois.items())}",
        "",
        "## Results",
        "",
        "| Category | Check | Status | Expected | Observed | Details | Artifact |",
        "|---|---|---:|---|---|---|---|",
    ]
    for event in ctx.events:
        artifact = artifact_link(event.artifact)
        lines.append(
            f"| {event.category} | {event.name} | {event.status} | {event.expected} | {event.observed} | {event.details} | {artifact} |"
        )
    if ctx.skipped:
        lines.extend(["", "## Skipped", ""])
        lines.extend(f"- {item}" for item in ctx.skipped)
    lines.extend(
        [
            "",
            "## Recommended Improvements",
            "",
            "- Treat any fail or inconclusive physical check as a reason to inspect wiring, motor direction, tape visibility, and dashboard mapping before a bout.",
            "- If direction is inconclusive but motion is detected, improve the orange tape shape so the major-axis angle is easier to track.",
            "- If arming is blocked, fix the firmware-reported armBlockReason instead of bypassing battery or pit-mode safety.",
            "- Re-run calibration whenever the camera or rig position changes.",
        ]
    )
    (ctx.out_dir / "report.md").write_text("\n".join(lines) + "\n", encoding="utf-8")

    rows = "\n".join(
        f"<tr class='{event.status}'><td>{event.category}</td><td>{event.name}</td><td>{event.status}</td><td>{event.expected}</td><td>{event.observed}</td><td>{event.details}</td><td>{artifact_link(event.artifact)}</td></tr>"
        for event in ctx.events
    )
    html = f"""<!doctype html>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ant Core Rig Vision Test Report</title>
<style>
body{{margin:0;background:#0b0d10;color:#edf2f4;font-family:Segoe UI,Arial,sans-serif}}
header{{padding:24px 32px;background:#15130c;border-bottom:4px solid #ffca4f}}
main{{padding:24px 32px;display:grid;gap:20px}}
.cards{{display:grid;grid-template-columns:repeat(auto-fit,minmax(180px,1fr));gap:12px}}
.card{{border:1px solid #33404c;background:#14191f;border-radius:8px;padding:14px}}
table{{border-collapse:collapse;width:100%;background:#11161b}}
td,th{{border:1px solid #33404c;padding:8px;vertical-align:top}}th{{text-align:left;color:#ffca4f}}
.pass td:nth-child(3){{color:#51d584}}.fail td:nth-child(3){{color:#ff5d5d}}.inconclusive td:nth-child(3){{color:#ffca4f}}.skipped td:nth-child(3){{color:#9aa6b2}}
a{{color:#8fc7ff}}
</style>
<header><h1>Ant Core Rig Vision Test Report</h1><p>{results['generated_at']}</p></header>
<main>
<section class="cards">
<div class="card"><b>Pass</b><br>{counts['pass']}</div>
<div class="card"><b>Fail</b><br>{counts['fail']}</div>
<div class="card"><b>Inconclusive</b><br>{counts['inconclusive']}</div>
<div class="card"><b>Skipped</b><br>{skipped_count}</div>
<div class="card"><b>Board</b><br>{ctx.board.get('base_url','unknown')}</div>
<div class="card"><b>Camera</b><br>{calibration.camera_index}</div>
</section>
<section><h2>Videos</h2><ul>
<li>{videos.get('rig_validation','')}</li>
<li>{videos.get('dashboard_tutorial','')}</li>
<li>{videos.get('combined','')}</li>
</ul></section>
<section><h2>Results</h2><table><thead><tr><th>Category</th><th>Check</th><th>Status</th><th>Expected</th><th>Observed</th><th>Details</th><th>Artifact</th></tr></thead><tbody>{rows}</tbody></table></section>
<section><h2>Recommendations</h2><p>Failures should be fixed at the wiring, mapping, calibration, or firmware safety layer. Do not bypass battery or pit-mode safety to make a test pass.</p></section>
</main>
"""
    (ctx.out_dir / "report.html").write_text(html, encoding="utf-8")


def write_blocked_reports(
    ctx: RunContext,
    args: argparse.Namespace,
    calibration: VisionCalibration,
    title: str,
    details: str,
) -> None:
    if not any(event.name == title for event in ctx.events):
        ctx.event("blocked", title, "fail", "hardware, dashboard, and camera reachable", "blocked", details)
    for item in [
        details,
        "No live motor or servo movement was attempted.",
        "Battery safety and pit-mode safety were not bypassed.",
    ]:
        if item not in ctx.skipped:
            ctx.skipped.append(item)
    if not ctx.validation_frames:
        ctx.validation_frames.append(
            make_blocked_placeholder(
                "Hardware validation blocked",
                [
                    details,
                    "No live motor or servo movement was attempted.",
                    "Battery safety and pit-mode safety were not bypassed.",
                    "Fix the blocker, then rerun the hardware validation command.",
                ],
            )
        )
    if not ctx.tutorial_frames:
        ctx.tutorial_frames.append(
            make_blocked_placeholder(
                "Dashboard tutorial blocked",
                [
                    "The live dashboard walkthrough could not be captured.",
                    "A dry-run tutorial can still be generated without hardware.",
                    "The report lists the exact blocker and skipped checks.",
                ],
            )
        )
    fps = int(getattr(args, "video_fps", 10))
    videos = {
        "rig_validation": write_video(ctx.validation_frames, ctx.out_dir / "rig_validation.mp4", fps=fps),
        "dashboard_tutorial": write_video(ctx.tutorial_frames, ctx.out_dir / "dashboard_tutorial.mp4", fps=fps),
    }
    if getattr(args, "combine_video", False):
        videos["combined"] = write_combined_video(ctx, fps)
    write_reports(ctx, calibration, videos)
    print(f"Blocked report written to {ctx.out_dir / 'report.html'}")


def add_discovery_to_context(ctx: RunContext, args: argparse.Namespace) -> None:
    if args.dry_run:
        discover_dry_run(ctx)
        return
    try:
        board = discover_serial(args.port, args.baud, ctx)
    except Exception as exc:
        ctx.board = {
            "port": getattr(args, "port", None) or "auto",
            "baud": getattr(args, "baud", BAUD),
            "serial_ok": False,
            "base_url": args.base_url or "http://192.168.4.1",
            "error": str(exc),
        }
        ctx.event("serial", "serial discovery", "fail", "PING STATUS CONFIG? DISARM", "", str(exc))
        return
    probe_url = args.base_url or board.get("base_url", "http://192.168.4.1")
    try:
        board["dashboard_probe"] = verify_dashboard_assets(probe_url)
        ctx.event(
            "network",
            "dashboard probe",
            "pass" if board["dashboard_probe"].get("status_ok") else "fail",
            "/api/status plus web assets",
            probe_url,
            board["dashboard_probe"].get("error", ""),
        )
        if args.scan_lan or not board["dashboard_probe"].get("status_ok"):
            board["network_scan"] = scan_dashboard_lan(args.scan_prefix, args.scan_timeout)
            ctx.event(
                "network",
                "LAN dashboard scan",
                "pass" if board["network_scan"] else "fail",
                "Ant Core dashboard on local /24",
                f"{len(board['network_scan'])} found",
            )
    except Exception as exc:
        board["dashboard_probe"] = {"base_url": probe_url, "status_ok": False, "error": str(exc)}
        ctx.event("network", "dashboard probe", "fail", "Ant Core dashboard reachable", probe_url, str(exc))


def run_calibration(args: argparse.Namespace, ctx: RunContext | None = None) -> None:
    if args.camera_index == "auto":
        candidates = enumerate_cameras(backend=args.camera_backend)
        if not candidates:
            raise SystemExit("no USB cameras found")
        print("Camera candidates:", ", ".join(map(str, candidates)))
        if args.auto_roi:
            errors: list[str] = []
            for candidate in candidates:
                camera: UsbCamera | None = None
                artifact = ""
                summary = "not calibrated"
                try:
                    camera = UsbCamera(str(candidate), args.camera_width, args.camera_height, args.camera_backend)
                    frame = camera.read_rgb()
                    summary = camera_frame_summary(frame, HsvRange(tuple(args.hsv_lower), tuple(args.hsv_upper)))  # type: ignore[arg-type]
                    artifact = save_camera_frame_artifact(
                        ctx,
                        frame,
                        f"camera_candidate_{candidate}.png",
                        f"Camera {candidate}: auto ROI calibration candidate frame",
                    )
                    calibration = auto_calibrate_from_frame(
                        frame,
                        str(candidate),
                        HsvRange(tuple(args.hsv_lower), tuple(args.hsv_upper)),  # type: ignore[arg-type]
                        args.roi_order,
                        args.roi_padding,
                        args.min_orange_area,
                    )
                    save_calibration(Path(args.calibration), calibration)
                    preview_path = Path(args.calibration).with_suffix(".preview.png")
                    draw_calibration_preview(frame, calibration).save(preview_path)
                    print(f"Auto ROI selected camera {candidate}")
                    print(f"Saved calibration to {Path(args.calibration).resolve()}")
                    print(f"Saved preview to {preview_path.resolve()}")
                    if ctx:
                        ctx.event("vision", f"camera {candidate} auto ROI", "pass", "three orange tape ROIs", "calibration saved", artifact=str(preview_path))
                    return
                except Exception as exc:
                    errors.append(f"camera {candidate}: {exc}")
                    if ctx:
                        ctx.event("vision", f"camera {candidate} auto ROI", "fail", "three orange tape ROIs", summary, str(exc), artifact)
                finally:
                    if camera is not None:
                        camera.close()
            raise SystemExit("auto ROI calibration failed:\n" + "\n".join(errors))
        camera_index = str(candidates[0])
    else:
        camera_index = str(args.camera_index)
    try:
        import cv2  # type: ignore
    except ImportError as exc:
        raise SystemExit("opencv-python is required for calibration. Run: python -m pip install -r requirements.txt") from exc

    camera = UsbCamera(camera_index, args.camera_width, args.camera_height, args.camera_backend)
    try:
        frame = camera.read_rgb()
        height, width = frame.shape[:2]
        if args.auto_roi:
            summary = camera_frame_summary(frame, HsvRange(tuple(args.hsv_lower), tuple(args.hsv_upper)))  # type: ignore[arg-type]
            artifact = save_camera_frame_artifact(
                ctx,
                frame,
                f"camera_candidate_{camera_index}.png",
                f"Camera {camera_index}: auto ROI calibration candidate frame",
            )
            try:
                calibration = auto_calibrate_from_frame(
                    frame,
                    camera_index,
                    HsvRange(tuple(args.hsv_lower), tuple(args.hsv_upper)),  # type: ignore[arg-type]
                    args.roi_order,
                    args.roi_padding,
                    args.min_orange_area,
                )
            except Exception as exc:
                if ctx:
                    ctx.event("vision", f"camera {camera_index} auto ROI", "fail", "three orange tape ROIs", summary, str(exc), artifact)
                raise
            save_calibration(Path(args.calibration), calibration)
            preview_path = Path(args.calibration).with_suffix(".preview.png")
            draw_calibration_preview(frame, calibration).save(preview_path)
            if ctx:
                ctx.event("vision", f"camera {camera_index} auto ROI", "pass", "three orange tape ROIs", "calibration saved", artifact=str(preview_path))
            print(f"Saved calibration to {Path(args.calibration).resolve()}")
            print(f"Saved preview to {preview_path.resolve()}")
            return
        rois: dict[str, Roi] = {}
        for name in ["motor1", "motor2", "servo1"]:
            print(f"Select ROI for {name}, then press Enter or Space. Press C to cancel.")
            rect = cv2.selectROI(f"Ant Core calibration - {name}", cv2.cvtColor(frame, cv2.COLOR_RGB2BGR), showCrosshair=True)
            cv2.destroyWindow(f"Ant Core calibration - {name}")
            x, y, w, h = [int(value) for value in rect]
            if w <= 0 or h <= 0:
                raise SystemExit(f"calibration cancelled while selecting {name}")
            rois[name] = Roi(x, y, w, h)
        calibration = VisionCalibration(
            camera_index=camera_index,
            resolution=(width, height),
            hsv=HsvRange(tuple(args.hsv_lower), tuple(args.hsv_upper)),  # type: ignore[arg-type]
            rois=rois,
        )
        calibration.baselines = {
            name: detect_orange_tape(frame, roi, calibration.hsv) for name, roi in calibration.rois.items()
        }
        save_calibration(Path(args.calibration), calibration)
        preview_path = Path(args.calibration).with_suffix(".preview.png")
        draw_calibration_preview(frame, calibration).save(preview_path)
        print(f"Saved calibration to {Path(args.calibration).resolve()}")
        print(f"Saved preview to {preview_path.resolve()}")
    finally:
        camera.close()


def open_camera_for_args(args: argparse.Namespace, calibration: VisionCalibration) -> CameraBase:
    if args.dry_run:
        return FakeCamera(calibration)
    camera_index = str(args.camera_index if args.camera_index != "auto" else calibration.camera_index)
    return UsbCamera(camera_index, args.camera_width, args.camera_height, args.camera_backend)


def load_or_fake_calibration(args: argparse.Namespace) -> VisionCalibration:
    path = Path(args.calibration)
    if args.dry_run:
        calibration = fake_calibration()
        frame = orange_line_frame(calibration)
        calibration.baselines = {name: detect_orange_tape(frame, roi, calibration.hsv) for name, roi in calibration.rois.items()}
        return calibration
    if not path.exists():
        raise RuntimeError(f"calibration file missing: {path}. Run calibrate first.")
    return load_calibration(path)


def run_validation(args: argparse.Namespace, tutorial_only: bool = False) -> RunContext:
    ctx = RunContext(ensure_out_dir(args.output_dir))
    calibration = placeholder_calibration_from_args(args)
    try:
        board = discover_dry_run(ctx) if args.dry_run else discover_serial(args.port, args.baud, ctx)
    except Exception as exc:
        ctx.event("serial", "serial discovery", "fail", "PING STATUS CONFIG? DISARM", "", str(exc))
        write_blocked_reports(ctx, args, calibration, "serial discovery blocked", str(exc))
        return ctx
    base_url = args.base_url or board["base_url"]
    if not args.dry_run and args.scan_lan:
        found = scan_dashboard_lan(args.scan_prefix, args.scan_timeout)
        board["network_scan"] = found
        if found and not args.base_url:
            base_url = found[0]["base_url"]
            ctx.event("network", "LAN dashboard scan", "pass", "Ant Core /api/status", base_url)
        elif not found:
            ctx.event("network", "LAN dashboard scan", "fail", "Ant Core /api/status", "no dashboard found")
    client: AntCoreClient = FakeAntCoreClient() if args.dry_run else AntCoreClient(base_url, args.admin_pin or "")
    if not args.dry_run and not args.admin_pin:
        if args.prompt_pin:
            client.admin_pin = getpass.getpass("Ant Core admin PIN: ")
        else:
            client.admin_pin = "antcore"
    login = client.login()
    ctx.event("api", "initial login", "pass" if login.ok else "inconclusive", "admin session", str(login.body), login.error)
    status = client.get_status()
    config = client.get_config()
    ctx.board["live_status"] = status
    ctx.board["live_config_schema"] = config.get("schema")
    run_dashboard_checks(client, args, ctx)
    try:
        calibration = load_or_fake_calibration(args)
    except Exception as exc:
        ctx.event("vision", "camera calibration", "fail", "saved orange-tape ROI calibration", "", str(exc))
        ctx.skipped.append("physical output checks skipped because camera calibration is unavailable")
        capture_dashboard_frames(client, args, ctx)
        write_blocked_reports(ctx, args, calibration, "camera calibration blocked", str(exc))
        return ctx
    ctx.camera = calibration_to_json(calibration)
    camera: CameraBase | None = None
    try:
        if not tutorial_only:
            try:
                camera = open_camera_for_args(args, calibration)
                for actuator in calibration.rois:
                    frame, detection = sample_detection(camera, calibration, actuator, samples=2)
                    status_name = "pass" if detection.found else "fail"
                    ctx.event("vision", f"{actuator} baseline detection", status_name, "orange tape visible", f"confidence {detection.confidence:.2f}, area {detection.area}")
                    ctx.validation_frames.append(annotate_detection(frame, calibration.rois[actuator], detection, f"{actuator} baseline", status_name))
                if args.live_output:
                    prompt_live_confirmation(args)
                    if enable_live_outputs(client, ctx):
                        try:
                            run_api_motion_tests(client, camera, calibration, config, args, ctx)
                            run_websocket_control_check(client, camera, calibration, args, ctx)
                        finally:
                            disable_live_outputs(client, ctx)
                    else:
                        ctx.skipped.append("live physical movement skipped because firmware safety or login blocked arming")
                else:
                    ctx.event("physical", "live motor and servo movement", "skipped", "--live-output", "not requested")
                    ctx.skipped.append("live motor and servo movement requires --live-output plus typed confirmation")
            except Exception as exc:
                ctx.event("vision", "USB camera capture", "fail", "camera opens and orange tape is visible", "", str(exc))
                ctx.skipped.append("physical output checks skipped because the USB camera could not be used")
                ctx.validation_frames.append(
                    make_blocked_placeholder(
                        "Camera capture blocked",
                        [
                            str(exc),
                            "No live motor or servo movement was attempted.",
                            "Check the selected camera index/backend and close other camera apps.",
                        ],
                    )
                )
        capture_dashboard_frames(client, args, ctx)
    finally:
        if camera is not None:
            camera.close()
    videos = {
        "rig_validation": write_video(ctx.validation_frames, ctx.out_dir / "rig_validation.mp4", fps=args.video_fps),
        "dashboard_tutorial": write_video(ctx.tutorial_frames, ctx.out_dir / "dashboard_tutorial.mp4", fps=args.video_fps),
    }
    if args.combine_video:
        videos["combined"] = write_combined_video(ctx, args.video_fps)
    write_reports(ctx, calibration, videos)
    print(f"Report written to {ctx.out_dir / 'report.html'}")
    return ctx


def run_tutorial(args: argparse.Namespace) -> RunContext:
    args.live_output = False
    return run_validation(args, tutorial_only=True)


def command_discover(args: argparse.Namespace) -> None:
    ctx = RunContext(ensure_out_dir(args.output_dir))
    board = discover_dry_run(ctx) if args.dry_run else discover_serial(args.port, args.baud, ctx)
    probe_url = args.base_url or board.get("base_url", "http://192.168.4.1")
    board["dashboard_probe"] = verify_dashboard_assets(probe_url) if not args.dry_run else {
        "base_url": probe_url,
        "status_ok": True,
        "schema": 10,
        "firmwareVersion": "dry-run",
        "filesystemVersion": "dry-run",
        "asset_ok": True,
        "assets": {"/": True, "/app.js": True, "/app.css": True},
        "error": "",
    }
    if not args.dry_run and (args.scan_lan or not board["dashboard_probe"].get("status_ok")):
        board["network_scan"] = scan_dashboard_lan(args.scan_prefix, args.scan_timeout)
    print(json.dumps(board, indent=2))
    if not args.dry_run and not board.get("serial_ok"):
        raise SystemExit(f"no serial responses on {board.get('port')}")


def command_run(args: argparse.Namespace) -> None:
    run_validation(args, tutorial_only=False)


def command_tutorial(args: argparse.Namespace) -> None:
    run_tutorial(args)


def command_all(args: argparse.Namespace) -> None:
    if not args.dry_run and not Path(args.calibration).exists():
        print(f"Calibration file {args.calibration} does not exist; starting calibration.")
        ctx = RunContext(ensure_out_dir(args.output_dir))
        args.output_dir = str(ctx.out_dir)
        try:
            run_calibration(args, ctx)
        except (SystemExit, Exception) as exc:
            calibration = placeholder_calibration_from_args(args)
            details = str(exc)
            ctx.event("vision", "camera calibration", "fail", "orange tape ROIs from USB camera", "", details)
            ctx.skipped.append("validation skipped because auto/manual calibration could not complete")
            add_discovery_to_context(ctx, args)
            write_blocked_reports(ctx, args, calibration, "camera calibration blocked", details)
            return
    run_validation(args, tutorial_only=False)


def add_common_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--port", default=None, help="Serial port for the XIAO ESP32-S3. Defaults to Espressif USB auto-detect.")
    parser.add_argument("--baud", type=int, default=BAUD)
    parser.add_argument("--base-url", default="", help="Dashboard URL override, for example http://antcore.local")
    parser.add_argument("--admin-pin", default="", help="Admin PIN. Defaults to antcore unless --prompt-pin is used.")
    parser.add_argument("--prompt-pin", action="store_true", help="Prompt for the admin PIN instead of using a CLI value.")
    parser.add_argument("--camera-index", default="0", help="USB camera index, path, or 'auto'")
    parser.add_argument("--camera-backend", choices=["dshow", "msmf", "any"], default="dshow", help="OpenCV capture backend on Windows")
    parser.add_argument("--camera-width", type=int, default=1280)
    parser.add_argument("--camera-height", type=int, default=720)
    parser.add_argument("--auto-roi", action="store_true", help="Automatically find orange tape ROIs from the camera frame")
    parser.add_argument("--roi-order", default="motor1,motor2,servo1", help="Comma separated left-to-right auto ROI names")
    parser.add_argument("--roi-padding", type=int, default=70)
    parser.add_argument("--min-orange-area", type=int, default=60)
    parser.add_argument("--hsv-lower", nargs=3, type=int, default=list(DEFAULT_HSV_LOWER), metavar=("H", "S", "V"))
    parser.add_argument("--hsv-upper", nargs=3, type=int, default=list(DEFAULT_HSV_UPPER), metavar=("H", "S", "V"))
    parser.add_argument("--calibration", default=str(DEFAULT_CALIBRATION))
    parser.add_argument("--output-dir", default="")
    parser.add_argument("--dry-run", action="store_true", help="Use fake camera/API data and generate reports without hardware")
    parser.add_argument("--scan-lan", action="store_true", help="Scan the local /24 for an Ant Core dashboard")
    parser.add_argument("--scan-prefix", default="", help="IPv4 prefix to scan, for example 192.168.0")
    parser.add_argument("--scan-timeout", type=float, default=0.45)
    parser.add_argument("--skip-browser", action="store_true", help="Skip live Chrome dashboard capture and use caption frames")
    parser.add_argument("--devtools-port", type=int, default=9360)
    parser.add_argument("--video-fps", type=int, default=10)
    parser.add_argument("--combine-video", action="store_true", help="Also create antcore_test_and_walkthrough.mp4")


def add_motion_args(parser: argparse.ArgumentParser) -> None:
    parser.add_argument("--live-output", action="store_true", help="Required for any motor/servo movement")
    parser.add_argument("--confirm-live-output", action="store_true", help="Skip typed confirmation for scripted lab use")
    parser.add_argument("--motion", choices=["safe", "full"], default="full")
    parser.add_argument("--motor-powers", default="", help="Comma separated motor powers, bounded to -1.0..1.0")
    parser.add_argument("--servo-positions", default="", help="Comma separated servo pulse widths, bounded to 500..2500 us")
    parser.add_argument("--motor-duration-ms", type=int, default=280)
    parser.add_argument("--servo-duration-ms", type=int, default=350)
    parser.add_argument("--rest-ms", type=int, default=220)


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description="Ant Core USB-camera rig tester and silent tutorial recorder")
    sub = parser.add_subparsers(dest="command", required=True)
    discover = sub.add_parser("discover", help="Read serial status/config and infer dashboard URL")
    add_common_args(discover)
    discover.set_defaults(func=command_discover)

    calibrate = sub.add_parser("calibrate", help="Select orange-tape ROIs and save calibration JSON")
    add_common_args(calibrate)
    calibrate.set_defaults(func=lambda args: run_calibration(args))

    run = sub.add_parser("run", help="Run dashboard and physical rig validation")
    add_common_args(run)
    add_motion_args(run)
    run.set_defaults(func=command_run)

    tutorial = sub.add_parser("tutorial", help="Record a silent captioned dashboard walkthrough")
    add_common_args(tutorial)
    tutorial.set_defaults(func=command_tutorial)

    all_cmd = sub.add_parser("all", help="Discover, calibrate if needed, validate, report, and record tutorial")
    add_common_args(all_cmd)
    add_motion_args(all_cmd)
    all_cmd.set_defaults(func=command_all)
    return parser


def main(argv: list[str] | None = None) -> None:
    parser = build_parser()
    args = parser.parse_args(argv)
    try:
        args.func(args)
    except KeyboardInterrupt:
        raise SystemExit("interrupted")
    except Exception as exc:
        raise SystemExit(str(exc)) from exc


if __name__ == "__main__":
    main()
