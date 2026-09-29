const axisOptions = ["leftX", "leftY", "rightX", "rightY", "leftTrigger", "rightTrigger"];
const buttonOptions = ["a", "b", "x", "y", "leftBumper", "rightBumper", "leftStickButton", "rightStickButton", "dpadUp", "dpadDown", "dpadLeft", "dpadRight", "menu", "view", "share", "xbox"];
const inputOptions = [...axisOptions, ...buttonOptions];
const buttonOptionPairs = [["", "Unassigned"], ...buttonOptions.map((name) => [name, name])];
const actionOptions = [
  ["none", "None"],
  ["robotArmToggle", "Robot arm toggle"],
  ["robotDisarm", "Robot disarm"],
  ["weaponArmToggle", "Weapon arm toggle"],
  ["weaponSafe", "Weapon safe"],
  ["driveInvertToggle", "Drive invert toggle"],
  ["selfRight", "Self-right macro"],
];
const drivePresets = {
  skid: {
    label: "2WD skid steer",
    mode: "arcade",
    throttleAxis: "leftY",
    turnAxis: "leftX",
    leftTankAxis: "leftY",
    rightTankAxis: "rightY",
    leftMotor: 1,
    rightMotor: 2,
    invertible: true,
    gyroAssist: false,
    autoInvertWithImu: false,
    throttleScale: 1,
    turnScale: 1,
    precisionScale: 0.45,
    turboScale: 1,
  },
  arcade: {
    label: "arcade drive",
    mode: "arcade",
    throttleAxis: "leftY",
    turnAxis: "leftX",
    leftTankAxis: "leftY",
    rightTankAxis: "rightY",
    invertible: false,
    gyroAssist: false,
    autoInvertWithImu: false,
    throttleScale: 1,
    turnScale: 1,
  },
  tank: {
    label: "tank drive",
    mode: "tank",
    throttleAxis: "leftY",
    turnAxis: "leftX",
    leftTankAxis: "leftY",
    rightTankAxis: "rightY",
    leftMotor: 1,
    rightMotor: 2,
    invertible: false,
    gyroAssist: false,
    autoInvertWithImu: false,
    throttleScale: 1,
    turnScale: 1,
  },
  invertible: {
    label: "invertible bot",
    mode: "arcade",
    throttleAxis: "leftY",
    turnAxis: "leftX",
    leftTankAxis: "leftY",
    rightTankAxis: "rightY",
    leftMotor: 1,
    rightMotor: 2,
    invertible: true,
    invertButton: "view",
    gyroAssist: false,
    autoInvertWithImu: true,
    throttleScale: 1,
    turnScale: 1,
  },
  gyro: {
    label: "gyro heading hold",
    mode: "arcade",
    throttleAxis: "leftY",
    turnAxis: "leftX",
    leftTankAxis: "leftY",
    rightTankAxis: "rightY",
    leftMotor: 1,
    rightMotor: 2,
    invertible: true,
    gyroAssist: true,
    gyroGain: 0.015,
    autoInvertWithImu: false,
    throttleScale: 1,
    turnScale: 1,
  },
};
const servoPresets = {
  joystick: {
    label: "joystick proportional",
    axisEnabled: true,
    axis: "rightY",
    detachOnDisarm: false,
    buttons: [],
  },
  trigger: {
    label: "trigger proportional",
    axisEnabled: true,
    axis: "rightTrigger",
    detachOnDisarm: false,
    buttons: [],
  },
  buttons: {
    label: "button positions",
    axisEnabled: false,
    detachOnDisarm: false,
    buttons: [
      { enabled: true, button: "a", us: 1000, toggle: false },
      { enabled: true, button: "y", us: 2000, toggle: false },
    ],
  },
  toggles: {
    label: "toggle positions",
    axisEnabled: false,
    detachOnDisarm: false,
    buttons: [
      { enabled: true, button: "leftBumper", us: 1000, toggle: true },
      { enabled: true, button: "rightBumper", us: 2000, toggle: true },
    ],
  },
  failsafe: {
    label: "failsafe hold",
    axisEnabled: false,
    detachOnDisarm: false,
    buttons: [],
  },
  detach: {
    label: "detach on disarm",
    detachOnDisarm: true,
  },
};
const weaponPresets = {
  disabled: {
    label: "disabled",
    enabled: false,
  },
  brushed: {
    label: "brushed weapon",
    enabled: true,
    profile: "brushed",
    input: "rightTrigger",
    armButton: "x",
    toggle: false,
    buttonPower: 1,
    maxOutput: 1,
    rampUpPerSecond: 3,
    rampDownPerSecond: 8,
    invert: false,
    requireDedicatedArm: true,
  },
  reversible: {
    label: "reversible weapon",
    enabled: true,
    profile: "reversible",
    input: "rightY",
    armButton: "x",
    toggle: false,
    buttonPower: 1,
    maxOutput: 1,
    rampUpPerSecond: 10,
    rampDownPerSecond: 10,
    invert: false,
    requireDedicatedArm: true,
  },
  spinner: {
    label: "spinner",
    enabled: true,
    profile: "spinner",
    input: "rightTrigger",
    armButton: "x",
    toggle: false,
    buttonPower: 1,
    maxOutput: 1,
    rampUpPerSecond: 3,
    rampDownPerSecond: 8,
    invert: false,
    requireDedicatedArm: true,
  },
  lifter: {
    label: "lifter/flipper",
    enabled: true,
    profile: "lifter",
    input: "rightBumper",
    armButton: "x",
    toggle: false,
    buttonPower: 1,
    maxOutput: 1,
    rampUpPerSecond: 20,
    rampDownPerSecond: 20,
    invert: false,
    requireDedicatedArm: true,
  },
  toggle: {
    label: "toggle output",
    enabled: true,
    profile: "brushed",
    input: "rightBumper",
    armButton: "x",
    toggle: true,
    buttonPower: 1,
    maxOutput: 1,
    rampUpPerSecond: 10,
    rampDownPerSecond: 10,
    invert: false,
    requireDedicatedArm: true,
  },
  verticalSpinner: {
    label: "vertical spinner",
    enabled: true,
    profile: "spinner",
    input: "rightTrigger",
    armButton: "x",
    toggle: false,
    buttonPower: 1,
    maxOutput: 0.9,
    rampUpPerSecond: 2.2,
    rampDownPerSecond: 9,
    invert: false,
    requireDedicatedArm: true,
    weaponType: "vertical spinner",
  },
  horizontalSpinner: {
    label: "horizontal spinner",
    enabled: true,
    profile: "spinner",
    input: "rightTrigger",
    armButton: "x",
    toggle: false,
    buttonPower: 1,
    maxOutput: 1,
    rampUpPerSecond: 2.8,
    rampDownPerSecond: 12,
    invert: false,
    requireDedicatedArm: true,
    weaponType: "horizontal spinner",
  },
  flipper: {
    label: "flipper",
    enabled: true,
    profile: "lifter",
    input: "rightBumper",
    armButton: "x",
    toggle: false,
    buttonPower: 1,
    maxOutput: 1,
    rampUpPerSecond: 20,
    rampDownPerSecond: 20,
    invert: false,
    requireDedicatedArm: true,
    weaponType: "flipper",
  },
  grabber: {
    label: "grabber",
    enabled: true,
    profile: "lifter",
    input: "rightTrigger",
    armButton: "x",
    toggle: false,
    buttonPower: 0.65,
    maxOutput: 0.65,
    rampUpPerSecond: 10,
    rampDownPerSecond: 10,
    invert: false,
    requireDedicatedArm: true,
    weaponType: "grabber",
  },
  pusher: {
    label: "pusher",
    enabled: false,
    weaponType: "pusher",
  },
};
const avatarLabels = {
  ant: "ANT",
  wedge: "WDG",
  spinner: "SPN",
  claw: "CLW",
  bolt: "BLT",
  shield: "SHD",
};
const setupQrUrl = "http://192.168.4.1/";
const setupQrSize = 25;
const setupQrBits = "1111111010011100101111111100000101110100000100000110111010001111000010111011011101000100101001011101101110101111001010101110110000010100101100010000011111111010101010101111111000000001011101010000000011100110111001110111100111100110001100101011001011000110111001000111101110110010101010001011010110001101001111011100111000001000100010000100100100001111110011110011111000111010000110111011010001000000110000101000011011111001000000000101001111000101011111111000010000101011001100000101000010010001001110111010001111011111110111011101000101000100111110101110101100111010011001110000010110110111001100001111111011000111101001001";

function loadStoredJson(key, fallback) {
  try {
    return JSON.parse(localStorage.getItem(key) || JSON.stringify(fallback));
  } catch (err) {
    return fallback;
  }
}

const state = {
  config: null,
  status: null,
  profiles: [],
  ws: null,
  token: localStorage.getItem("antcoreToken") || "",
  clientId: "", // Unique to this page instance, including duplicated tabs.
  webControlEnabled: false,
  controlClaimPending: false,
  statusPollInFlight: false,
  statusRequestSeq: 0,
  lastStatusUptimeMs: 0,
  sticks: {
    left: { x: 0, y: 0 },
    right: { x: 0, y: 0 },
  },
  triggers: {
    leftTrigger: 0,
    rightTrigger: 0,
  },
  buttons: {},
  learn: null,
  setupAck: loadStoredJson("antcoreSetupAck", {}),
  fightHistory: loadStoredJson("antcoreFightHistory", []),
  packs: [],
  selectedPackId: localStorage.getItem("antcoreSelectedPackId") || "",
  lastDebrief: loadStoredJson("antcoreLastDebrief", null),
  timer: {
    running: false,
    remaining: 120,
    lastTick: 0,
  },
  fight: {
    active: false,
    startedAt: 0,
    startUptimeMs: 0,
    lowestBattery: null,
    startEvents: null,
    lastArmed: false,
    countdownUntil: 0,
  },
};

if (!state.clientId) {
  state.clientId = Array.from(crypto.getRandomValues(new Uint8Array(8)),
    (byte) => byte.toString(16).padStart(2, "0")).join("");
  // Ownership is also bound to the WebSocket connection by the firmware.
}

function $(id) {
  return document.getElementById(id);
}

function toast(message) {
  const box = $("toast");
  box.textContent = message;
  box.classList.add("show");
  clearTimeout(toast.timer);
  toast.timer = setTimeout(() => box.classList.remove("show"), 2600);
}

function setBadge(el, text, level) {
  if (!el) return;
  el.textContent = text;
  el.className = `badge ${level || ""}`.trim();
}

function setBusy(el, busy) {
  if (!el) return;
  el.disabled = busy;
  el.classList.toggle("busy", busy);
}

function authHeaders(headers = {}) {
  const out = { ...headers };
  if (state.token) out["X-AntCore-Token"] = state.token;
  return out;
}

async function parseResponse(res) {
  const type = res.headers.get("content-type") || "";
  return type.includes("application/json") ? await res.json() : await res.text();
}

async function api(path, options = {}) {
  const opts = { ...options };
  const timeoutMs = options.timeoutMs || 5000;
  delete opts.timeoutMs;
  delete opts.auth;
  let timeout = null;
  if (typeof AbortController !== "undefined" && timeoutMs > 0) {
    const controller = new AbortController();
    opts.signal = controller.signal;
    timeout = setTimeout(() => controller.abort(), timeoutMs);
  }
  opts.headers = authHeaders(opts.headers || {});
  let res;
  try {
    res = await fetch(path, opts);
    if (res.status === 401 && options.auth !== false && await loginWithPrompt()) {
      opts.headers = authHeaders(options.headers || {});
      res = await fetch(path, opts);
    }
  } catch (err) {
    if (err.name === "AbortError") throw new Error("request timed out");
    throw err;
  } finally {
    if (timeout) clearTimeout(timeout);
  }
  const body = await parseResponse(res);
  if (!res.ok) {
    const message = typeof body === "string" ? body : (body.message || body.error || body.reason || res.statusText);
    const err = new Error(message);
    err.status = res.status;
    err.body = body;
    throw err;
  }
  return body;
}

async function loginWithPrompt() {
  const pin = prompt("Ant Core admin PIN");
  if (!pin) return false;
  const result = await api("/api/auth/login", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ pin }),
    auth: false,
  });
  state.token = result.token || "";
  localStorage.setItem("antcoreToken", state.token);
  renderAuth();
  toast(result.defaultPin ? "Logged in. Change the default PIN." : "Logged in");
  return !!state.token;
}

async function logout() {
  try {
    if (state.token) await api("/api/auth/logout", { method: "POST", auth: false });
  } catch (err) {
  }
  state.token = "";
  state.webControlEnabled = false;
  localStorage.removeItem("antcoreToken");
  renderAuth();
  toast("Logged out");
}

function renderAuth() {
  const locked = !state.token;
  setBadge($("authBadge"), locked ? "LOCKED" : "UNLOCKED", locked ? "warn" : "good");
  if ($("loginBtn")) $("loginBtn").textContent = locked ? "Login" : "Logout";
}

function armBlockReason(err) {
  return err?.body?.reason || visibleSafetyReason(state.status) || err?.message || "safety interlock";
}

function visibleSafetyReason(status) {
  if (!status) return "";
  if (status.safety?.armBlockReason) return status.safety.armBlockReason;
  if (status.battery?.critical) return "battery critical";
  if (status.safety?.benchMode) return "bench mode active";
  return status.disarmReason || "";
}

async function refreshStatusQuietly() {
  try {
    await loadStatus();
  } catch (err) {
    toast(`Status refresh failed: ${err.message}`);
  }
}

async function runAction(el, action, failurePrefix = "Action failed") {
  setBusy(el, true);
  try {
    await action();
  } catch (err) {
    toast(`${failurePrefix}: ${err.message}`);
    await refreshStatusQuietly();
  } finally {
    setBusy(el, false);
  }
}

async function setRobotArmed(shouldArm) {
  try {
    await api(shouldArm ? "/api/arm" : "/api/disarm", shouldArm ? { method: "POST" } : { method: "POST", auth: false, timeoutMs: 1200 });
    await loadStatus();
    toast(shouldArm ? "Robot armed" : "Robot disarmed");
  } catch (err) {
    await refreshStatusQuietly();
    toast(shouldArm ? `Arm blocked: ${armBlockReason(err)}` : `Disarm failed: ${err.message}`);
  }
}

async function toggleRobotArm(el) {
  setBusy(el, true);
  try {
    await setRobotArmed(!state.status?.armed);
  } finally {
    setBusy(el, false);
  }
}

async function toggleWeaponArm(el) {
  setBusy(el, true);
  try {
    await api(state.status?.weapon?.armed ? "/api/weapon/disarm" : "/api/weapon/arm", { method: "POST" });
    await loadStatus();
    toast(state.status?.weapon?.armed ? "Weapon armed" : "Weapon safe");
  } catch (err) {
    await refreshStatusQuietly();
    toast(`Weapon action blocked: ${err.message}`);
  } finally {
    setBusy(el, false);
  }
}

async function togglePitMode(el) {
  setBusy(el, true);
  try {
    const enabled = !state.status?.safety?.pitMode;
    if (enabled && !confirm("Enable pit mode and lock all live outputs off?")) return;
    await api("/api/pit", {
      method: "POST",
      headers: { "content-type": "application/json" },
      body: JSON.stringify({ enabled }),
    });
    await loadConfig();
    await loadStatus();
    toast(enabled ? "Pit mode enabled" : "Pit mode disabled");
  } catch (err) {
    await refreshStatusQuietly();
    toast(`Pit mode failed: ${err.message}`);
  } finally {
    setBusy(el, false);
  }
}

async function setLiveOutputFromUi() {
  const enabled = $("liveOutputEnabled").checked;
  if (enabled && !confirm("Enable live output tests? Motors or servos can move once the robot is armed.")) {
    $("liveOutputEnabled").checked = !!state.status?.liveOutputEnabled;
    return;
  }
  await api("/api/test/live-output", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ enabled }),
  });
  await loadStatus();
  toast(enabled ? "Live output tests enabled" : "Live output tests disabled");
}

async function runMotorTest() {
  const payload = {
    motor: Number($("testMotor").value),
    power: Number($("testMotorPower").value),
    durationMs: Number($("testMotorDuration").value),
  };
  if (!confirm(`Pulse motor ${payload.motor} at ${payload.power.toFixed(2)} for ${payload.durationMs} ms?`)) return;
  await api("/api/test/motor", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(payload),
  });
  await loadStatus();
  toast("Motor test queued");
}

async function runServoTest() {
  const payload = {
    servo: Number($("testServo").value),
    us: Number($("testServoUs").value),
    durationMs: Number($("testServoDuration").value),
  };
  if (!confirm(`Pulse servo ${payload.servo} to ${payload.us} us for ${payload.durationMs} ms?`)) return;
  await api("/api/test/servo", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(payload),
  });
  await loadStatus();
  toast("Servo test queued");
}

async function saveCalibration() {
  await saveConfig();
}

function captureCalibrationCenters() {
  document.querySelectorAll(".cal-axis").forEach((row) => {
    const raw = rawAxisValue(row.dataset.axis);
    row.querySelector(".calCenter").value = raw.toFixed(3);
  });
  $("mappingTestMode").checked = true;
  toast("Centers captured. Mapping test mode enabled until saved off.");
}

function resetCalibration() {
  const cfg = state.config || {};
  cfg.control = cfg.control || {};
  cfg.control.calibration = {
    enabled: true,
    mappingTestMode: false,
    axes: axisOptions.map(defaultCalibrationAxis),
  };
  bindChecked("calibrationEnabled", true);
  bindChecked("mappingTestMode", false);
  renderCalibrationConfig(cfg);
  toast("Calibration reset locally. Save to write it to the board.");
}

async function loadConfig() {
  state.config = await api("/api/config", { auth: false });
  renderConfig();
}

async function loadProfiles() {
  try {
    const result = await api("/api/profiles", { auth: false });
    state.profiles = result.profiles || [];
    renderProfileList(result);
  } catch (err) {
    state.profiles = [];
    renderProfileList({ ok: false, profiles: [] });
  }
}

async function loadPacks() {
  try {
    const result = await api("/api/packs", { auth: false });
    state.packs = result.packs || [];
    renderPackList();
  } catch (err) {
    state.packs = [];
    renderPackList(false);
  }
}

async function loadStatus() {
  if (state.statusPollInFlight) return state.status;
  const seq = ++state.statusRequestSeq;
  state.statusPollInFlight = true;
  try {
    const status = await api("/api/status", { auth: false, timeoutMs: 2500 });
    if (seq < state.statusRequestSeq || Number(status.uptimeMs || 0) < state.lastStatusUptimeMs) {
      return state.status;
    }
    state.lastStatusUptimeMs = Number(status.uptimeMs || 0);
    state.status = status;
    renderStatus(state.status);
    return state.status;
  } finally {
    state.statusPollInFlight = false;
  }
}

function connectWs() {
  const url = `${location.protocol === "https:" ? "wss" : "ws"}://${location.host}/ws`;
  state.ws = new WebSocket(url);
  state.ws.onopen = () => {
    toast("Live link connected");
    if (state.webControlEnabled) sendClaimControl();
  };
  state.ws.onmessage = (event) => {
    let msg;
    try {
      msg = JSON.parse(event.data);
    } catch (err) {
      toast("Live telemetry frame was invalid");
      return;
    }
    if (msg.type === "status") {
      state.statusRequestSeq++;
      state.lastStatusUptimeMs = Math.max(state.lastStatusUptimeMs, Number(msg.uptimeMs || 0));
      state.status = msg;
      renderStatus(msg);
    }
    if (msg.type === "controlClaim") {
      state.controlClaimPending = false;
      state.webControlEnabled = !!msg.ok && msg.webDriverId === state.clientId;
      if (!state.webControlEnabled && msg.reason) toast(`Control not held: ${msg.reason}`);
      if (state.status) renderStatus(state.status);
    }
    if (msg.type === "log" && state.status) {
      state.status.logs = [...(state.status.logs || []), msg.line].slice(-64);
      renderLogs(state.status.logs);
    }
  };
  state.ws.onclose = () => setTimeout(connectWs, 1200);
}

function optionList(values, selected) {
  return values.map((value) => `<option value="${value}" ${value === selected ? "selected" : ""}>${value}</option>`).join("");
}

function optionListPairs(values, selected) {
  return values.map(([value, label]) => `<option value="${value}" ${value === selected ? "selected" : ""}>${label}</option>`).join("");
}

function escapeHtml(value) {
  return String(value ?? "").replace(/[&<>"']/g, (char) => ({
    "&": "&amp;",
    "<": "&lt;",
    ">": "&gt;",
    '"': "&quot;",
    "'": "&#39;",
  })[char]);
}

function motorSelectOptions(selected) {
  return [1, 2, 3].map((value) => `<option value="${value}" ${value === selected ? "selected" : ""}>Motor ${value}</option>`).join("");
}

function bindNumber(id, value) {
  const el = $(id);
  if (el) el.value = value ?? "";
}

function bindChecked(id, value) {
  const el = $(id);
  if (el) el.checked = !!value;
}

function setSecretPlaceholder(id, isSet) {
  const el = $(id);
  if (!el) return;
  el.value = "";
  el.placeholder = isSet ? "saved; leave blank to keep" : "not set";
}

function learnKindForSelect(select) {
  if (!select) return "";
  if (select.classList.contains("servoAxis")) return "axis";
  if (select.classList.contains("servoButtonName") || select.classList.contains("macroButton")) return "button";
  if (["mapThrottleAxis", "mapTurnAxis", "mapLeftTankAxis", "mapRightTankAxis", "driveThrottleAxis", "driveTurnAxis"].includes(select.id)) return "axis";
  if (["mapWeaponInput", "weaponInput"].includes(select.id)) return "input";
  if (["mapArmButton", "mapWeaponArmButton", "mapDriveInvertButton", "mapTurboButton", "mapPrecisionButton", "driveInvertButton", "driveTurboButton", "drivePrecisionButton", "weaponArmButton"].includes(select.id)) return "button";
  return "";
}

function controllerRawForLearn(status = state.status) {
  const controller = status?.controller || {};
  return controller.xboxConnected ? (controller.xboxRaw || controller.active || {}) : (controller.active || controller.webRaw || {});
}

function cloneInputs(raw = {}) {
  return {
    axes: { ...(raw.axes || {}) },
    buttons: { ...(raw.buttons || {}) },
  };
}

function updateLearnStatus(text, active = false) {
  const el = $("mappingLearnStatus");
  if (!el) return;
  el.textContent = text;
  el.classList.toggle("active", active);
}

function clearLearnButtons() {
  document.querySelectorAll(".learn-button.active").forEach((btn) => btn.classList.remove("active"));
}

function stopInputLearn(message) {
  state.learn = null;
  clearLearnButtons();
  updateLearnStatus(message || "Use Learn on a mapping field, then move an axis or press a button on the active controller.");
}

function startInputLearn(select, kind, button) {
  const baseline = cloneInputs(controllerRawForLearn());
  state.learn = {
    select,
    kind,
    button,
    baseline,
    startedAt: performance.now(),
  };
  clearLearnButtons();
  button?.classList.add("active");
  const label = select.closest("label")?.textContent?.replace("Learn", "").trim() || "mapping";
  updateLearnStatus(`Learning ${label}: move ${kind === "button" ? "or press" : "an"} ${kind}.`, true);
}

function strongestMovedAxis(raw, baseline) {
  let best = "";
  let bestDelta = 0;
  const axes = raw.axes || {};
  for (const name of axisOptions) {
    const value = Number(axes[name] || 0);
    const start = Number(baseline.axes?.[name] || 0);
    const delta = Math.abs(value - start);
    if (delta > bestDelta && delta >= 0.35 && Math.abs(value) >= 0.35) {
      best = name;
      bestDelta = delta;
    }
  }
  return best;
}

function newlyPressedButton(raw, baseline) {
  const buttons = raw.buttons || {};
  for (const name of buttonOptions) {
    if (buttons[name] && !baseline.buttons?.[name]) return name;
  }
  return "";
}

function processInputLearn(status) {
  if (!state.learn) return;
  if (performance.now() - state.learn.startedAt > 8000) {
    stopInputLearn("Input learn timed out.");
    return;
  }
  const raw = controllerRawForLearn(status);
  const button = newlyPressedButton(raw, state.learn.baseline);
  const axis = strongestMovedAxis(raw, state.learn.baseline);
  const learned = state.learn.kind === "axis" ? axis : state.learn.kind === "button" ? button : (button || axis);
  if (!learned) return;
  const select = state.learn.select;
  if (![...select.options].some((option) => option.value === learned)) {
    stopInputLearn(`${learned} is not valid for that field.`);
    return;
  }
  select.value = learned;
  select.dispatchEvent(new Event("change", { bubbles: true }));
  stopInputLearn(`Mapped ${learned}.`);
  toast(`Mapped ${learned}`);
}

function installLearnButtons(root = document) {
  root.querySelectorAll("select").forEach((select) => {
    if (select.dataset.learnReady) return;
    const kind = learnKindForSelect(select);
    if (!kind) return;
    select.dataset.learnReady = "1";
    const button = document.createElement("button");
    button.type = "button";
    button.className = "learn-button";
    button.textContent = "Learn";
    button.title = "Assign this field from the next detected controller input";
    button.addEventListener("click", (ev) => {
      ev.preventDefault();
      startInputLearn(select, kind, button);
    });
    select.insertAdjacentElement("afterend", button);
  });
}

function capturePointer(el, pointerId) {
  try {
    el.setPointerCapture?.(pointerId);
  } catch (err) {
  }
}

function releasePointer(el, pointerId) {
  try {
    if (el.hasPointerCapture?.(pointerId)) el.releasePointerCapture(pointerId);
  } catch (err) {
  }
}

function renderConfig() {
  const cfg = state.config;
  if (!cfg) return;
  $("driveMode").value = cfg.drive.mode || "arcade";
  $("driveThrottleAxis").innerHTML = optionList(axisOptions, cfg.drive.throttleAxis || "leftY");
  $("driveTurnAxis").innerHTML = optionList(axisOptions, cfg.drive.turnAxis || "leftX");
  $("mapThrottleAxis").innerHTML = optionList(axisOptions, cfg.drive.throttleAxis || "leftY");
  $("mapTurnAxis").innerHTML = optionList(axisOptions, cfg.drive.turnAxis || "leftX");
  $("mapLeftTankAxis").innerHTML = optionList(axisOptions, cfg.drive.leftTankAxis || "leftY");
  $("mapRightTankAxis").innerHTML = optionList(axisOptions, cfg.drive.rightTankAxis || "rightY");
  bindNumber("driveDeadband", cfg.drive.deadband);
  bindNumber("driveExpo", cfg.drive.expo);
  bindNumber("throttleScale", cfg.drive.throttleScale);
  bindNumber("turnScale", cfg.drive.turnScale);
  $("leftMotor").innerHTML = motorSelectOptions(cfg.drive.leftMotor);
  $("rightMotor").innerHTML = motorSelectOptions(cfg.drive.rightMotor);
  bindChecked("driveInvertible", cfg.drive.invertible);
  $("driveInvertButton").innerHTML = optionList(buttonOptions, cfg.drive.invertButton || "view");
  $("driveTurboButton").innerHTML = optionList(buttonOptions, cfg.drive.turboButton || "rightStickButton");
  $("drivePrecisionButton").innerHTML = optionList(buttonOptions, cfg.drive.precisionButton || "leftStickButton");
  $("mapDriveInvertButton").innerHTML = optionList(buttonOptions, cfg.drive.invertButton || "view");
  $("mapTurboButton").innerHTML = optionList(buttonOptions, cfg.drive.turboButton || "rightStickButton");
  $("mapPrecisionButton").innerHTML = optionList(buttonOptions, cfg.drive.precisionButton || "leftStickButton");
  bindNumber("driveTurboScale", cfg.drive.turboScale ?? 1);
  bindNumber("drivePrecisionScale", cfg.drive.precisionScale ?? 0.45);
  bindChecked("driveGyroAssist", cfg.drive.gyroAssist);
  bindNumber("driveGyroGain", cfg.drive.gyroGain ?? 0.015);
  bindChecked("driveAutoInvert", cfg.drive.autoInvertWithImu);
  bindNumber("driveAutoInvertThreshold", cfg.drive.autoInvertAzThreshold ?? -0.45);

  $("weaponProfile").value = cfg.weapon.profile || "brushed";
  $("weaponInput").innerHTML = optionList(inputOptions, cfg.weapon.input);
  $("weaponArmButton").innerHTML = optionList(buttonOptions, cfg.weapon.armButton || "x");
  $("mapWeaponInput").innerHTML = optionList(inputOptions, cfg.weapon.input);
  $("mapWeaponArmButton").innerHTML = optionList(buttonOptions, cfg.weapon.armButton || "x");
  $("weaponToggle").value = String(!!cfg.weapon.toggle);
  bindChecked("weaponEnabled", cfg.weapon.enabled);
  bindChecked("weaponInvert", cfg.weapon.invert);
  bindChecked("weaponRequireArm", cfg.weapon.requireDedicatedArm ?? true);
  bindNumber("weaponButtonPower", cfg.weapon.buttonPower);
  bindNumber("weaponMaxOutput", cfg.weapon.maxOutput);
  bindNumber("weaponRampUp", cfg.weapon.rampUpPerSecond ?? 3);
  bindNumber("weaponRampDown", cfg.weapon.rampDownPerSecond ?? 8);

  $("robotName").value = cfg.robotName || "Ant Core";
  $("activeProfile").value = cfg.activeProfile || "Default";
  $("garageRobotName").value = cfg.robotName || "Ant Core";
  $("garageProfileName").value = cfg.activeProfile || "Default";
  $("garageBotType").value = cfg.garage?.botType || "skid";
  $("garageWeaponType").value = cfg.garage?.weaponType || "pusher";
  $("garageAccent").value = cfg.garage?.accent || "#ffca4f";
  $("garageAvatar").value = cfg.garage?.avatar || "ant";
  $("garageNotes").value = cfg.garage?.notes || "";
  bindChecked("mapXboxArmEnabled", cfg.control?.xboxArmEnabled ?? true);
  $("mapArmButton").innerHTML = optionList(buttonOptions, cfg.control?.armButton || "menu");
  bindChecked("calibrationEnabled", cfg.control?.calibration?.enabled ?? true);
  bindChecked("mappingTestMode", cfg.control?.calibration?.mappingTestMode);
  const selfRight = cfg.control?.selfRight || {};
  bindChecked("selfRightEnabled", selfRight.enabled);
  $("selfRightTarget").value = selfRight.target || "servo1";
  bindNumber("selfRightServoUs", selfRight.servoUs ?? 2000);
  bindNumber("selfRightMotorPower", selfRight.motorPower ?? 1);
  bindNumber("selfRightDuration", selfRight.durationMs ?? 550);
  bindNumber("selfRightCooldown", selfRight.cooldownMs ?? 1500);
  bindChecked("selfRightRequireWeaponArm", selfRight.requireWeaponArm ?? true);
  $("cameraFrameSize").value = cfg.camera?.frameSize || "qvga";
  bindNumber("cameraQuality", cfg.camera?.jpegQuality ?? 18);
  bindNumber("cameraBrightness", cfg.camera?.brightness ?? 0);
  bindNumber("cameraContrast", cfg.camera?.contrast ?? 0);
  bindNumber("cameraSaturation", cfg.camera?.saturation ?? 0);
  bindChecked("cameraHmirror", cfg.camera?.hmirror);
  bindChecked("cameraVflip", cfg.camera?.vflip);
  bindChecked("authEnabled", cfg.security?.authEnabled ?? true);
  setSecretPlaceholder("adminPin", cfg.security?.adminPinSet);
  setSecretPlaceholder("apPassword", cfg.apPasswordSet);
  bindChecked("pitMode", cfg.safety?.pitMode);
  bindChecked("requireControlSource", cfg.safety?.requireControlSource ?? true);

  bindChecked("batteryEnabled", cfg.battery.enabled);
  bindChecked("benchMode", cfg.battery.benchMode);
  bindNumber("batteryCalibration", cfg.battery.calibration);
  bindNumber("batteryWarn", cfg.battery.warnVoltage);
  bindNumber("batteryCritical", cfg.battery.criticalVoltage);
  bindChecked("derateEnabled", cfg.battery.derateEnabled);
  bindNumber("derateVoltage", cfg.battery.derateVoltage ?? 6.8);
  bindNumber("derateScale", cfg.battery.derateScale ?? 0.7);
  bindChecked("staEnabled", cfg.wifi?.staEnabled);
  $("staSsid").value = cfg.wifi?.staSsid || "";
  setSecretPlaceholder("staPassword", cfg.wifi?.staPasswordSet);

  renderMotorConfig(cfg);
  renderServoConfig(cfg);
  renderActionSlotConfig(cfg);
  renderCalibrationConfig(cfg);
  renderGarage();
  installLearnButtons();
}

function renderMotorConfig(cfg) {
  $("motorConfig").innerHTML = cfg.motors.map((m, index) => `
    <div class="row" data-motor="${index}">
      <div class="row-title"><span>Motor ${m.index}</span><span>D pins ${m.pinA}/${m.pinB}</span></div>
      <div class="form-grid">
        <label class="switch"><input class="motorInvert" type="checkbox" ${m.invert ? "checked" : ""}> invert</label>
        <label>Trim <input class="motorTrim" type="number" step="0.01" min="-0.25" max="0.25" value="${m.trim}"></label>
        <label>Max Output <input class="motorMax" type="number" step="0.05" min="0.05" max="1" value="${m.maxOutput}"></label>
        <label>Ramp / sec <input class="motorRamp" type="number" step="0.5" min="0" max="20" value="${m.rampPerSecond}"></label>
      </div>
    </div>
  `).join("");
}

function renderServoConfig(cfg) {
  $("servoConfig").innerHTML = cfg.servos.map((s, index) => `
    <div class="row" data-servo="${index}">
      <div class="row-title"><span>Servo ${s.index}</span><span>D pin ${s.pin}</span></div>
      <div class="preset-grid servo-presets" aria-label="Servo ${s.index} presets">
        <button class="text-button" data-servo-preset="joystick" type="button">Joystick</button>
        <button class="text-button" data-servo-preset="trigger" type="button">Trigger</button>
        <button class="text-button" data-servo-preset="buttons" type="button">Buttons</button>
        <button class="text-button" data-servo-preset="toggles" type="button">Toggles</button>
        <button class="text-button" data-servo-preset="failsafe" type="button">Failsafe</button>
        <button class="text-button" data-servo-preset="detach" type="button">Detach</button>
      </div>
      <div class="form-grid">
        <label class="switch"><input class="servoEnabled" type="checkbox" ${s.enabled ? "checked" : ""}> enabled</label>
        <label class="switch"><input class="servoAxisEnabled" type="checkbox" ${s.axisEnabled ? "checked" : ""}> axis active</label>
        <label>Axis <select class="servoAxis">${optionList(axisOptions, s.axis)}</select></label>
        <label class="switch"><input class="servoInvert" type="checkbox" ${s.invert ? "checked" : ""}> invert</label>
        <label>Min us <input class="servoMin" type="number" min="500" max="2400" value="${s.minUs}"></label>
        <label>Neutral us <input class="servoNeutral" type="number" min="500" max="2500" value="${s.neutralUs}"></label>
        <label>Max us <input class="servoMax" type="number" min="501" max="2500" value="${s.maxUs}"></label>
        <label>Failsafe us <input class="servoFailsafe" type="number" min="500" max="2500" value="${s.failsafeUs}"></label>
        <label class="switch"><input class="servoDetach" type="checkbox" ${s.detachOnDisarm ? "checked" : ""}> detach on disarm</label>
      </div>
      <div class="rows">
        ${s.buttons.map((b, buttonIndex) => `
          <div class="form-grid servo-button" data-button="${buttonIndex}">
            <label class="switch"><input class="servoButtonEnabled" type="checkbox" ${b.enabled ? "checked" : ""}> button map</label>
            <label>Button <select class="servoButtonName">${optionListPairs(buttonOptionPairs, b.button || "")}</select></label>
            <label>PWM us <input class="servoButtonUs" type="number" min="500" max="2500" value="${b.us}"></label>
            <label>Mode <select class="servoButtonMode"><option value="momentary" ${b.toggle ? "" : "selected"}>Momentary</option><option value="toggle" ${b.toggle ? "selected" : ""}>Toggle</option></select></label>
          </div>
        `).join("")}
      </div>
    </div>
  `).join("");
  document.querySelectorAll("[data-servo-preset]").forEach((btn) => {
    btn.addEventListener("click", () => applyServoPreset(btn.closest("[data-servo]"), btn.dataset.servoPreset));
  });
}

function renderActionSlotConfig(cfg) {
  const slots = cfg.control?.actions?.length
    ? cfg.control.actions
    : Array.from({ length: 4 }, (_, index) => ({ index: index + 1, enabled: false, button: "", action: "none" }));
  $("macroConfig").innerHTML = slots.map((slot, index) => `
    <div class="row" data-action-slot="${index}">
      <div class="row-title"><span>Slot ${slot.index || index + 1}</span><span>${slot.enabled ? slot.action : "disabled"}</span></div>
      <div class="form-grid">
        <label class="switch"><input class="macroEnabled" type="checkbox" ${slot.enabled ? "checked" : ""}> enabled</label>
        <label>Button <select class="macroButton">${optionListPairs(buttonOptionPairs, slot.button || "")}</select></label>
        <label>Action <select class="macroAction">${optionListPairs(actionOptions, slot.action || "none")}</select></label>
      </div>
    </div>
  `).join("");
}

function defaultCalibrationAxis(name) {
  const positiveOnly = name === "leftTrigger" || name === "rightTrigger";
  return { name, min: positiveOnly ? 0 : -1, center: 0, max: 1, deadband: 0.03, invert: false, positiveOnly };
}

function calibrationAxes(cfg = state.config) {
  const existing = cfg?.control?.calibration?.axes || [];
  return axisOptions.map((name) => ({
    ...defaultCalibrationAxis(name),
    ...(existing.find((axis) => axis.name === name) || {}),
  }));
}

function rawAxisValue(name) {
  const axes = state.status?.controller?.xboxRaw?.axes || {};
  return Number(axes[name] ?? 0);
}

function calibratedAxisValue(name) {
  const axes = state.status?.controller?.xboxCalibrated?.axes || state.status?.controller?.active?.axes || {};
  return Number(axes[name] ?? 0);
}

function renderCalibrationConfig(cfg) {
  const axes = calibrationAxes(cfg);
  $("calibrationRows").innerHTML = axes.map((axis) => {
    const raw = rawAxisValue(axis.name);
    const calibrated = calibratedAxisValue(axis.name);
    return `
      <div class="row cal-axis" data-axis="${axis.name}">
        <div class="row-title"><span>${axis.name}</span><span>raw ${raw.toFixed(2)} / cal ${calibrated.toFixed(2)}</span></div>
        <div class="form-grid">
          <label>Min <input class="calMin" type="number" min="${axis.positiveOnly ? 0 : -1}" max="1" step="0.01" value="${axis.min}"></label>
          <label>Center <input class="calCenter" type="number" min="${axis.positiveOnly ? 0 : -1}" max="1" step="0.01" value="${axis.center}"></label>
          <label>Max <input class="calMax" type="number" min="${axis.positiveOnly ? 0 : -1}" max="1" step="0.01" value="${axis.max}"></label>
          <label>Deadband <input class="calDeadband" type="number" min="0" max="0.45" step="0.01" value="${axis.deadband}"></label>
          <label class="switch"><input class="calInvert" type="checkbox" ${axis.invert ? "checked" : ""}> invert</label>
        </div>
        <div class="actions">
          <button class="text-button calSetMin" type="button">Use Raw Min</button>
          <button class="text-button calSetCenter" type="button">Use Raw Center</button>
          <button class="text-button calSetMax" type="button">Use Raw Max</button>
        </div>
      </div>
    `;
  }).join("");
  $("calibrationRows").querySelectorAll(".cal-axis").forEach((row) => {
    const name = row.dataset.axis;
    row.querySelector(".calSetMin").addEventListener("click", () => { row.querySelector(".calMin").value = rawAxisValue(name).toFixed(3); });
    row.querySelector(".calSetCenter").addEventListener("click", () => { row.querySelector(".calCenter").value = rawAxisValue(name).toFixed(3); });
    row.querySelector(".calSetMax").addEventListener("click", () => { row.querySelector(".calMax").value = rawAxisValue(name).toFixed(3); });
  });
}

function renderProfileList(result = {}) {
  const profiles = result.profiles || state.profiles || [];
  const select = $("profileSelect");
  if (!select) return;
  const active = state.config?.activeProfile || result.activeProfile || "";
  select.innerHTML = profiles.length
    ? profiles.map((profile) => {
        const name = profile.name || "";
        return `<option value="${escapeHtml(name)}" ${name === active ? "selected" : ""}>${escapeHtml(name)}</option>`;
      }).join("")
    : `<option value="">No saved profiles</option>`;
  setBadge($("profilesBadge"), result.ok === false ? "storage off" : `${profiles.length} saved`, result.ok === false ? "warn" : profiles.length ? "good" : "warn");
  renderGarage();
}

function avatarText(name) {
  return avatarLabels[name] || String(name || "AC").slice(0, 3).toUpperCase();
}

function renderGarage() {
  const cfg = state.config || {};
  const garage = cfg.garage || {};
  const robotName = $("garageRobotName")?.value || cfg.robotName || "Ant Core";
  const botType = $("garageBotType")?.value || garage.botType || "skid";
  const weaponType = $("garageWeaponType")?.value || garage.weaponType || "pusher";
  const accent = $("garageAccent")?.value || garage.accent || "#ffca4f";
  const avatar = $("garageAvatar")?.value || garage.avatar || "ant";
  [$("garageAvatarPreview"), $("fightAvatar"), $("spectatorAvatar")].forEach((el) => {
    if (!el) return;
    el.textContent = avatarText(avatar);
    el.style.setProperty("--avatar-accent", accent);
  });
  if ($("garageRobotPreview")) $("garageRobotPreview").textContent = robotName;
  if ($("garageMetaPreview")) $("garageMetaPreview").textContent = `${botType} / ${weaponType}`;
  if ($("fightRobotName")) $("fightRobotName").textContent = robotName;
  if ($("spectatorRobotName")) $("spectatorRobotName").textContent = robotName;
  setBadge($("garageProfileBadge"), `${(state.profiles || []).length} profiles`, state.profiles?.length ? "good" : "warn");
  const cards = $("robotGarageCards");
  if (!cards) return;
  const profiles = state.profiles?.length ? state.profiles : [{ name: cfg.activeProfile || "Default", size: 0 }];
  cards.innerHTML = profiles.map((profile) => {
    const active = (profile.name || "") === (cfg.activeProfile || "");
    const meta = active ? `${botType} / ${weaponType}` : "saved profile";
    return `
      <button class="garage-card ${active ? "active" : ""}" data-garage-profile="${escapeHtml(profile.name || "")}" type="button">
        <span class="mini-avatar" style="--avatar-accent:${escapeHtml(accent)}">${active ? avatarText(avatar) : "BOT"}</span>
        <strong>${escapeHtml(profile.name || "Default")}</strong>
        <small>${escapeHtml(meta)}</small>
      </button>
    `;
  }).join("");
  cards.querySelectorAll("[data-garage-profile]").forEach((btn) => {
    btn.addEventListener("click", () => {
      const select = $("profileSelect");
      if (select) select.value = btn.dataset.garageProfile;
      gotoTab("diagnostics");
      toast(`Selected ${btn.dataset.garageProfile}`);
    });
  });
}

function selectedProfileName() {
  return $("profileSelect").value || $("activeProfile").value.trim();
}

function selectedPack() {
  return state.packs.find((pack) => pack.id === state.selectedPackId) || state.packs[0] || null;
}

function clearPackForm() {
  state.selectedPackId = "";
  localStorage.removeItem("antcoreSelectedPackId");
  $("packName").value = "";
  $("packChargeVoltage").value = "8.40";
  $("packCycles").value = "0";
  $("packWeak").checked = false;
  $("packNotes").value = "";
  renderPackList();
}

function loadPackIntoForm(pack) {
  if (!pack) {
    clearPackForm();
    return;
  }
  state.selectedPackId = pack.id || "";
  localStorage.setItem("antcoreSelectedPackId", state.selectedPackId);
  $("packName").value = pack.name || "";
  $("packChargeVoltage").value = Number(pack.chargeVoltage || 8.4).toFixed(2);
  $("packCycles").value = Number(pack.cycles || 0);
  $("packWeak").checked = !!pack.weak;
  $("packNotes").value = pack.notes || "";
  renderPackList();
}

function packSagText(pack = selectedPack()) {
  const charge = Number(pack?.chargeVoltage || $("packChargeVoltage")?.value || 0);
  const live = Number(state.status?.battery?.packVolts || 0);
  if (!charge || !live) return "sag --";
  return `sag ${(charge - live).toFixed(2)} V`;
}

function renderPackList(ok = true) {
  const select = $("packSelect");
  if (!select) return;
  if (!state.selectedPackId && state.packs[0]) state.selectedPackId = state.packs[0].id || "";
  select.innerHTML = state.packs.length
    ? state.packs.map((pack) => `<option value="${escapeHtml(pack.id)}" ${pack.id === state.selectedPackId ? "selected" : ""}>${escapeHtml(pack.name || pack.id)}</option>`).join("")
    : `<option value="">No packs saved</option>`;
  const active = selectedPack();
  setBadge($("packSagValue"), ok ? packSagText(active) : "storage off", ok ? (active?.weak ? "warn" : "good") : "warn");
  $("packList").innerHTML = state.packs.length ? state.packs.map((pack) => `
    <button class="pack-card ${pack.id === state.selectedPackId ? "active" : ""} ${pack.weak ? "weak" : ""}" data-pack-id="${escapeHtml(pack.id)}" type="button">
      <strong>${escapeHtml(pack.name || pack.id)}</strong>
      <span>${Number(pack.chargeVoltage || 0).toFixed(2)} V charge / ${Number(pack.cycles || 0)} cycles</span>
      <small>${pack.weak ? "WEAK PACK" : escapeHtml(pack.notes || "no notes")}</small>
    </button>
  `).join("") : `<div class="check todo"><span>TODO</span>Add packs before competition day.</div>`;
  $("packList").querySelectorAll("[data-pack-id]").forEach((btn) => {
    btn.addEventListener("click", () => loadPackIntoForm(state.packs.find((pack) => pack.id === btn.dataset.packId)));
  });
}

function collectConfig() {
  const cfg = structuredClone(state.config);
  cfg.robotName = ($("garageRobotName")?.value || $("robotName").value).trim() || "Ant Core";
  cfg.activeProfile = ($("garageProfileName")?.value || $("activeProfile").value).trim() || "Default";
  cfg.apPassword = $("apPassword").value;
  cfg.garage = cfg.garage || {};
  cfg.garage.botType = $("garageBotType")?.value || "skid";
  cfg.garage.weaponType = $("garageWeaponType")?.value || "pusher";
  cfg.garage.accent = $("garageAccent")?.value || "#ffca4f";
  cfg.garage.avatar = $("garageAvatar")?.value || "ant";
  cfg.garage.notes = $("garageNotes")?.value.trim() || "";
  cfg.security = cfg.security || {};
  cfg.security.authEnabled = $("authEnabled").checked;
  cfg.security.adminPin = $("adminPin").value;
  cfg.control = cfg.control || {};
  cfg.control.xboxArmEnabled = $("mapXboxArmEnabled").checked;
  cfg.control.armButton = $("mapArmButton").value;
  cfg.control.calibration = cfg.control.calibration || {};
  cfg.control.calibration.enabled = $("calibrationEnabled").checked;
  cfg.control.calibration.mappingTestMode = $("mappingTestMode").checked;
  cfg.control.calibration.axes = [...document.querySelectorAll(".cal-axis")].map((row) => ({
    name: row.dataset.axis,
    min: Number(row.querySelector(".calMin").value),
    center: Number(row.querySelector(".calCenter").value),
    max: Number(row.querySelector(".calMax").value),
    deadband: Number(row.querySelector(".calDeadband").value),
    invert: row.querySelector(".calInvert").checked,
  }));
  cfg.control.selfRight = cfg.control.selfRight || {};
  cfg.control.selfRight.enabled = $("selfRightEnabled").checked;
  cfg.control.selfRight.target = $("selfRightTarget").value;
  cfg.control.selfRight.servoUs = Number($("selfRightServoUs").value);
  cfg.control.selfRight.motorPower = Number($("selfRightMotorPower").value);
  cfg.control.selfRight.durationMs = Number($("selfRightDuration").value);
  cfg.control.selfRight.cooldownMs = Number($("selfRightCooldown").value);
  cfg.control.selfRight.requireWeaponArm = $("selfRightRequireWeaponArm").checked;
  cfg.control.actions = [...document.querySelectorAll("[data-action-slot]")].map((row, index) => ({
    index: index + 1,
    enabled: row.querySelector(".macroEnabled").checked,
    button: row.querySelector(".macroButton").value,
    action: row.querySelector(".macroAction").value,
  }));
  cfg.camera = cfg.camera || {};
  cfg.camera.frameSize = $("cameraFrameSize").value;
  cfg.camera.jpegQuality = Number($("cameraQuality").value);
  cfg.camera.brightness = Number($("cameraBrightness").value);
  cfg.camera.contrast = Number($("cameraContrast").value);
  cfg.camera.saturation = Number($("cameraSaturation").value);
  cfg.camera.hmirror = $("cameraHmirror").checked;
  cfg.camera.vflip = $("cameraVflip").checked;
  cfg.safety = cfg.safety || {};
  cfg.safety.pitMode = $("pitMode").checked;
  cfg.safety.requireControlSource = $("requireControlSource").checked;
  cfg.drive.mode = $("driveMode").value;
  cfg.drive.throttleAxis = $("mapThrottleAxis").value;
  cfg.drive.turnAxis = $("mapTurnAxis").value;
  cfg.drive.leftTankAxis = $("mapLeftTankAxis").value;
  cfg.drive.rightTankAxis = $("mapRightTankAxis").value;
  cfg.drive.deadband = Number($("driveDeadband").value);
  cfg.drive.expo = Number($("driveExpo").value);
  cfg.drive.throttleScale = Number($("throttleScale").value);
  cfg.drive.turnScale = Number($("turnScale").value);
  cfg.drive.leftMotor = Number($("leftMotor").value);
  cfg.drive.rightMotor = Number($("rightMotor").value);
  cfg.drive.invertible = $("driveInvertible").checked;
  cfg.drive.invertButton = $("mapDriveInvertButton").value;
  cfg.drive.turboButton = $("mapTurboButton").value;
  cfg.drive.precisionButton = $("mapPrecisionButton").value;
  cfg.drive.turboScale = Number($("driveTurboScale").value);
  cfg.drive.precisionScale = Number($("drivePrecisionScale").value);
  cfg.drive.gyroAssist = $("driveGyroAssist").checked;
  cfg.drive.gyroGain = Number($("driveGyroGain").value);
  cfg.drive.autoInvertWithImu = $("driveAutoInvert").checked;
  cfg.drive.autoInvertAzThreshold = Number($("driveAutoInvertThreshold").value);
  cfg.motors = [...document.querySelectorAll("[data-motor]")].map((row, i) => ({
    ...cfg.motors[i],
    invert: row.querySelector(".motorInvert").checked,
    trim: Number(row.querySelector(".motorTrim").value),
    maxOutput: Number(row.querySelector(".motorMax").value),
    rampPerSecond: Number(row.querySelector(".motorRamp").value),
  }));
  cfg.weapon.enabled = $("weaponEnabled").checked;
  cfg.weapon.profile = $("weaponProfile").value;
  cfg.weapon.input = $("mapWeaponInput").value;
  cfg.weapon.armButton = $("mapWeaponArmButton").value;
  cfg.weapon.toggle = $("weaponToggle").value === "true";
  cfg.weapon.invert = $("weaponInvert").checked;
  cfg.weapon.requireDedicatedArm = $("weaponRequireArm").checked;
  cfg.weapon.buttonPower = Number($("weaponButtonPower").value);
  cfg.weapon.maxOutput = Number($("weaponMaxOutput").value);
  cfg.weapon.rampUpPerSecond = Number($("weaponRampUp").value);
  cfg.weapon.rampDownPerSecond = Number($("weaponRampDown").value);
  cfg.battery.enabled = $("batteryEnabled").checked;
  cfg.battery.benchMode = $("benchMode").checked;
  cfg.battery.calibration = Number($("batteryCalibration").value);
  cfg.battery.warnVoltage = Number($("batteryWarn").value);
  cfg.battery.criticalVoltage = Number($("batteryCritical").value);
  cfg.battery.derateEnabled = $("derateEnabled").checked;
  cfg.battery.derateVoltage = Number($("derateVoltage").value);
  cfg.battery.derateScale = Number($("derateScale").value);
  cfg.wifi = cfg.wifi || {};
  cfg.wifi.staEnabled = $("staEnabled").checked;
  cfg.wifi.staSsid = $("staSsid").value.trim();
  cfg.wifi.staPassword = $("staPassword").value;
  cfg.servos = [...document.querySelectorAll("[data-servo]")].map((row, i) => ({
    ...cfg.servos[i],
    enabled: row.querySelector(".servoEnabled").checked,
    axisEnabled: row.querySelector(".servoAxisEnabled").checked,
    axis: row.querySelector(".servoAxis").value,
    invert: row.querySelector(".servoInvert").checked,
    minUs: Number(row.querySelector(".servoMin").value),
    neutralUs: Number(row.querySelector(".servoNeutral").value),
    maxUs: Number(row.querySelector(".servoMax").value),
    failsafeUs: Number(row.querySelector(".servoFailsafe").value),
    detachOnDisarm: row.querySelector(".servoDetach").checked,
    buttons: [...row.querySelectorAll(".servo-button")].map((buttonRow, buttonIndex) => ({
      ...cfg.servos[i].buttons[buttonIndex],
      enabled: buttonRow.querySelector(".servoButtonEnabled").checked,
      button: buttonRow.querySelector(".servoButtonName").value,
      us: Number(buttonRow.querySelector(".servoButtonUs").value),
      toggle: buttonRow.querySelector(".servoButtonMode").value === "toggle",
    })),
  }));
  return cfg;
}

function riskyConfigChanged(cfg) {
  if (state.status?.battery?.supported !== false && !cfg.battery.enabled) return "Battery safety is disabled. Outputs will no longer be blocked by low voltage.";
  if (cfg.security && !cfg.security.authEnabled) return "Admin PIN protection is disabled. Anyone on Wi-Fi can change dangerous settings.";
  if (cfg.safety && !cfg.safety.requireControlSource) return "The robot can arm without a fresh Xbox or web driver source.";
  return "";
}

async function saveConfig() {
  const cfg = collectConfig();
  const warning = riskyConfigChanged(cfg);
  if (warning && !confirm(`${warning}\n\nSave this configuration?`)) return false;
  await api("/api/config", {
    method: "PUT",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(cfg),
  });
  toast("Configuration saved. Robot disarmed.");
  await loadConfig();
  await loadStatus();
  return true;
}

function renderBars(list, container, type) {
  container.innerHTML = list.map((item) => {
    const val = type === "motor" ? item.output : ((item.us - 1500) / 500);
    const pct = Math.max(0, Math.min(100, (val + 1) * 50));
    const text = type === "motor"
      ? `${item.output.toFixed(2)} target ${(item.target ?? 0).toFixed(2)} PWM ${item.pwmA ?? 0}/${item.pwmB ?? 0} ${item.override || ""}`
      : `${item.us} us failsafe ${item.failsafeUs ?? "--"} ${item.override || ""}`;
    return `<div class="bar"><span>${type === "motor" ? "Motor" : "Servo"} ${item.index}</span><div class="track"><div class="fill" style="width:${pct}%"></div></div><strong>${text}</strong></div>`;
  }).join("");
}

function renderLogs(logs = []) {
  $("logBox").textContent = logs.slice(-64).join("\n");
  $("logBox").scrollTop = $("logBox").scrollHeight;
}

function renderSafetyBanners(status) {
  const banners = [];
  const safety = status.safety || {};
  const config = safety.config || {};
  if (status.battery?.supported === false) banners.push(["warn", "Battery monitoring unavailable on this board: check battery externally; no automatic low-voltage protection"]);
  if (safety.configValid === false) banners.push(["danger", "Configuration has errors; arming is blocked"]);
  else if ((config.warningCount || 0) > 0) banners.push(["warn", "Configuration has warnings"]);
  if (safety.pitMode) banners.push(["warn", "Pit mode is active; all live outputs are locked off"]);
  if (safety.mappingTestMode) banners.push(["warn", "Controller mapping test mode is active; arming is blocked"]);
  if (status.liveOutputEnabled) banners.push(["warn", "Live output tests are enabled"]);
  if (safety.defaultAdminPin) banners.push(["warn", "Default admin PIN is still active"]);
  if (safety.benchMode) banners.push(["warn", "USB bench mode is active; arming is blocked"]);
  if (safety.batteryCritical) banners.push(["danger", "Battery is critical; outputs are locked safe"]);
  else if (safety.batteryWarn) banners.push(["warn", "Battery is below warning threshold"]);
  if (safety.otaInProgress) banners.push(["warn", "OTA update in progress"]);
  if (safety.requireControlSource !== false && !safety.activeControlAvailable) banners.push(["warn", "No active Xbox or web driver control source"]);
  if (!banners.length) banners.push(["good", "Safety interlocks nominal"]);
  $("safetyBanners").innerHTML = banners.map(([level, text]) => `<div class="banner ${level}">${text}</div>`).join("");
}

function saveSetupAck() {
  localStorage.setItem("antcoreSetupAck", JSON.stringify(state.setupAck));
}

function renderSetupQr() {
  const box = $("setupQr");
  if (!box || box.dataset.rendered === "1") return;
  box.style.gridTemplateColumns = `repeat(${setupQrSize}, 1fr)`;
  box.innerHTML = [...setupQrBits].map((bit) => `<span class="${bit === "1" ? "on" : ""}"></span>`).join("");
  box.dataset.rendered = "1";
  if ($("setupQrUrl")) $("setupQrUrl").textContent = setupQrUrl;
  setBadge($("setupQrBadge"), "ready", "good");
}

function setupFlowSteps(status) {
  const cfg = state.config || {};
  const sta = status.wifi?.sta || {};
  const activeProfile = cfg.activeProfile || "Default";
  const profileSaved = (state.profiles || []).some((profile) => profile.name === activeProfile);
  return [
    {
      id: "security",
      label: "Set Security",
      ok: !status.security?.defaultPin,
      tab: "diagnostics",
      detail: "Change the admin PIN and AP password before testing near other people.",
    },
    {
      id: "network",
      label: "Join Home Wi-Fi",
      ok: !!sta.connected,
      tab: "diagnostics",
      detail: sta.connected ? `LAN reachable at ${sta.ip || "assigned IP"}` : "Keep AP fallback on, then add trusted LAN credentials for debugging.",
    },
    {
      id: "controller",
      label: "Pair Controller",
      ok: !!status.controller?.xboxConnected,
      tab: "controller",
      detail: "Pair the Xbox controller and confirm raw axes and buttons move in the inspector.",
    },
    {
      id: "mapping",
      label: "Map Controls",
      ok: !!cfg.control?.armButton && !status.safety?.mappingTestMode,
      tab: "controller",
      detail: "Assign arm, weapon, invert, turbo, precision, self-right, and servo controls.",
    },
    {
      id: "driveDirection",
      label: "Check Drive Direction",
      ok: !!state.setupAck.driveDirection,
      tab: "drive",
      ack: true,
      detail: "Use the drive direction wizard to record whether each side needs inversion.",
    },
    {
      id: "servoLimits",
      label: "Set Servo Limits",
      ok: !!state.setupAck.servoLimits,
      tab: "servos",
      ack: true,
      detail: "Set min, neutral, max, failsafe, and detach behavior so mechanisms cannot bind.",
    },
    {
      id: "battery",
      label: "Calibrate Battery",
      ok: !!cfg.battery?.enabled && (status.battery?.packVolts || 0) > 0.05,
      tab: "diagnostics",
      detail: "Verify raw ADC, pack voltage, warning threshold, critical threshold, and bench mode before arming.",
    },
    {
      id: "fpv",
      label: "Verify FPV",
      ok: !!status.camera?.ready,
      tab: "fpv",
      detail: "Confirm the camera stream is upright and tune quality, exposure, mirror, and flip.",
    },
    {
      id: "backup",
      label: "Save Profile And Backup",
      ok: profileSaved || !!state.setupAck.exportBackup,
      tab: "diagnostics",
      detail: "Save a named profile and export JSON so the robot can be recovered after reflashing.",
    },
  ].filter((step) => (step.id !== "fpv" || status.camera?.supported !== false)
    && (step.id !== "battery" || status.battery?.supported !== false));
}

function renderSetupFlow(status) {
  const steps = setupFlowSteps(status);
  const complete = steps.filter((step) => step.ok).length;
  const pct = Math.round((complete / steps.length) * 100);
  const next = steps.find((step) => !step.ok) || null;
  state.setupNextStep = next;
  setBadge($("setupFlowBadge"), next ? `${complete}/${steps.length}` : "ready", next ? "warn" : "good");
  $("setupProgressBar").style.width = `${pct}%`;
  $("setupNextBtn").textContent = next ? `Open ${next.label}` : "Open Dashboard";
  $("setupFlow").innerHTML = steps.map((step, index) => `
    <div class="setup-step ${step.ok ? "done" : ""} ${next?.id === step.id ? "current" : ""}">
      <span class="marker">${step.ok ? "OK" : String(index + 1).padStart(2, "0")}</span>
      <div>
        <h3>${escapeHtml(step.label)}</h3>
        <p>${escapeHtml(step.detail)}</p>
      </div>
      <div class="setup-actions">
        <button class="text-button" data-setup-tab="${step.tab}" type="button">Open</button>
        ${step.ack ? `<button class="text-button" data-setup-ack="${step.id}" type="button">${step.ok ? "Checked" : "Mark Checked"}</button>` : ""}
      </div>
    </div>
  `).join("");
  document.querySelectorAll("[data-setup-tab]").forEach((btn) => {
    btn.addEventListener("click", () => gotoTab(btn.dataset.setupTab));
  });
  document.querySelectorAll("[data-setup-ack]").forEach((btn) => {
    btn.addEventListener("click", () => {
      state.setupAck[btn.dataset.setupAck] = true;
      saveSetupAck();
      renderSetupFlow(state.status || status);
      toast("Setup step checked");
    });
  });
}

function renderSetupChecklist(status) {
  const cfg = state.config || {};
  const items = [
    ["Admin PIN changed", !status.security?.defaultPin],
    ...(status.battery?.supported === false ? [] : [["Battery calibrated", !!cfg.battery?.enabled && (status.battery?.packVolts || 0) > 0.05]]),
    ["Controller linked", !!status.controller?.xboxConnected],
    ["Home Wi-Fi ready", !!status.wifi?.sta?.connected],
    ...(status.camera?.supported === false ? [] : [["Camera detected", !!status.camera?.ready]]),
    ["Mapping test off", !status.safety?.mappingTestMode],
    ["Pit mode off", !status.safety?.pitMode],
    ["Bench mode off", !status.safety?.benchMode],
  ];
  const complete = items.filter((item) => item[1]).length;
  setBadge($("setupBadge"), `${complete}/${items.length}`, complete === items.length ? "good" : "warn");
  $("setupChecklist").innerHTML = items.map(([text, ok]) => `<div class="check ${ok ? "ok" : "todo"}"><span>${ok ? "OK" : "TODO"}</span>${text}</div>`).join("");
}

function preFightItems(status = state.status) {
  const cfg = state.config || {};
  const selectedProfile = !!(cfg.activeProfile || "").trim();
  const controlReady = !!status?.controller?.xboxConnected || status?.controller?.source === "web";
  return [
    ...(status?.battery?.supported === false ? [] : [["Battery OK", !!status?.battery?.enabled && !status?.battery?.critical && (status?.battery?.packVolts || 0) >= (cfg.battery?.criticalVoltage || 6.4)]]),
    ["Controller or web driver ready", controlReady],
    ["Robot disarmed", !status?.armed],
    ["Weapon safe", !status?.weapon?.armed],
    ["Profile selected", selectedProfile],
    ...(status?.camera?.supported === false ? [] : [["Camera ready", !!status?.camera?.ready]]),
    ["Pit mode off", !status?.safety?.pitMode],
    ["Drive direction tested", !!state.setupAck.driveDirection],
  ];
}

function renderPreFightChecklist(status = state.status) {
  const items = preFightItems(status);
  const ready = items.every((item) => item[1]);
  setBadge($("fightReadyBadge"), ready ? "READY" : "not ready", ready ? "good" : "warn");
  if ($("preFightChecklist")) {
    $("preFightChecklist").innerHTML = items.map(([text, ok]) => `<div class="check ${ok ? "ok" : "todo"}"><span>${ok ? "OK" : "TODO"}</span>${text}</div>`).join("");
  }
  document.body.classList.toggle("bout-ready", ready);
  return ready;
}

function eventDelta(current = {}, baseline = {}) {
  return Math.max(0, Number(current || 0) - Number(baseline || 0));
}

function startFightSession(countdown = false) {
  const status = state.status || {};
  state.fight.active = true;
  state.fight.startedAt = Date.now();
  state.fight.startUptimeMs = status.uptimeMs || 0;
  state.fight.lowestBattery = status.battery?.packVolts || null;
  state.fight.startEvents = { ...(status.events || {}) };
  state.fight.lastArmed = !!status.armed;
  if (countdown) state.fight.countdownUntil = performance.now() + 3000;
  state.timer.remaining = Number($("timerDuration")?.value || 120);
  state.timer.running = true;
  state.timer.lastTick = countdown ? 0 : performance.now();
  document.body.classList.add("fight-mode");
  renderTimer();
  toast(countdown ? "Fight countdown started" : "Fight session started");
}

function finishFightSession(status = state.status) {
  if (!state.fight.active) return;
  const events = status?.events || {};
  const baseline = state.fight.startEvents || {};
  const duration = Math.max(0, Math.round((Date.now() - state.fight.startedAt) / 1000));
  const debrief = {
    at: new Date().toISOString(),
    robotName: state.config?.robotName || status?.robotName || "Ant Core",
    profile: state.config?.activeProfile || status?.activeProfile || "Default",
    duration,
    lowestBattery: state.fight.lowestBattery,
    disconnects: eventDelta(events.controlDisconnects, baseline.controlDisconnects),
    failsafes: eventDelta(events.failsafes, baseline.failsafes),
    weaponArms: eventDelta(events.weaponArms, baseline.weaponArms),
    disarms: eventDelta(events.disarms, baseline.disarms),
    resetReason: status?.system?.resetReason || "unknown",
    highlights: (status?.logs || []).slice(-8),
  };
  state.lastDebrief = debrief;
  state.fightHistory = [debrief, ...(state.fightHistory || [])].slice(0, 10);
  localStorage.setItem("antcoreLastDebrief", JSON.stringify(debrief));
  localStorage.setItem("antcoreFightHistory", JSON.stringify(state.fightHistory));
  state.fight.active = false;
  state.timer.running = false;
  setBadge($("debriefBadge"), "ready", "good");
  renderDebrief();
}

function renderDebrief() {
  const debrief = state.lastDebrief;
  if (!$("debriefSummary")) return;
  if (!debrief) {
    $("debriefSummary").innerHTML = `<div class="check todo"><span>IDLE</span>No fight session recorded yet.</div>`;
    return;
  }
  const mm = String(Math.floor(debrief.duration / 60)).padStart(2, "0");
  const ss = String(debrief.duration % 60).padStart(2, "0");
  $("debriefSummary").innerHTML = `
    <div class="telemetry-grid compact">
      <div><span>Duration</span><strong>${mm}:${ss}</strong></div>
      <div><span>Lowest V</span><strong>${debrief.lowestBattery ? debrief.lowestBattery.toFixed(2) : "--"}</strong></div>
      <div><span>Disconnects</span><strong>${debrief.disconnects}</strong></div>
      <div><span>Failsafes</span><strong>${debrief.failsafes}</strong></div>
      <div><span>Weapon Arms</span><strong>${debrief.weaponArms}</strong></div>
      <div><span>Reset</span><strong>${escapeHtml(debrief.resetReason)}</strong></div>
    </div>
    <pre>${escapeHtml((debrief.highlights || []).join("\n"))}</pre>
  `;
}

function renderFightStatus(status) {
  const battery = status.battery || {};
  const controller = status.controller || {};
  const weapon = status.weapon || {};
  const signal = controller.source === "none" ? "no source" : controller.webDriverLocked ? "web lock" : controller.xboxConnected ? "xbox" : controller.source;
  if ($("fightArmState")) $("fightArmState").textContent = status.armed ? "ARMED" : "DISARMED";
  if ($("fightWeaponState")) $("fightWeaponState").textContent = weapon.armed ? "HOT" : "SAFE";
  if ($("fightBattery")) $("fightBattery").textContent = battery.supported === false ? "N/A" : `${Number(battery.packVolts || 0).toFixed(2)} V`;
  if ($("fightSource")) $("fightSource").textContent = controller.source || "none";
  if ($("fightSignal")) $("fightSignal").textContent = signal || "idle";
  if ($("fightWeaponHot")) $("fightWeaponHot").textContent = weapon.armed ? "WEAPON HOT" : "weapon safe";
  if ($("spectatorArm")) $("spectatorArm").textContent = status.armed ? "ARMED" : "DISARMED";
  if ($("spectatorWeapon")) $("spectatorWeapon").textContent = weapon.armed ? "HOT" : "SAFE";
  if ($("spectatorBattery")) $("spectatorBattery").textContent = battery.supported === false ? "N/A" : `${Number(battery.packVolts || 0).toFixed(2)} V`;
  if ($("spectatorSource")) $("spectatorSource").textContent = controller.source || "none";
  if ($("spectatorSignal")) $("spectatorSignal").textContent = signal || "idle";
  document.body.classList.toggle("weapon-hot-active", !!weapon.armed);
  document.body.classList.toggle("battery-alert", !!battery.warn || !!battery.critical);
  if (state.fight.active && battery.packVolts > 0) {
    state.fight.lowestBattery = state.fight.lowestBattery == null ? battery.packVolts : Math.min(state.fight.lowestBattery, battery.packVolts);
  }
  if (state.fight.active && state.fight.lastArmed && !status.armed) finishFightSession(status);
  state.fight.lastArmed = !!status.armed;
  renderPreFightChecklist(status);
  renderDebrief();
}

function renderControlTrainer(status = state.status) {
  const cfg = state.config || {};
  const active = status?.controller?.active?.buttons || {};
  const labels = {
    leftStick: `${cfg.drive?.mode || "arcade"} drive`,
    rightStick: `servo ${cfg.servos?.[0]?.axis || "rightY"}`,
    menu: "robot arm",
    view: "drive invert",
    x: "weapon arm",
    a: "servo pos",
    b: "disarm slot",
    y: "self-right",
    lb: "servo/trigger",
    rb: "weapon/lifter",
    lt: "left trigger",
    rt: "weapon input",
  };
  const hot = (name) => active[name] ? " active" : "";
  setBadge($("trainerBadge"), status?.controller?.source || "none", status?.controller?.source === "none" ? "warn" : "good");
  $("controlTrainer").innerHTML = `
    <div class="trainer-pad">
      <span class="trainer-button${hot("leftBumper")}">LB<small>${labels.lb}</small></span>
      <span class="trainer-button${hot("rightBumper")}">RB<small>${labels.rb}</small></span>
      <span class="trainer-stick">L<small>${labels.leftStick}</small></span>
      <span class="trainer-stick">R<small>${labels.rightStick}</small></span>
      <span class="trainer-button${hot("view")}">VIEW<small>${labels.view}</small></span>
      <span class="trainer-button${hot("menu")}">MENU<small>${labels.menu}</small></span>
      <span class="trainer-face${hot("y")}">Y<small>${labels.y}</small></span>
      <span class="trainer-face${hot("x")}">X<small>${labels.x}</small></span>
      <span class="trainer-face${hot("b")}">B<small>${labels.b}</small></span>
      <span class="trainer-face${hot("a")}">A<small>${labels.a}</small></span>
      <span class="trainer-trigger">LT<small>${labels.lt}</small></span>
      <span class="trainer-trigger">RT<small>${labels.rt}</small></span>
    </div>
  `;
}

function renderDriveWizard() {
  const done = !!state.setupAck.driveDirection;
  setBadge($("driveWizardBadge"), done ? "checked" : "unchecked", done ? "good" : "warn");
  if ($("driveWizardStatus")) {
    $("driveWizardStatus").textContent = done
      ? "Drive direction has been recorded for this browser. Save Direction writes the current inversion values."
      : "Direction check has not been recorded for this browser.";
  }
}

function renderConfigHealth(status) {
  const config = status.safety?.config || {};
  const errors = config.errors || [];
  const warnings = config.warnings || [];
  const ok = config.valid !== false;
  const badgeText = ok ? `${warnings.length} warnings` : `${errors.length} errors`;
  setBadge($("configHealthBadge"), badgeText, ok ? (warnings.length ? "warn" : "good") : "danger");
  const rows = [
    ...errors.map((text) => ["ERR", text, "todo"]),
    ...warnings.map((text) => ["WARN", text, "todo"]),
  ];
  $("configHealth").innerHTML = rows.length
    ? rows.map(([tag, text, cls]) => `<div class="check ${cls}"><span>${tag}</span>${escapeHtml(text)}</div>`).join("")
    : `<div class="check ok"><span>OK</span>Configuration is valid for arming.</div>`;
}

function gotoTab(tabName) {
  const tab = document.querySelector(`.tab[data-tab="${tabName}"]`);
  if (tab) tab.click();
}

function renderWizard(status) {
  const steps = [
    { label: "Security", ok: !status.security?.defaultPin, tab: "diagnostics" },
    { label: "Network", ok: !!status.wifi?.sta?.connected, tab: "diagnostics" },
    { label: "Controller", ok: !!status.controller?.xboxConnected, tab: "controller" },
    { label: "Action Mapping", ok: !!state.config?.control?.armButton, tab: "controller" },
    { label: "Drive", ok: !!state.config?.drive?.mode, tab: "drive" },
    { label: "Servos", ok: Array.isArray(state.config?.servos) && state.config.servos.length === 2, tab: "servos" },
    ...(status.battery?.supported === false ? [] : [{ label: "Battery", ok: !!state.config?.battery?.enabled && (status.battery?.packVolts || 0) > 0.05, tab: "diagnostics" }]),
    ...(status.camera?.supported === false ? [] : [{ label: "Camera", ok: !!status.camera?.ready, tab: "fpv" }]),
  ];
  const done = steps.filter((step) => step.ok).length;
  setBadge($("wizardBadge"), `${done}/${steps.length}`, done === steps.length ? "good" : "warn");
  $("wizardSteps").innerHTML = steps.map((step) => `
    <div class="wizard-step ${step.ok ? "ok" : "todo"}">
      <span>${step.ok ? "OK" : "TODO"}</span>
      ${step.label}
      <button class="text-button" data-goto="${step.tab}" type="button">${step.ok ? "Open" : "Set"}</button>
    </div>
  `).join("");
  document.querySelectorAll("[data-goto]").forEach((btn) => {
    btn.addEventListener("click", () => gotoTab(btn.dataset.goto));
  });
}

function renderControllerInspector(status) {
  const controller = status.controller || {};
  setBadge($("driverBadge"), controller.webDriverLocked ? "web locked" : (controller.source || "none"), controller.source === "none" ? "warn" : "good");
  const raw = controller.active || {};
  const axes = raw.axes || {};
  const buttons = raw.buttons || {};
  const selfRight = controller.selfRight || {};
  const axisHtml = axisOptions.map((name) => {
    const value = Number(axes[name] || 0);
    const pct = Math.max(0, Math.min(100, (value + 1) * 50));
    return `<div class="axis"><span>${escapeHtml(name)}</span><div class="track"><div class="fill" style="width:${pct}%"></div></div><strong>${value.toFixed(2)}</strong></div>`;
  }).join("");
  const buttonHtml = buttonOptions.map((name) => `<span class="chip ${buttons[name] ? "active" : ""}">${escapeHtml(name)}</span>`).join("");
  const actionHtml = (controller.actionSlots || [])
    .map((slot) => `<span class="chip ${slot.enabled ? "" : "muted"} ${slot.pressed ? "active" : ""}">${slot.index}: ${escapeHtml(slot.button || "none")} -> ${escapeHtml(slot.action)}</span>`)
    .join("");
  $("controllerInspector").innerHTML = `
    <div class="telemetry-grid compact">
      <div><span>Source</span><strong>${controller.source || "none"}</strong></div>
      <div><span>Web lock</span><strong>${controller.webDriverLocked ? "held" : "free"}</strong></div>
      <div><span>Web clients</span><strong>${controller.webClients ?? 0}</strong></div>
      <div><span>Drive invert</span><strong>${controller.driveInverted ? "on" : "off"}</strong></div>
      <div><span>Auto invert</span><strong>${controller.autoDriveInverted ? "active" : "off"}</strong></div>
      <div><span>Effective drive</span><strong>${controller.effectiveDriveInverted ? "inverted" : "normal"}</strong></div>
      <div><span>Gyro assist</span><strong>${controller.gyroAssistActive ? "active" : "off"}</strong></div>
      <div><span>Self right</span><strong>${selfRight.active ? "running" : selfRight.enabled ? selfRight.target : "off"}</strong></div>
      <div><span>Self right cool</span><strong>${selfRight.cooldownRemainingMs ?? 0} ms</strong></div>
    </div>
    <div class="axes">${axisHtml}</div>
    <div class="chips">${buttonHtml}</div>
    <div class="chips">${actionHtml}</div>
  `;
}

function renderCalibrationLiveValues(status) {
  const rows = document.querySelectorAll(".cal-axis");
  if (!rows.length) return;
  rows.forEach((row) => {
    const name = row.dataset.axis;
    const raw = Number(status.controller?.xboxRaw?.axes?.[name] ?? 0);
    const calibrated = Number(status.controller?.xboxCalibrated?.axes?.[name] ?? raw);
    const title = row.querySelector(".row-title span:last-child");
    if (title) title.textContent = `raw ${raw.toFixed(2)} / cal ${calibrated.toFixed(2)}`;
  });
}

function renderBatteryCalibration(status) {
  const battery = status.battery || {};
  const cfg = state.config?.battery || {};
  const unsupported = battery.supported === false;
  ["batteryEnabled", "batteryCalibration", "batteryWarn", "batteryCritical", "derateEnabled",
    "derateVoltage", "derateScale"].forEach(id => { $(id).disabled = unsupported; });
  if (unsupported) {
    $("batteryEnabled").checked = false;
    $("derateEnabled").checked = false;
    ["batteryAdcValue", "batteryPackValue", "batteryCellValue", "batteryDividerValue",
      "batteryCalibrationValue", "batteryThresholdValue"].forEach(id => { $(id).textContent = "N/A"; });
    $("batteryDerateState").textContent = "unavailable";
    $("batteryBenchState").textContent = battery.benchMode ? "USB bench" : "normal";
    return;
  }
  const calibration = Number(cfg.calibration ?? $("batteryCalibration")?.value ?? 1);
  const warn = Number(cfg.warnVoltage ?? $("batteryWarn")?.value ?? 7);
  const critical = Number(cfg.criticalVoltage ?? $("batteryCritical")?.value ?? 6.4);
  if ($("batteryAdcValue")) $("batteryAdcValue").textContent = `${Number(battery.adcVolts || 0).toFixed(3)} V`;
  if ($("batteryPackValue")) $("batteryPackValue").textContent = `${Number(battery.packVolts || 0).toFixed(2)} V`;
  if ($("batteryCellValue")) $("batteryCellValue").textContent = `${Number(battery.cellVolts || 0).toFixed(2)} V`;
  if ($("batteryDividerValue")) $("batteryDividerValue").textContent = `x${Number(battery.dividerMultiplier ?? 3).toFixed(2)}`;
  if ($("batteryCalibrationValue")) $("batteryCalibrationValue").textContent = `cal ${calibration.toFixed(2)}`;
  if ($("batteryThresholdValue")) $("batteryThresholdValue").textContent = `warn ${warn.toFixed(1)} V / critical ${critical.toFixed(1)} V`;
  if ($("batteryDerateState")) {
    $("batteryDerateState").textContent = battery.derating ? "active" : cfg.derateEnabled ? `armed at ${Number(cfg.derateVoltage || 0).toFixed(1)} V` : "off";
  }
  if ($("batteryBenchState")) $("batteryBenchState").textContent = battery.benchMode ? "USB bench" : "normal";
}

function updateStreamImage(imageId, blankId, camera) {
  const img = $(imageId);
  const blank = $(blankId);
  if (!img || !blank) return;
  if (camera.ready) {
    const streamUrl = `http://${location.hostname}:81/stream`;
    if (img.dataset.host !== location.hostname) {
      img.dataset.host = location.hostname;
      img.src = streamUrl;
    }
    img.style.display = "block";
    blank.style.display = "none";
  } else {
    img.removeAttribute("src");
    delete img.dataset.host;
    img.style.display = "none";
    blank.style.display = "grid";
    blank.textContent = camera.supported === false ? "Camera unavailable on this board" : (camera.lastError || "Camera stream waiting");
  }
}

function renderStatus(status) {
  state.status = status;
  const battery = status.battery || {};
  const controller = status.controller || {};
  const camera = status.camera || {};
  ["saveCameraBtn", "cameraFrameSize", "cameraQuality", "cameraBrightness", "cameraContrast",
    "cameraSaturation", "cameraHmirror", "cameraVflip"].forEach((id) => {
    $(id).disabled = camera.supported === false;
  });
  const wifi = status.wifi || {};
  const ap = wifi.ap || {};
  const sta = wifi.sta || {};
  const apSsid = ap.ssid || wifi.ssid || "AP";
  const apIp = ap.ip || wifi.ip || "";
  const staConnected = !!sta.connected;
  setBadge($("armBadge"), status.armed ? "ARMED" : "DISARMED", status.armed ? "good" : "danger");
  setBadge($("batteryBadge"), battery.supported === false ? "Battery N/A" : `${(battery.packVolts || 0).toFixed(2)} V`, battery.supported === false ? "muted" : battery.critical ? "danger" : battery.warn ? "warn" : "good");
  setBadge($("wifiBadge"), staConnected ? `LAN ${sta.ip}` : apSsid, staConnected ? "good" : "warn");
  setBadge($("pitBadge"), status.safety?.pitMode ? "PIT ON" : "PIT OFF", status.safety?.pitMode ? "warn" : "good");
  setBadge($("liveOutputBadge"), status.liveOutputEnabled ? "enabled" : "locked", status.liveOutputEnabled ? "warn" : "danger");
  const selfRight = controller.selfRight || {};
  setBadge($("selfRightBadge"), selfRight.active ? "running" : selfRight.enabled ? selfRight.target : "disabled", selfRight.active ? "warn" : selfRight.enabled ? "good" : "muted");
  bindChecked("liveOutputEnabled", status.liveOutputEnabled);
  setBadge($("scanBadge"), controller.bleScanning ? "scanning" : controller.bleScanEnabled ? "scan ready" : "scan off", controller.xboxConnected ? "good" : "warn");
  $("batteryValue").textContent = battery.supported === false ? "unavailable" : `${(battery.packVolts || 0).toFixed(2)} V`;
  renderBatteryCalibration(status);
  $("controllerValue").textContent = controller.xboxConnected ? "Xbox connected" : controller.bleScanning ? "scanning" : "not connected";
  $("sourceValue").textContent = controller.source || "none";
  $("clientValue").textContent = controller.webClients ?? ap.clients ?? wifi.clients ?? 0;
  $("networkValue").textContent = staConnected ? `${sta.ip} LAN` : `${apSsid} ${apIp}`;
  $("captiveValue").textContent = ap.captiveDns ? "ready" : "off";
  $("resetValue").textContent = status.system?.resetReason || "unknown";
  if ($("firmwareVersionValue")) $("firmwareVersionValue").textContent = status.firmwareVersion || "unknown";
  if ($("filesystemVersionValue")) $("filesystemVersionValue").textContent = status.filesystemVersion || "unknown";
  if ($("schemaVersionValue")) $("schemaVersionValue").textContent = status.schema ?? "--";
  if ($("restartPendingValue")) $("restartPendingValue").textContent = status.restartPending ? "pending" : "idle";
  setBadge($("otaVersionBadge"), status.otaInProgress ? "updating" : "ready", status.otaInProgress ? "warn" : "good");
  $("cameraValue").textContent = camera.ready
    ? `${(camera.fps || 0).toFixed(1)} fps, ${(camera.avgCaptureMs || 0).toFixed(1)} ms cap`
    : (camera.lastError || "not ready");
  $("imuValue").textContent = status.imu?.present ? `0x${Number(status.imu.address).toString(16)}` : "not fitted";
  const safetyReason = visibleSafetyReason(status);
  $("safeLine").textContent = safetyReason ? `Safe state: ${safetyReason}` : "Robot is live. Keep failsafe access clear.";
  const weapon = status.weapon || {};
  $("weaponLine").textContent = weapon.enabled
    ? `${weapon.profile || "weapon"} is ${weapon.armed ? "armed" : "safe"}${weapon.disarmReason ? `: ${weapon.disarmReason}` : ""}`
    : "Weapon output is disabled in the drive page.";
  const armLabel = status.armed ? "KILL" : "ARM";
  const armTitle = status.armed ? "Disarm immediately" : safetyReason ? `Arm blocked: ${safetyReason}` : "Arm robot";
  [$("armBtn"), $("padArmBtn")].forEach((btn) => {
    if (!btn) return;
    btn.textContent = armLabel;
    btn.title = armTitle;
    btn.classList.toggle("armed", status.armed);
  });
  $("weaponArmBtn").textContent = weapon.armed ? "Weapon Safe" : "Weapon Arm";
  $("weaponArmBtn").classList.toggle("armed", weapon.armed);
  $("pitModeBtn").textContent = status.safety?.pitMode ? "Exit Pit" : "Pit Mode";
  const ownsWebLock = controller.webDriverLocked && controller.webDriverId === state.clientId;
  if (!state.controlClaimPending) state.webControlEnabled = ownsWebLock;
  $("takeControlBtn").classList.toggle("armed", ownsWebLock);
  $("takeControlBtn").textContent = state.controlClaimPending ? "..." : ownsWebLock ? "HELD" : "TAKE";
  renderSafetyBanners(status);
  renderConfigHealth(status);
  renderSetupChecklist(status);
  renderSetupQr();
  renderSetupFlow(status);
  renderWizard(status);
  renderControllerInspector(status);
  renderControlTrainer(status);
  renderFightStatus(status);
  renderDriveWizard();
  renderCalibrationLiveValues(status);
  renderBars(status.motors || [], $("motorBars"), "motor");
  renderBars(status.servos || [], $("servoBars"), "servo");
  renderLogs(status.logs || []);
  processInputLearn(status);
  updateStreamImage("fpvStream", "fpvBlank", camera);
  updateStreamImage("fightStream", "fightBlank", camera);
  updateStreamImage("spectatorStream", "spectatorBlank", camera);
}

function linkSelects(aId, bId) {
  const a = $(aId);
  const b = $(bId);
  if (!a || !b) return;
  a.addEventListener("change", () => { b.value = a.value; });
  b.addEventListener("change", () => { a.value = b.value; });
}

function setSelectValue(id, value) {
  const el = $(id);
  if (!el || value === undefined) return;
  const stringValue = String(value);
  if ([...el.options].some((option) => option.value === stringValue)) {
    el.value = stringValue;
  }
}

function setNumberValue(id, value) {
  if ($(id) && value !== undefined) $(id).value = Number(value);
}

function setCheckedValue(id, value) {
  if ($(id) && value !== undefined) $(id).checked = !!value;
}

function applyDrivePreset(name) {
  const preset = drivePresets[name];
  if (!preset) return;
  setSelectValue("driveMode", preset.mode);
  setSelectValue("driveThrottleAxis", preset.throttleAxis);
  setSelectValue("mapThrottleAxis", preset.throttleAxis);
  setSelectValue("driveTurnAxis", preset.turnAxis);
  setSelectValue("mapTurnAxis", preset.turnAxis);
  setSelectValue("mapLeftTankAxis", preset.leftTankAxis);
  setSelectValue("mapRightTankAxis", preset.rightTankAxis);
  setSelectValue("leftMotor", preset.leftMotor);
  setSelectValue("rightMotor", preset.rightMotor);
  setSelectValue("driveInvertButton", preset.invertButton);
  setSelectValue("mapDriveInvertButton", preset.invertButton);
  setNumberValue("throttleScale", preset.throttleScale);
  setNumberValue("turnScale", preset.turnScale);
  setNumberValue("driveTurboScale", preset.turboScale);
  setNumberValue("drivePrecisionScale", preset.precisionScale);
  setNumberValue("driveGyroGain", preset.gyroGain);
  setCheckedValue("driveInvertible", preset.invertible);
  setCheckedValue("driveGyroAssist", preset.gyroAssist);
  setCheckedValue("driveAutoInvert", preset.autoInvertWithImu);
  toast(`Drive preset applied: ${preset.label}`);
}

function setRowSelect(row, selector, value) {
  const el = row?.querySelector(selector);
  if (!el || value === undefined) return;
  const stringValue = String(value);
  if ([...el.options].some((option) => option.value === stringValue)) {
    el.value = stringValue;
  }
}

function setRowNumber(row, selector, value) {
  const el = row?.querySelector(selector);
  if (el && value !== undefined) el.value = Number(value);
}

function setRowChecked(row, selector, value) {
  const el = row?.querySelector(selector);
  if (el && value !== undefined) el.checked = !!value;
}

function clearServoButtonRows(row) {
  row.querySelectorAll(".servo-button").forEach((buttonRow) => {
    setRowChecked(buttonRow, ".servoButtonEnabled", false);
    setRowSelect(buttonRow, ".servoButtonName", "");
    setRowNumber(buttonRow, ".servoButtonUs", 1500);
    setRowSelect(buttonRow, ".servoButtonMode", "momentary");
  });
}

function applyServoPreset(row, name) {
  const preset = servoPresets[name];
  if (!row || !preset) return;
  setRowChecked(row, ".servoEnabled", true);
  setRowChecked(row, ".servoAxisEnabled", preset.axisEnabled);
  setRowSelect(row, ".servoAxis", preset.axis);
  setRowChecked(row, ".servoDetach", preset.detachOnDisarm);
  if (name !== "detach") {
    setRowChecked(row, ".servoInvert", false);
    setRowNumber(row, ".servoMin", 1000);
    setRowNumber(row, ".servoNeutral", 1500);
    setRowNumber(row, ".servoMax", 2000);
    setRowNumber(row, ".servoFailsafe", 1500);
    clearServoButtonRows(row);
  }
  (preset.buttons || []).forEach((button, index) => {
    const buttonRow = row.querySelectorAll(".servo-button")[index];
    if (!buttonRow) return;
    setRowChecked(buttonRow, ".servoButtonEnabled", button.enabled);
    setRowSelect(buttonRow, ".servoButtonName", button.button);
    setRowNumber(buttonRow, ".servoButtonUs", button.us);
    setRowSelect(buttonRow, ".servoButtonMode", button.toggle ? "toggle" : "momentary");
  });
  toast(`Servo preset applied: ${preset.label}`);
}

function applyWeaponPreset(name) {
  const preset = weaponPresets[name];
  if (!preset) return;
  setCheckedValue("weaponEnabled", preset.enabled);
  if (preset.weaponType) setSelectValue("garageWeaponType", preset.weaponType);
  if (name !== "disabled") {
    setSelectValue("weaponProfile", preset.profile);
    setSelectValue("weaponInput", preset.input);
    setSelectValue("mapWeaponInput", preset.input);
    setSelectValue("weaponArmButton", preset.armButton);
    setSelectValue("mapWeaponArmButton", preset.armButton);
    setSelectValue("weaponToggle", String(!!preset.toggle));
    setNumberValue("weaponButtonPower", preset.buttonPower);
    setNumberValue("weaponMaxOutput", preset.maxOutput);
    setNumberValue("weaponRampUp", preset.rampUpPerSecond);
    setNumberValue("weaponRampDown", preset.rampDownPerSecond);
    setCheckedValue("weaponInvert", preset.invert);
    setCheckedValue("weaponRequireArm", preset.requireDedicatedArm);
  }
  toast(`Weapon preset applied: ${preset.label}`);
}

function wsSend(payload) {
  if (!state.ws || state.ws.readyState !== WebSocket.OPEN) return;
  state.ws.send(JSON.stringify(payload));
}

function sendClaimControl() {
  wsSend({ type: "claimControl", token: state.token, clientId: state.clientId });
}

function sendControlFrame() {
  if (!state.webControlEnabled || !state.token) return;
  const frame = {
    type: "control",
    token: state.token,
    clientId: state.clientId,
    leftX: state.sticks.left.x,
    leftY: state.sticks.left.y,
    rightX: state.sticks.right.x,
    rightY: state.sticks.right.y,
    leftTrigger: state.triggers.leftTrigger,
    rightTrigger: state.triggers.rightTrigger,
    buttons: state.buttons,
  };
  wsSend(frame);
}

async function takeWebControl() {
  if (!state.token && !await loginWithPrompt()) return;
  state.controlClaimPending = true;
  sendClaimControl();
  await loadStatus();
  const ownsWebLock = state.status?.controller?.webDriverId === state.clientId;
  state.webControlEnabled = ownsWebLock;
  state.controlClaimPending = false;
  toast(ownsWebLock ? "Web control claimed" : "Web control is held elsewhere");
}

async function releaseWebControl({ quiet = false } = {}) {
  state.webControlEnabled = false;
  state.controlClaimPending = false;
  wsSend({ type: "releaseControl", token: state.token, clientId: state.clientId });
  try {
    if (state.token) await api("/api/control/release", { method: "POST", timeoutMs: 1200 });
  } catch (err) {
  }
  if (!quiet) {
    await loadStatus();
    toast("Web control released");
  }
}

function releaseWebControlSoon() {
  if (!state.webControlEnabled && !state.controlClaimPending) return;
  state.webControlEnabled = false;
  state.controlClaimPending = false;
  wsSend({ type: "releaseControl", token: state.token, clientId: state.clientId });
  if (navigator.sendBeacon && state.token) {
    const blob = new Blob(["{}"], { type: "application/json" });
    navigator.sendBeacon("/api/control/release", blob);
  }
}

function setupSticks() {
  document.querySelectorAll(".stick").forEach((stick) => {
    const nub = stick.querySelector("span");
    const which = stick.dataset.stick;
    const reset = () => {
      state.sticks[which] = { x: 0, y: 0 };
      nub.style.transform = "translate(0px, 0px)";
    };
    const update = (ev) => {
      const rect = stick.getBoundingClientRect();
      const radius = rect.width / 2;
      const limit = Math.max(1, radius - (nub.offsetWidth / 2));
      const dx = ev.clientX - (rect.left + radius);
      const dy = ev.clientY - (rect.top + radius);
      const mag = Math.min(limit, Math.hypot(dx, dy));
      const angle = Math.atan2(dy, dx);
      const x = Math.cos(angle) * mag;
      const y = Math.sin(angle) * mag;
      nub.style.transform = `translate(${x}px, ${y}px)`;
      state.sticks[which] = {
        x: Math.max(-1, Math.min(1, x / limit)),
        y: Math.max(-1, Math.min(1, -y / limit)),
      };
    };
    stick.addEventListener("pointerdown", (ev) => {
      capturePointer(stick, ev.pointerId);
      update(ev);
    });
    stick.addEventListener("pointermove", (ev) => {
      if (stick.hasPointerCapture(ev.pointerId)) update(ev);
    });
    stick.addEventListener("pointerup", (ev) => {
      releasePointer(stick, ev.pointerId);
      reset();
    });
    stick.addEventListener("pointercancel", reset);
    stick.addEventListener("lostpointercapture", reset);
  });

  document.querySelectorAll("[data-pad]").forEach((btn) => {
    const name = btn.dataset.pad;
    const on = (ev) => {
      capturePointer(btn, ev.pointerId);
      state.buttons[name] = true;
    };
    const off = (ev) => {
      releasePointer(btn, ev.pointerId);
      state.buttons[name] = false;
    };
    btn.addEventListener("pointerdown", on);
    btn.addEventListener("pointerup", off);
    btn.addEventListener("pointercancel", off);
    btn.addEventListener("lostpointercapture", () => { state.buttons[name] = false; });
  });

  document.querySelectorAll("[data-trigger]").forEach((slider) => {
    const name = slider.dataset.trigger;
    const update = () => {
      state.triggers[name] = Math.max(0, Math.min(1, Number(slider.value) || 0));
    };
    const reset = () => {
      slider.value = 0;
      state.triggers[name] = 0;
    };
    slider.addEventListener("input", update);
    slider.addEventListener("change", update);
    slider.addEventListener("pointerup", reset);
    slider.addEventListener("pointercancel", reset);
    slider.addEventListener("lostpointercapture", reset);
    slider.addEventListener("blur", reset);
  });
}

function uploadFile(inputId, url) {
  const file = $(inputId).files[0];
  if (!file) {
    toast("Choose a .bin file first");
    return Promise.resolve(false);
  }
  if (!confirm(`Upload ${file.name} and reboot the Ant Core?`)) return Promise.resolve(false);
  return (async () => {
    if (!state.token && !await loginWithPrompt()) {
      return false;
    }
    const body = new FormData();
    body.append("file", file, file.name);
    return await new Promise((resolve, reject) => {
      const xhr = new XMLHttpRequest();
      xhr.open("POST", url);
      xhr.setRequestHeader("X-AntCore-Token", state.token);
      xhr.upload.onprogress = (event) => {
        if (event.lengthComputable) toast(`OTA ${Math.round((event.loaded / event.total) * 100)}%`);
      };
      xhr.onload = () => {
        try {
          const result = JSON.parse(xhr.responseText || "{}");
          if (xhr.status >= 200 && xhr.status < 300) {
            toast(result.message || "Upload complete");
            resolve(true);
          } else {
            reject(new Error(result.reason || result.error || result.message || xhr.statusText));
          }
        } catch (err) {
          reject(err);
        }
      };
      xhr.onerror = () => reject(new Error("network error"));
      xhr.send(body);
    });
  })();
}

function downloadText(filename, text, type = "application/json") {
  const blob = new Blob([text], { type });
  const url = URL.createObjectURL(blob);
  const a = document.createElement("a");
  a.href = url;
  a.download = filename;
  document.body.appendChild(a);
  a.click();
  a.remove();
  URL.revokeObjectURL(url);
}

async function exportConfigSnapshot() {
  const cfg = await api("/api/config/export", { auth: false });
  downloadText(`antcore-${cfg.robotName || "config"}.json`, JSON.stringify(cfg, null, 2));
  state.setupAck.exportBackup = true;
  saveSetupAck();
  if (state.status) renderSetupFlow(state.status);
  toast("Configuration exported");
}

function renderTimer() {
  const seconds = Math.max(0, Math.ceil(state.timer.remaining));
  const mm = String(Math.floor(seconds / 60)).padStart(2, "0");
  const ss = String(seconds % 60).padStart(2, "0");
  const text = `${mm}:${ss}`;
  ["matchTimer", "fightTimer", "spectatorTimer"].forEach((id) => {
    if ($(id)) $(id).textContent = text;
  });
  if ($("fightCountdown")) {
    const remaining = Math.ceil((state.fight.countdownUntil - performance.now()) / 1000);
    $("fightCountdown").textContent = remaining > 0 ? String(remaining) : (state.fight.active ? "FIGHT" : "READY");
  }
}

function tickTimer() {
  if (!state.timer.running) {
    renderTimer();
    return;
  }
  const now = performance.now();
  if (state.fight.countdownUntil && now < state.fight.countdownUntil) {
    state.timer.lastTick = 0;
    renderTimer();
    return;
  }
  if (state.fight.countdownUntil && now >= state.fight.countdownUntil) {
    state.fight.countdownUntil = 0;
    state.timer.lastTick = now;
    renderTimer();
    return;
  }
  const elapsed = state.timer.lastTick ? (now - state.timer.lastTick) / 1000 : 0;
  state.timer.lastTick = now;
  state.timer.remaining = Math.max(0, state.timer.remaining - elapsed);
  if (state.timer.remaining <= 0) {
    state.timer.running = false;
    toast("Match timer complete");
  }
  renderTimer();
}

async function importConfig() {
  const file = $("importConfigFile").files[0];
  if (!file) {
    toast("Choose a config JSON file first");
    return;
  }
  const imported = JSON.parse(await file.text());
  if (!confirm("Import this configuration and disarm the robot?")) return;
  await api("/api/config", {
    method: "PUT",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(imported),
  });
  await loadConfig();
  await loadStatus();
  toast("Configuration imported");
}

async function saveNamedProfile() {
  const name = $("activeProfile").value.trim();
  if (!name) {
    toast("Enter a profile name first");
    return;
  }
  if (!await saveConfig()) return;
  const result = await api("/api/profiles/save", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ name }),
  });
  await loadProfiles();
  toast(`Profile saved: ${result.name || name}`);
}

async function loadNamedProfile() {
  const name = selectedProfileName();
  if (!name) {
    toast("Choose a saved profile first");
    return;
  }
  if (!confirm(`Load profile "${name}" and disarm the robot?`)) return;
  const result = await api("/api/profiles/load", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ name }),
  });
  await loadConfig();
  await loadProfiles();
  await loadPacks();
  await loadStatus();
  toast(`Profile loaded: ${result.name || name}`);
}

async function deleteNamedProfile() {
  const name = selectedProfileName();
  if (!name) {
    toast("Choose a saved profile first");
    return;
  }
  if (!confirm(`Delete profile "${name}" from this Ant Core?`)) return;
  const result = await api("/api/profiles/delete", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ name }),
  });
  await loadProfiles();
  toast(`Profile deleted: ${result.name || name}`);
}

function applyDriveDirectionWizard() {
  const cfg = state.config || {};
  const leftIndex = Math.max(0, Number($("leftMotor").value || cfg.drive?.leftMotor || 1) - 1);
  const rightIndex = Math.max(0, Number($("rightMotor").value || cfg.drive?.rightMotor || 2) - 1);
  const motorRows = [...document.querySelectorAll("[data-motor]")];
  if (motorRows[leftIndex]) motorRows[leftIndex].querySelector(".motorInvert").checked = $("leftWheelForward").value === "no";
  if (motorRows[rightIndex]) motorRows[rightIndex].querySelector(".motorInvert").checked = $("rightWheelForward").value === "no";
  state.setupAck.driveDirection = true;
  saveSetupAck();
  renderDriveWizard();
  if (state.status) renderSetupFlow(state.status);
  toast("Drive direction inversion applied locally");
}

async function savePack() {
  const payload = {
    id: state.selectedPackId,
    name: $("packName").value.trim(),
    chargeVoltage: Number($("packChargeVoltage").value || 8.4),
    weak: $("packWeak").checked,
    cycles: Number($("packCycles").value || 0),
    notes: $("packNotes").value.trim(),
  };
  if (!payload.name) {
    toast("Enter a pack name first");
    return;
  }
  const result = await api("/api/packs/save", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify(payload),
  });
  state.selectedPackId = result.id || state.selectedPackId;
  localStorage.setItem("antcoreSelectedPackId", state.selectedPackId);
  await loadPacks();
  toast(result.message || "Pack saved");
}

async function deletePack() {
  const pack = selectedPack();
  if (!pack) {
    toast("Choose a pack first");
    return;
  }
  if (!confirm(`Delete battery pack "${pack.name || pack.id}"?`)) return;
  const result = await api("/api/packs/delete", {
    method: "POST",
    headers: { "content-type": "application/json" },
    body: JSON.stringify({ id: pack.id }),
  });
  state.selectedPackId = "";
  localStorage.removeItem("antcoreSelectedPackId");
  await loadPacks();
  toast(result.message || "Pack deleted");
}

function setupEvents() {
  [["fpvStream", "fpvBlank"], ["fightStream", "fightBlank"], ["spectatorStream", "spectatorBlank"]].forEach(([imageId, blankId]) => {
    $(imageId).addEventListener("error", () => {
      const fpv = $(imageId);
      const blank = $(blankId);
      if (!fpv || !blank) return;
      delete fpv.dataset.host;
      fpv.removeAttribute("src");
      fpv.style.display = "none";
      blank.textContent = "Camera stream unavailable";
      blank.style.display = "grid";
    });
  });
  document.addEventListener("visibilitychange", () => {
    if (document.hidden) releaseWebControlSoon();
  });
  window.addEventListener("pagehide", releaseWebControlSoon);
  document.querySelectorAll(".tab").forEach((tab) => {
    tab.addEventListener("click", () => {
      document.querySelectorAll(".tab").forEach((el) => el.classList.remove("active"));
      document.querySelectorAll(".page").forEach((el) => el.classList.remove("active"));
      tab.classList.add("active");
      $(tab.dataset.tab).classList.add("active");
      document.body.classList.toggle("fight-mode", tab.dataset.tab === "fight");
    });
  });
  $("loginBtn").addEventListener("click", () => state.token ? logout() : loginWithPrompt());
  $("armBtn").addEventListener("click", (ev) => toggleRobotArm(ev.currentTarget));
  $("padArmBtn").addEventListener("click", (ev) => toggleRobotArm(ev.currentTarget));
  $("weaponArmBtn").addEventListener("click", (ev) => toggleWeaponArm(ev.currentTarget));
  $("pitModeBtn").addEventListener("click", (ev) => togglePitMode(ev.currentTarget));
  $("takeControlBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, takeWebControl, "Take control failed"));
  $("releaseControlBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, releaseWebControl, "Release control failed"));
  $("setLiveOutputBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, setLiveOutputFromUi, "Live output toggle failed"));
  $("runMotorTestBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, runMotorTest, "Motor test failed"));
  $("runServoTestBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, runServoTest, "Servo test failed"));
  document.querySelectorAll("[data-drive-preset]").forEach((btn) => {
    btn.addEventListener("click", () => applyDrivePreset(btn.dataset.drivePreset));
  });
  document.querySelectorAll("[data-weapon-preset]").forEach((btn) => {
    btn.addEventListener("click", () => applyWeaponPreset(btn.dataset.weaponPreset));
  });
  $("saveCalibrationBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, saveCalibration, "Calibration save failed"));
  $("captureCenterBtn").addEventListener("click", captureCalibrationCenters);
  $("resetCalibrationBtn").addEventListener("click", () => {
    if (confirm("Reset all Xbox axis calibration values?")) resetCalibration();
  });
  $("disarmBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    await api("/api/disarm", { method: "POST", auth: false, timeoutMs: 1200 });
    await loadStatus();
    toast("Robot disarmed");
  }, "Disarm failed"));
  $("fightDisarmBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    await api("/api/disarm", { method: "POST", auth: false, timeoutMs: 1200 });
    await loadStatus();
    finishFightSession(state.status);
    toast("Emergency disarm sent");
  }, "Emergency disarm failed"));
  $("fightExitBtn").addEventListener("click", () => {
    document.body.classList.remove("fight-mode");
    gotoTab("dashboard");
  });
  $("fightStartBtn").addEventListener("click", () => startFightSession(false));
  $("fightCountdownBtn").addEventListener("click", () => startFightSession(true));
  $("readyBoutBtn").addEventListener("click", () => {
    const ready = renderPreFightChecklist(state.status);
    toast(ready ? "Ready for bout" : "Checklist still has blockers");
  });
  $("refreshBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    await loadStatus();
    toast("Status refreshed");
  }, "Refresh failed"));
  $("scanBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    await api("/api/ble/scan", { method: "POST" });
    await loadStatus();
    toast("BLE scan requested");
  }, "BLE scan failed"));
  $("forgetBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    if (!confirm("Forget all BLE controller bonds?")) return;
    await api("/api/ble/forget", { method: "POST" });
    await loadStatus();
    toast("Controller bonds cleared");
  }, "BLE forget failed"));
  ["saveDriveBtn", "saveServosBtn", "saveBatteryBtn", "saveWifiBtn", "saveSecurityBtn", "saveSafetyBtn", "saveMappingBtn", "saveCameraBtn"].forEach((id) => {
    $(id).addEventListener("click", (ev) => runAction(ev.currentTarget, saveConfig, "Save failed"));
  });
  $("timerStartBtn").addEventListener("click", () => {
    if (state.timer.remaining <= 0) state.timer.remaining = Number($("timerDuration").value || 120);
    state.timer.running = true;
    state.timer.lastTick = performance.now();
    renderTimer();
  });
  $("timerPauseBtn").addEventListener("click", () => {
    state.timer.running = false;
    renderTimer();
  });
  $("timerResetBtn").addEventListener("click", () => {
    state.timer.running = false;
    state.timer.remaining = Number($("timerDuration").value || 120);
    renderTimer();
  });
  $("timerDuration").addEventListener("change", () => {
    if (!state.timer.running) {
      state.timer.remaining = Number($("timerDuration").value || 120);
      renderTimer();
    }
  });
  $("resetConfigBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    if (!confirm("Factory reset all Ant Core configuration?")) return;
    await api("/api/config/reset", { method: "POST" });
    await loadConfig();
    await loadStatus();
    toast("Factory defaults restored");
  }, "Factory reset failed"));
  $("clearLogsBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    if (!confirm("Clear the diagnostic log?")) return;
    await api("/api/logs/clear", { method: "POST" });
    await loadStatus();
    toast("Logs cleared");
  }, "Clear log failed"));
  $("exportConfigBtn").addEventListener("click", () => exportConfigSnapshot().catch((err) => toast(`Export failed: ${err.message}`)));
  $("setupExportBtn").addEventListener("click", () => exportConfigSnapshot().catch((err) => toast(`Export failed: ${err.message}`)));
  $("setupNextBtn").addEventListener("click", () => {
    const step = state.setupNextStep;
    gotoTab(step ? step.tab : "dashboard");
    toast(step ? `Opened ${step.label}` : "Setup complete");
  });
  $("setupProfilesBtn").addEventListener("click", () => gotoTab("diagnostics"));
  $("setupPitShortcutBtn").addEventListener("click", (ev) => togglePitMode(ev.currentTarget));
  $("saveGarageBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, saveConfig, "Garage save failed"));
  $("robotName").addEventListener("input", () => {
    $("garageRobotName").value = $("robotName").value;
    renderGarage();
  });
  $("activeProfile").addEventListener("input", () => {
    $("garageProfileName").value = $("activeProfile").value;
    renderGarage();
  });
  ["garageRobotName", "garageProfileName", "garageBotType", "garageWeaponType", "garageAccent", "garageAvatar", "garageNotes"].forEach((id) => {
    $(id).addEventListener("input", () => {
      if (id === "garageRobotName") $("robotName").value = $("garageRobotName").value;
      if (id === "garageProfileName") $("activeProfile").value = $("garageProfileName").value;
      renderGarage();
    });
    $(id).addEventListener("change", renderGarage);
  });
  $("driveWizardApplyBtn").addEventListener("click", applyDriveDirectionWizard);
  $("driveWizardSaveBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    applyDriveDirectionWizard();
    await saveConfig();
  }, "Drive direction save failed"));
  $("packSelect").addEventListener("change", () => {
    state.selectedPackId = $("packSelect").value;
    localStorage.setItem("antcoreSelectedPackId", state.selectedPackId);
    loadPackIntoForm(selectedPack());
  });
  $("newPackBtn").addEventListener("click", clearPackForm);
  $("savePackBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, savePack, "Pack save failed"));
  $("deletePackBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, deletePack, "Pack delete failed"));
  ["packChargeVoltage", "packWeak", "packCycles", "packNotes"].forEach((id) => {
    $(id).addEventListener("input", () => renderPackList());
    $(id).addEventListener("change", () => renderPackList());
  });
  $("exportLogsBtn").addEventListener("click", async () => {
    const logs = await api("/api/logs");
    downloadText("antcore-log.txt", (logs.logs || []).join("\n"), "text/plain");
    toast("Log exported");
  });
  $("exportBlackboxBtn").addEventListener("click", async () => {
    const log = await api("/api/blackbox");
    downloadText("antcore-blackbox.log", typeof log === "string" ? log : "", "text/plain");
    toast("Blackbox exported");
  });
  $("clearBlackboxBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    if (!confirm("Clear the persistent blackbox log?")) return;
    await api("/api/blackbox/clear", { method: "POST" });
    await loadStatus();
    toast("Blackbox cleared");
  }, "Blackbox clear failed"));
  $("importConfigBtn").addEventListener("click", (ev) => {
    if (!$("importConfigFile").files[0]) {
      $("importConfigFile").click();
      return;
    }
    runAction(ev.currentTarget, importConfig, "Import failed");
  });
  $("importConfigFile").addEventListener("change", () => {
    if ($("importConfigFile").files[0]) importConfig().catch((err) => toast(`Import failed: ${err.message}`));
  });
  $("saveProfileBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, saveNamedProfile, "Profile save failed"));
  $("loadProfileBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, loadNamedProfile, "Profile load failed"));
  $("deleteProfileBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, deleteNamedProfile, "Profile delete failed"));
  $("refreshProfilesBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, async () => {
    await loadProfiles();
    toast("Profiles refreshed");
  }, "Profile refresh failed"));
  $("firmwareUploadBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, () => uploadFile("firmwareFile", "/api/ota/firmware"), "Firmware upload failed"));
  $("fsUploadBtn").addEventListener("click", (ev) => runAction(ev.currentTarget, () => uploadFile("fsFile", "/api/ota/filesystem"), "Filesystem upload failed"));
}

function renderControlChips() {
  $("controlChips").innerHTML = inputOptions.map((name) => `<span class="chip">${name}</span>`).join("");
  ["leftMotor", "rightMotor"].forEach((id) => {
    $(id).innerHTML = motorSelectOptions(1);
  });
  ["driveInvertButton", "driveTurboButton", "drivePrecisionButton", "weaponArmButton", "mapArmButton", "mapWeaponArmButton", "mapDriveInvertButton", "mapTurboButton", "mapPrecisionButton"].forEach((id) => {
    $(id).innerHTML = optionList(buttonOptions, "");
  });
  ["driveThrottleAxis", "driveTurnAxis", "mapThrottleAxis", "mapTurnAxis", "mapLeftTankAxis", "mapRightTankAxis"].forEach((id) => {
    $(id).innerHTML = optionList(axisOptions, "");
  });
  $("mapWeaponInput").innerHTML = optionList(inputOptions, "");
}

async function boot() {
  renderAuth();
  renderControlChips();
  setupEvents();
  linkSelects("driveInvertButton", "mapDriveInvertButton");
  linkSelects("driveTurboButton", "mapTurboButton");
  linkSelects("drivePrecisionButton", "mapPrecisionButton");
  linkSelects("driveThrottleAxis", "mapThrottleAxis");
  linkSelects("driveTurnAxis", "mapTurnAxis");
  linkSelects("weaponArmButton", "mapWeaponArmButton");
  linkSelects("weaponInput", "mapWeaponInput");
  setupSticks();
  connectWs();
  await loadConfig();
  await loadProfiles();
  await loadPacks();
  await loadStatus();
  setInterval(sendControlFrame, 33);
  setInterval(loadStatus, 4000);
  setInterval(tickTimer, 250);
  renderTimer();
}

boot().catch((err) => toast(err.message));
