#!/usr/bin/env python3
"""Serial viewer, command bridge, and smoke tester for Ant Core firmware."""

from __future__ import annotations

import argparse
import json
import queue
import threading
import time
from dataclasses import dataclass, field
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Iterable

try:
    import serial
    from serial.tools import list_ports
except ImportError:  # pragma: no cover
    serial = None
    list_ports = None


BAUD = 115200
ESPRESSIF_VIDS = {0x303A}


def extract_prefixed_json(line: str, prefix: str) -> dict[str, Any] | None:
    if not line.startswith(prefix + " "):
        return None
    return json.loads(line[len(prefix) + 1 :])


def list_serial_port_names() -> list[str]:
    if list_ports is None:
        return []
    ports = sorted(list_ports.comports(), key=lambda item: item.device)
    return [port.device for port in ports]


def choose_port(preferred: str | None = None) -> str:
    if preferred:
        return preferred
    if list_ports is None:
        raise RuntimeError("pyserial port discovery unavailable; pass --port")
    ports = list(list_ports.comports())
    for port in ports:
        if port.vid in ESPRESSIF_VIDS:
            return port.device
    if not ports:
        raise RuntimeError("no serial ports found")
    if len(ports) == 1:
        return ports[0].device
    names = ", ".join(port.device for port in ports)
    raise RuntimeError(f"multiple serial ports found ({names}); pass --port")


@dataclass
class BridgeState:
    status: dict[str, Any] = field(default_factory=dict)
    config: dict[str, Any] = field(default_factory=dict)
    raw: list[str] = field(default_factory=list)
    last_error: str = ""
    connected: bool = False


class SerialBridge:
    def __init__(self, port: str, baud: int = BAUD) -> None:
        if serial is None:
            raise RuntimeError("pyserial is required. Install with: python -m pip install -r requirements.txt")
        self.port = port
        self.baud = baud
        self.state = BridgeState()
        self._serial: serial.Serial | None = None
        self._stop = threading.Event()
        self._reader: threading.Thread | None = None
        self._responses: queue.Queue[str] = queue.Queue()
        self._lock = threading.Lock()

    def connect(self) -> None:
        self._serial = serial.Serial(self.port, self.baud, timeout=0.1, write_timeout=0.5)
        self.state.connected = True
        self._reader = threading.Thread(target=self._read_loop, name="antcore-serial-reader", daemon=True)
        self._reader.start()
        time.sleep(0.4)

    def close(self) -> None:
        self._stop.set()
        with self._lock:
            if self._serial is not None:
                try:
                    self._serial.close()
                finally:
                    self._serial = None
        self.state.connected = False

    def command(self, command: str, wait: float = 0.7) -> list[str]:
        command = command.strip()
        if not command:
            return []
        with self._lock:
            if self._serial is None:
                raise RuntimeError("serial port is not connected")
            self._serial.write((command + "\n").encode("ascii", errors="replace"))
            self._serial.flush()
        deadline = time.time() + wait
        lines: list[str] = []
        while time.time() < deadline:
            try:
                line = self._responses.get(timeout=0.05)
            except queue.Empty:
                continue
            lines.append(line)
            if line.startswith(("OK", "ERR", "PONG", "STATUS ", "CONFIG ")):
                break
        return lines

    def snapshot(self) -> dict[str, Any]:
        return {
            "port": self.port,
            "connected": self.state.connected,
            "status": self.state.status,
            "config": self.state.config,
            "raw": self.state.raw[-120:],
            "last_error": self.state.last_error,
        }

    def _read_loop(self) -> None:
        assert self._serial is not None
        while not self._stop.is_set():
            try:
                raw = self._serial.readline()
            except Exception as exc:  # pragma: no cover
                self.state.last_error = str(exc)
                break
            if not raw:
                continue
            line = raw.decode("utf-8", errors="replace").strip()
            if not line:
                continue
            self.state.raw.append(line)
            self.state.raw = self.state.raw[-300:]
            try:
                if parsed := extract_prefixed_json(line, "STATUS"):
                    self.state.status = parsed
                elif parsed := extract_prefixed_json(line, "CONFIG"):
                    self.state.config = parsed
            except json.JSONDecodeError as exc:
                self.state.last_error = f"invalid JSON from firmware: {exc}"
            self._responses.put(line)


class BridgeHttpHandler(BaseHTTPRequestHandler):
    bridge: SerialBridge

    def do_GET(self) -> None:
        if self.path == "/":
            self._send(HTTPStatus.OK, "text/html", DASHBOARD_HTML)
        elif self.path == "/state":
            self._send(HTTPStatus.OK, "application/json", json.dumps(self.bridge.snapshot()))
        else:
            self._send(HTTPStatus.NOT_FOUND, "text/plain", "not found")

    def do_POST(self) -> None:
        if self.path != "/command":
            self._send(HTTPStatus.NOT_FOUND, "text/plain", "not found")
            return
        length = int(self.headers.get("content-length", "0"))
        body = self.rfile.read(length).decode("utf-8")
        try:
            payload = json.loads(body or "{}")
            command = str(payload.get("command", ""))
            lines = self.bridge.command(command)
            self._send(HTTPStatus.OK, "application/json", json.dumps({"ok": True, "lines": lines}))
        except Exception as exc:
            self._send(HTTPStatus.BAD_REQUEST, "application/json", json.dumps({"ok": False, "error": str(exc)}))

    def log_message(self, fmt: str, *args: Any) -> None:
        return

    def _send(self, status: HTTPStatus, content_type: str, body: str) -> None:
        data = body.encode("utf-8")
        self.send_response(status)
        self.send_header("content-type", content_type)
        self.send_header("content-length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)


DASHBOARD_HTML = """<!doctype html>
<meta name="viewport" content="width=device-width,initial-scale=1">
<title>Ant Core Serial Bridge</title>
<style>
body{margin:0;background:#0b0e11;color:#edf2f4;font-family:system-ui,sans-serif}
header{padding:16px 20px;border-bottom:1px solid #33404c;display:flex;justify-content:space-between;gap:12px;flex-wrap:wrap}
main{padding:20px;display:grid;gap:14px;grid-template-columns:1fr 1fr}.panel{border:1px solid #33404c;background:#151a20;border-radius:8px;padding:14px}
button,input{min-height:38px;border-radius:6px;border:1px solid #33404c;background:#1d242c;color:#eef3f5;padding:7px 10px}pre{white-space:pre-wrap;overflow:auto;max-height:420px}
.danger{border-color:#ff5d5d;color:#ffd7d7}@media(max-width:800px){main{grid-template-columns:1fr}}
</style>
<header><h1>Ant Core Serial Bridge</h1><div><button onclick="cmd('STATUS')">Status</button> <button onclick="cmd('CONFIG?')">Config</button> <button onclick="cmd('ARM')" class="danger">Arm</button> <button onclick="cmd('DISARM')" class="danger">Disarm</button></div></header>
<main><section class="panel"><h2>Command</h2><input id="command" value="PING"><button onclick="cmd(document.getElementById('command').value)">Send</button><pre id="response"></pre></section><section class="panel"><h2>State</h2><pre id="state"></pre></section><section class="panel" style="grid-column:1/-1"><h2>Raw Log</h2><pre id="raw"></pre></section></main>
<script>
async function refresh(){const r=await fetch('/state');const s=await r.json();state.textContent=JSON.stringify(s.status,null,2);raw.textContent=s.raw.join('\\n');}
async function cmd(command){const r=await fetch('/command',{method:'POST',headers:{'content-type':'application/json'},body:JSON.stringify({command})});const j=await r.json();response.textContent=JSON.stringify(j,null,2);refresh();}
setInterval(refresh,1000);refresh();
</script>
"""


def run_smoke(bridge: SerialBridge, live_output: bool) -> None:
    checks = ["PING", "STATUS", "CONFIG?", "BLE_SCAN", "DISARM"]
    for command in checks:
        lines = bridge.command(command, wait=1.5)
        print(f"$ {command}")
        for line in lines:
            print(line)
        if not lines:
            raise RuntimeError(f"no response to {command}")
    if live_output:
        for command in ["LIVE_OUTPUT_ENABLE 1", "ARM", "MOTOR_TEST 1 0.12 120", "SERVO_TEST 1 1500 120", "DISARM", "LIVE_OUTPUT_ENABLE 0"]:
            print(f"$ {command}")
            print("\n".join(bridge.command(command, wait=1.2)))


def serve(bridge: SerialBridge, host: str, port: int) -> None:
    BridgeHttpHandler.bridge = bridge
    server = ThreadingHTTPServer((host, port), BridgeHttpHandler)
    print(f"Serial bridge dashboard: http://{host}:{port}/")
    server.serve_forever()


def main(argv: Iterable[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--port", help="Serial port. Defaults to an Espressif USB device when one is discoverable.")
    parser.add_argument("--baud", type=int, default=BAUD)
    parser.add_argument("--serve", action="store_true", help="Host local HTML dashboard.")
    parser.add_argument("--host", default="127.0.0.1")
    parser.add_argument("--http-port", type=int, default=8765)
    parser.add_argument("--smoke", action="store_true", help="Run safe serial smoke checks.")
    parser.add_argument("--live-output", action="store_true", help="Allow live motor/servo smoke tests.")
    args = parser.parse_args(argv)

    port = choose_port(args.port)
    bridge = SerialBridge(port, args.baud)
    bridge.connect()
    print(f"Connected to {port} at {args.baud}")
    try:
        if args.smoke:
            run_smoke(bridge, args.live_output)
        if args.serve:
            serve(bridge, args.host, args.http_port)
        if not args.smoke and not args.serve:
            print("Type commands. Ctrl+C to exit.")
            while True:
                bridge.command(input("> "), wait=1.0)
    finally:
        bridge.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
