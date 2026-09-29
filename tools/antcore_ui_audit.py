"""Headless browser audit for the hosted Ant Core UI.

The default mock mode verifies button wiring and dashboard rendering without
touching the robot. Use --url with the live ESP address for a lightweight
served-asset/layout audit.
"""

from __future__ import annotations

import argparse
import functools
import http.server
import json
import shutil
import subprocess
import tempfile
import threading
import time
import urllib.request
from pathlib import Path
from typing import Any

import websocket


ROOT = Path(__file__).resolve().parents[1]
DATA_DIR = ROOT / "data"
OUT_DIR = ROOT / ".pio" / "layout-check"


MOCK_SCRIPT = r"""
(() => {
  localStorage.setItem('antcoreToken', 'mock-token');
  localStorage.setItem('antcoreClientId', 'mock-client-0001');
  window.confirm = () => true;
  window.prompt = () => 'antcore';
  const status = {
    type: 'status', schema: 11, firmwareVersion: '1.1.0-modular', filesystemVersion: '2026-09-14-modular', robotName: 'Ant Core', activeProfile: 'Default', uptimeMs: 1000,
    armed: false, disarmReason: 'battery critical', otaInProgress: false,
    restartPending: false, liveOutputEnabled: false, driveInverted: false,
    garage: { botType: 'skid', weaponType: 'pusher', accent: '#ffca4f', avatar: 'ant', notes: 'test robot' },
    events: { controlDisconnects: 1, failsafes: 2, weaponArms: 3, disarms: 4 },
    system: { resetReason: 'software reset', freeHeap: 230000, minFreeHeap: 210000, freePsram: 7600000 },
    security: { authEnabled: true, sessionActive: true, defaultPin: true, authFailures: 0, webClients: 1 },
    wifi: {
      mdns: 'antcore.local',
      mode: 'ap_sta', staEnabled: true, ssid: 'AntCore-BACC',
      ip: '192.168.4.1', clients: 1,
      ap: { ssid: 'AntCore-BACC', ip: '192.168.4.1', clients: 1, captiveDns: true },
      sta: { enabled: true, ssid: 'HomeWiFi', connected: true, ip: '192.168.0.97', rssi: -42, everConnected: true }
    },
    controller: { xboxConnected: false, xboxConnecting: false, bleScanEnabled: true, bleScanning: false, armButton: 'menu', xboxArmEnabled: true, source: 'none', webDriverLocked: false, webDriverId: '', webClients: 1, driveInverted: false, autoDriveInverted: false, effectiveDriveInverted: false, gyroAssistActive: false, selfRight: { enabled: true, target: 'servo1', active: false, cooldownRemainingMs: 0 }, calibration: { enabled: true, mappingTestMode: false, axes: [] }, actionSlots: [{ index: 1, enabled: true, button: 'b', action: 'robotDisarm', pressed: false }, { index: 2, enabled: true, button: 'y', action: 'selfRight', pressed: false }, { index: 3, enabled: false, button: '', action: 'none', pressed: false }, { index: 4, enabled: false, button: '', action: 'none', pressed: false }], xboxRaw: { axes: {}, buttons: {} }, xboxCalibrated: { axes: {}, buttons: {} }, active: { axes: {}, buttons: {} } },
    battery: { enabled: true, adcVolts: 0, packVolts: 0, cellVolts: 0, warn: false, critical: true, benchMode: false, derating: false },
    imu: { present: false, address: 0, ax: 0, ay: 0, az: 0, gx: 0, gy: 0, gz: 0 },
    camera: { ready: true, frameSize: 'qvga', jpegQuality: 18, brightness: 0, contrast: 0, saturation: 0, hmirror: false, vflip: false, streamUrl: '/stream', snapshotUrl: '/snapshot.jpg', lastError: '', fps: 52.1, avgCaptureMs: 14.2 },
    weapon: { enabled: false, armed: false, profile: 'brushed', requireDedicatedArm: true, disarmReason: 'boot', target: 0, profileOutput: 0, rampUpPerSecond: 3, rampDownPerSecond: 8 },
    safety: { canArm: false, armBlockReason: 'battery critical', pitMode: false, mappingTestMode: false, requireControlSource: true, benchMode: false, batteryCritical: true, batteryWarn: false, otaInProgress: false, activeControlAvailable: false, defaultAdminPin: true, configValid: true, config: { valid: true, errorCount: 0, warningCount: 1, errors: [], warnings: ['default admin PIN is still active'] } },
    motors: [{ index: 1, target: 0, output: 0, pwmA: 0, pwmB: 0, pwmMax: 1023, override: 'disarmed' }, { index: 2, target: 0, output: 0, pwmA: 0, pwmB: 0, pwmMax: 1023, override: 'disarmed' }, { index: 3, target: 0, output: 0, pwmA: 0, pwmB: 0, pwmMax: 1023, override: 'disarmed' }],
    servos: [{ index: 1, attached: true, us: 1500, failsafeUs: 1500, neutralUs: 1500, minUs: 1000, maxUs: 2000, override: 'failsafe', buttonToggles: [{ index: 2, button: 'y', latched: false }] }, { index: 2, attached: true, us: 1500, failsafeUs: 1500, neutralUs: 1500, minUs: 1000, maxUs: 2000, override: 'failsafe', buttonToggles: [] }],
    logs: ['1 INFO boot']
  };
  window.__mockStatus = status;
  const config = {
    schema: 11, robotName: 'Ant Core', activeProfile: 'Default', apPassword: '', apPasswordSet: true,
    garage: { botType: 'skid', weaponType: 'pusher', accent: '#ffca4f', avatar: 'ant', notes: 'test robot' },
    security: { authEnabled: true, adminPin: '', adminPinSet: true, defaultPin: true },
    safety: { pitMode: false, requireControlSource: true },
    control: { xboxArmEnabled: true, armButton: 'menu', calibration: { enabled: true, mappingTestMode: false, axes: ['leftX', 'leftY', 'rightX', 'rightY', 'leftTrigger', 'rightTrigger'].map((name) => ({ name, min: name.includes('Trigger') ? 0 : -1, center: 0, max: 1, deadband: 0.03, invert: false, positiveOnly: name.includes('Trigger') })) }, selfRight: { enabled: true, target: 'servo1', servoUs: 2000, motorPower: 1, durationMs: 550, cooldownMs: 1500, requireWeaponArm: true }, actions: [{ index: 1, enabled: true, button: 'b', action: 'robotDisarm' }, { index: 2, enabled: true, button: 'y', action: 'selfRight' }, { index: 3, enabled: false, button: '', action: 'none' }, { index: 4, enabled: false, button: '', action: 'none' }] },
    camera: { frameSize: 'qvga', jpegQuality: 18, brightness: 0, contrast: 0, saturation: 0, hmirror: false, vflip: false },
    wifi: { staEnabled: true, staSsid: 'HomeWiFi', staPassword: '', staPasswordSet: true },
    battery: { enabled: true, benchMode: false, calibration: 1, warnVoltage: 7, criticalVoltage: 6.4, derateEnabled: false, derateVoltage: 6.8, derateScale: 0.7 },
    drive: { mode: 'arcade', throttleAxis: 'leftY', turnAxis: 'leftX', leftTankAxis: 'leftY', rightTankAxis: 'rightY', deadband: 0.07, expo: 0.25, throttleScale: 1, turnScale: 1, leftMotor: 1, rightMotor: 2, invertible: true, invertButton: 'view', turboButton: 'rightStickButton', precisionButton: 'leftStickButton', turboScale: 1, precisionScale: 0.45, gyroAssist: false, gyroGain: 0.015, autoInvertWithImu: true, autoInvertAzThreshold: -0.45 },
    motors: [1, 2, 3].map((n) => ({ index: n, pinA: n * 2 - 1, pinB: n * 2, invert: false, trim: 0, maxOutput: 1, rampPerSecond: 6 })),
    weapon: { enabled: false, motor: 3, profile: 'brushed', input: 'rightTrigger', armButton: 'x', invert: false, toggle: false, requireDedicatedArm: true, buttonPower: 1, maxOutput: 1, rampUpPerSecond: 3, rampDownPerSecond: 8 },
    servos: [
      { index: 1, pin: 43, enabled: true, axisEnabled: true, axis: 'rightY', invert: false, minUs: 1000, neutralUs: 1500, maxUs: 2000, failsafeUs: 1500, detachOnDisarm: false, buttons: [{ enabled: true, button: 'a', us: 1000, toggle: false }, { enabled: true, button: 'y', us: 2000, toggle: true }, { enabled: false, button: '', us: 1500, toggle: false }, { enabled: false, button: '', us: 1500, toggle: false }] },
      { index: 2, pin: 44, enabled: true, axisEnabled: false, axis: 'rightX', invert: false, minUs: 1000, neutralUs: 1500, maxUs: 2000, failsafeUs: 1500, detachOnDisarm: false, buttons: [{ enabled: true, button: 'leftBumper', us: 1000, toggle: false }, { enabled: true, button: 'rightBumper', us: 2000, toggle: false }, { enabled: false, button: '', us: 1500, toggle: false }, { enabled: false, button: '', us: 1500, toggle: false }] }
    ]
  };
  window.__mockConfig = config;
  let profiles = [{ name: 'Default', path: '/profiles/default.json', size: 2048 }];
  let packs = [{ id: 'main-pack', name: 'Main Pack', chargeVoltage: 8.4, weak: false, cycles: 4, notes: 'fresh' }];
  window.__requests = [];
  window.fetch = async (path, options = {}) => {
    const cleanPath = new URL(String(path), location.href).pathname;
    const payload = options.body ? JSON.parse(options.body) : {};
    window.__requests.push({ path: cleanPath, method: options.method || 'GET' });
    let code = 200;
    let body = { ok: true };
    let contentType = 'application/json';
    if (cleanPath === '/api/status') body = status;
    else if (cleanPath === '/api/auth/login') {
      body = { ok: true, token: 'mock-token', defaultPin: true };
    } else if (cleanPath === '/api/config/export') {
      body = config;
    } else if (cleanPath === '/api/config') {
      if ((options.method || 'GET') === 'PUT') {
        Object.keys(config).forEach((key) => delete config[key]);
        Object.assign(config, JSON.parse(JSON.stringify(payload)));
        body = { ok: true, message: 'config saved' };
      } else {
        body = config;
      }
    } else if (cleanPath === '/api/profiles') {
      body = { ok: true, activeProfile: config.activeProfile, profiles };
    } else if (cleanPath === '/api/profiles/save') {
      const name = payload.name || config.activeProfile || 'Default';
      profiles = profiles.filter((profile) => profile.name !== name);
      profiles.push({ name, path: `/profiles/${name.toLowerCase()}.json`, size: 2048 });
      body = { ok: true, name, message: 'profile saved' };
    } else if (cleanPath === '/api/profiles/load') {
      config.activeProfile = payload.name || 'Default';
      body = { ok: true, name: config.activeProfile, message: 'profile loaded' };
    } else if (cleanPath === '/api/profiles/delete') {
      const name = payload.name || 'Default';
      profiles = profiles.filter((profile) => profile.name !== name);
      body = { ok: true, name, message: 'profile deleted' };
    } else if (cleanPath === '/api/packs') {
      body = { ok: true, packs };
    } else if (cleanPath === '/api/packs/save') {
      const id = payload.id || (payload.name || 'pack').toLowerCase().replace(/[^a-z0-9]+/g, '-').replace(/^-|-$/g, '');
      packs = packs.filter((pack) => pack.id !== id);
      packs.push({ id, name: payload.name || 'Pack', chargeVoltage: payload.chargeVoltage || 8.4, weak: !!payload.weak, cycles: payload.cycles || 0, notes: payload.notes || '' });
      body = { ok: true, id, message: 'pack saved' };
    } else if (cleanPath === '/api/packs/delete') {
      const id = payload.id || 'main-pack';
      packs = packs.filter((pack) => pack.id !== id);
      body = { ok: true, id, message: 'pack deleted' };
    } else if (cleanPath === '/api/arm') {
      code = 409;
      body = { ok: false, armed: false, reason: 'battery critical' };
    } else if (cleanPath === '/api/disarm') {
      status.armed = false;
      status.disarmReason = 'web';
      body = { ok: true, armed: false };
    } else if (cleanPath === '/api/test/live-output') {
      status.liveOutputEnabled = !!payload.enabled;
      body = { ok: true, liveOutputEnabled: status.liveOutputEnabled, message: status.liveOutputEnabled ? 'live output tests enabled' : 'live output tests disabled' };
    } else if (cleanPath === '/api/test/motor') {
      body = { ok: true, message: 'motor test queued', motor: payload.motor || 1 };
    } else if (cleanPath === '/api/test/servo') {
      body = { ok: true, message: 'servo test queued', servo: payload.servo || 1 };
    } else if (cleanPath === '/api/pit') {
      status.safety.pitMode = !!payload.enabled;
      if (status.safety.pitMode) status.liveOutputEnabled = false;
      config.safety.pitMode = !!payload.enabled;
      body = { ok: true, pitMode: status.safety.pitMode, message: status.safety.pitMode ? 'pit mode enabled' : 'pit mode disabled' };
    } else if (cleanPath === '/api/ble/scan') {
      status.controller.bleScanning = true;
      body = { ok: true, scanEnabled: true };
    } else if (cleanPath === '/api/ble/forget') {
      body = { ok: true, message: 'BLE bonds cleared' };
    } else if (cleanPath === '/api/config/reset') {
      body = { ok: true, message: 'defaults restored' };
    } else if (cleanPath === '/api/weapon/arm') {
      code = 409;
      body = { ok: false, reason: 'robot disarmed' };
    } else if (cleanPath === '/api/logs') {
      body = { logs: status.logs, blackboxBytes: 128, blackboxAvailable: true };
    } else if (cleanPath === '/api/logs/clear') {
      status.logs = [];
      body = { ok: true, message: 'logs cleared' };
    } else if (cleanPath === '/api/blackbox') {
      contentType = 'text/plain';
      body = '1 INFO boot\\n2 WARN battery critical\\n';
    } else if (cleanPath === '/api/blackbox/clear') {
      body = { ok: true, message: 'blackbox log cleared' };
    } else {
      body = { ok: true, message: 'upload complete' };
    }
    return new Response(typeof body === 'string' ? body : JSON.stringify(body), { status: code, headers: { 'content-type': contentType } });
  };
  class MockWebSocket {
    constructor(url) {
      this.url = url;
      this.readyState = 1;
      window.__ws = this;
      this.sent = [];
      setTimeout(() => this.onopen?.(), 0);
    }
    send(payload) {
      const message = JSON.parse(payload);
      this.sent.push(message);
      if (message.type === 'claimControl') {
        status.controller.webDriverLocked = true;
        status.controller.webDriverId = message.clientId || '';
        setTimeout(() => this.onmessage?.({ data: JSON.stringify({ type: 'controlClaim', ok: true, webDriverId: status.controller.webDriverId }) }), 0);
      }
      if (message.type === 'releaseControl') {
        status.controller.webDriverLocked = false;
        status.controller.webDriverId = '';
      }
    }
    close() { this.readyState = 3; this.onclose?.(); }
  }
  MockWebSocket.OPEN = 1;
  window.WebSocket = MockWebSocket;
})();
"""


BUTTON_AUDIT = r"""
(async () => {
  const sleep = (ms) => new Promise((resolve) => setTimeout(resolve, ms));
  const text = (id) => document.getElementById(id)?.textContent || '';
  const click = async (id) => {
    document.getElementById(id).click();
    await sleep(220);
    return text('toast');
  };
  const result = {
    initial: {
      armBadge: text('armBadge'), batteryBadge: text('batteryBadge'), wifiBadge: text('wifiBadge'),
      source: text('sourceValue'), controller: text('controllerValue'), battery: text('batteryValue'),
      camera: text('cameraValue'), imu: text('imuValue'), network: text('networkValue'),
      firmwareVersion: text('firmwareVersionValue'), filesystemVersion: text('filesystemVersionValue'),
      captive: text('captiveValue'),
      reset: text('resetValue'),
      clients: text('clientValue'), safe: text('safeLine'), padArm: text('padArmBtn'), headerArm: text('armBtn')
    },
    toasts: {}
  };
  result.toasts.headerArm = await click('armBtn');
  document.querySelector('[data-tab="fpv"]').click();
  await sleep(50);
  result.toasts.fpvArm = await click('padArmBtn');
  result.toasts.disarm = await click('disarmBtn');
  result.toasts.refresh = await click('refreshBtn');
  result.toasts.scan = await click('scanBtn');
  result.toasts.forget = await click('forgetBtn');
  result.toasts.saveDrive = await click('saveDriveBtn');
  result.toasts.saveMapping = await click('saveMappingBtn');
  result.toasts.saveCalibration = await click('saveCalibrationBtn');
  result.toasts.saveServos = await click('saveServosBtn');
  result.toasts.saveBattery = await click('saveBatteryBtn');
  result.toasts.saveWifi = await click('saveWifiBtn');
  result.toasts.saveSafety = await click('saveSafetyBtn');
  document.getElementById('liveOutputEnabled').checked = true;
  result.toasts.liveOutput = await click('setLiveOutputBtn');
  result.toasts.pitMode = await click('pitModeBtn');
  result.toasts.profileSave = await click('saveProfileBtn');
  result.toasts.profileRefresh = await click('refreshProfilesBtn');
  result.toasts.profileLoad = await click('loadProfileBtn');
  result.toasts.profileDelete = await click('deleteProfileBtn');
  result.toasts.setupNext = await click('setupNextBtn');
  result.toasts.setupExport = await click('setupExportBtn');
  document.querySelector('[data-setup-ack="driveDirection"]').click();
  await sleep(20);
  const storedSetupAck = JSON.parse(localStorage.getItem('antcoreSetupAck') || '{}');
  result.setupFlow = {
    rows: document.querySelectorAll('.setup-step').length,
    nextLabel: document.getElementById('setupNextBtn').textContent,
    badge: text('setupFlowBadge'),
    progressWidth: document.getElementById('setupProgressBar').style.width,
    driveAckStored: storedSetupAck.driveDirection === true
  };
  result.setupQr = {
    url: text('setupQrUrl'),
    badge: text('setupQrBadge'),
    cells: document.querySelectorAll('#setupQr span').length,
    darkCells: document.querySelectorAll('#setupQr span.on').length
  };
  result.batteryCalibration = {
    adc: text('batteryAdcValue'),
    pack: text('batteryPackValue'),
    cell: text('batteryCellValue'),
    divider: text('batteryDividerValue'),
    calibration: text('batteryCalibrationValue'),
    thresholds: text('batteryThresholdValue'),
    derating: text('batteryDerateState'),
    bench: text('batteryBenchState')
  };

  document.querySelector('[data-tab="fight"]').click();
  await sleep(50);
  result.toasts.readyBout = await click('readyBoutBtn');
  result.toasts.fightStart = await click('fightStartBtn');
  result.toasts.fightDisarm = await click('fightDisarmBtn');
  result.fightMode = {
    activePage: document.getElementById('fight').classList.contains('active'),
    bodyClass: document.body.classList.contains('fight-mode'),
    disarmVisible: getComputedStyle(document.getElementById('fightDisarmBtn')).display !== 'none',
    checklistRows: document.querySelectorAll('#preFightChecklist .check').length,
    debriefText: text('debriefSummary'),
    topbarHidden: getComputedStyle(document.querySelector('.topbar')).display === 'none'
  };
  document.getElementById('fightExitBtn').click();
  await sleep(50);
  result.fightMode.exited = !document.body.classList.contains('fight-mode');

  document.querySelector('[data-tab="garage"]').click();
  await sleep(50);
  document.getElementById('garageRobotName').value = 'Mock Ant';
  document.getElementById('garageRobotName').dispatchEvent(new Event('input', { bubbles: true }));
  document.getElementById('garageWeaponType').value = 'vertical spinner';
  document.getElementById('garageWeaponType').dispatchEvent(new Event('change', { bubbles: true }));
  result.toasts.saveGarage = await click('saveGarageBtn');
  result.garage = {
    previewName: text('garageRobotPreview'),
    previewMeta: text('garageMetaPreview'),
    cardCount: document.querySelectorAll('.garage-card').length
  };

  document.querySelector('[data-tab="controller"]').click();
  await sleep(50);
  result.trainer = {
    hasDiagram: document.querySelectorAll('#controlTrainer .trainer-pad span').length >= 10,
    hasDriveLabel: text('controlTrainer').includes('drive'),
    hasWeaponLabel: text('controlTrainer').includes('weapon')
  };

  document.querySelector('[data-tab="packs"]').click();
  await sleep(50);
  document.getElementById('packName').value = 'Spare Pack';
  document.getElementById('packChargeVoltage').value = '8.32';
  document.getElementById('packCycles').value = '7';
  document.getElementById('packWeak').checked = true;
  document.getElementById('packNotes').value = 'sags hard';
  result.toasts.savePack = await click('savePackBtn');
  await sleep(50);
  result.packs = {
    options: document.querySelectorAll('#packSelect option').length,
    listText: text('packList'),
    sag: text('packSagValue')
  };
  result.toasts.deletePack = await click('deletePackBtn');

  document.querySelector('[data-tab="spectator"]').click();
  await sleep(50);
  result.spectator = {
    robot: text('spectatorRobotName'),
    battery: text('spectatorBattery'),
    source: text('spectatorSource'),
    noButtons: document.querySelectorAll('#spectator button').length === 0
  };

  window.__mockStatus.controller.active = {
    ageMs: 0,
    axes: { leftX: 0, leftY: 0, rightX: 0, rightY: 0, leftTrigger: 0, rightTrigger: 0 },
    buttons: {}
  };
  renderStatus(window.__mockStatus);
  await sleep(20);
  document.querySelector('#mapThrottleAxis + .learn-button').click();
  await sleep(20);
  window.__mockStatus.controller.active.axes.rightY = 0.86;
  renderStatus(window.__mockStatus);
  await sleep(20);
  const learnedAxis = document.getElementById('mapThrottleAxis').value === 'rightY';

  window.__mockStatus.controller.active.axes.rightY = 0;
  window.__mockStatus.controller.active.buttons = {};
  renderStatus(window.__mockStatus);
  await sleep(20);
  document.querySelector('#mapArmButton + .learn-button').click();
  await sleep(20);
  window.__mockStatus.controller.active.buttons.a = true;
  renderStatus(window.__mockStatus);
  await sleep(20);
  const learnedButton = document.getElementById('mapArmButton').value === 'a';
  result.learn = { learnedAxis, learnedButton, status: text('mappingLearnStatus') };

  window.__mockStatus.controller.xboxRaw = {
    ageMs: 0,
    axes: { leftX: -0.12, leftY: 0.04, rightX: 0.2, rightY: -0.08, leftTrigger: 0, rightTrigger: 0 },
    buttons: {}
  };
  window.__mockStatus.controller.xboxCalibrated = {
    ageMs: 0,
    axes: { leftX: 0, leftY: 0, rightX: 0.2, rightY: -0.08, leftTrigger: 0, rightTrigger: 0 },
    buttons: {}
  };
  renderStatus(window.__mockStatus);
  await sleep(20);
  document.getElementById('captureCenterBtn').click();
  await sleep(20);
  const capturedCenter = Math.abs(Number(document.querySelector('[data-axis="leftX"] .calCenter').value) + 0.12) < 0.002;
  const mappingTestEnabled = document.getElementById('mappingTestMode').checked === true;
  document.getElementById('resetCalibrationBtn').click();
  await sleep(20);
  const resetCenter = Number(document.querySelector('[data-axis="leftX"] .calCenter').value) === 0;
  result.calibration = { capturedCenter, mappingTestEnabled, resetCenter };
  const actionValues = [...document.querySelectorAll('.macroAction option')].map((option) => option.value);
  result.selfRight = {
    optionPresent: actionValues.includes('selfRight'),
    configRendered: document.getElementById('selfRightEnabled').checked === true
      && document.getElementById('selfRightTarget').value === 'servo1',
    badgeRendered: text('selfRightBadge') === 'servo1',
    inspectorRendered: document.getElementById('controllerInspector').textContent.includes('Self right')
  };
  const servoModes = [...document.querySelectorAll('.servoButtonMode')].map((select) => select.value);
  result.servoToggle = {
    modeControlsRendered: servoModes.length >= 8,
    toggleLoaded: servoModes.includes('toggle'),
    momentaryLoaded: servoModes.includes('momentary')
  };
  document.querySelector('[data-tab="servos"]').click();
  await sleep(50);
  const servoRow = document.querySelector('[data-servo="0"]');
  servoRow.querySelector('[data-servo-preset="trigger"]').click();
  await sleep(20);
  const triggerPreset = servoRow.querySelector('.servoEnabled').checked === true
    && servoRow.querySelector('.servoAxisEnabled').checked === true
    && servoRow.querySelector('.servoAxis').value === 'rightTrigger'
    && servoRow.querySelector('.servoButtonEnabled').checked === false;
  servoRow.querySelector('[data-servo-preset="buttons"]').click();
  await sleep(20);
  const buttonRows = servoRow.querySelectorAll('.servo-button');
  const buttonsPreset = servoRow.querySelector('.servoAxisEnabled').checked === false
    && buttonRows[0].querySelector('.servoButtonEnabled').checked === true
    && buttonRows[0].querySelector('.servoButtonName').value === 'a'
    && buttonRows[0].querySelector('.servoButtonMode').value === 'momentary'
    && buttonRows[1].querySelector('.servoButtonName').value === 'y';
  servoRow.querySelector('[data-servo-preset="toggles"]').click();
  await sleep(20);
  const togglesPreset = buttonRows[0].querySelector('.servoButtonName').value === 'leftBumper'
    && buttonRows[0].querySelector('.servoButtonMode').value === 'toggle'
    && buttonRows[1].querySelector('.servoButtonName').value === 'rightBumper'
    && buttonRows[1].querySelector('.servoButtonMode').value === 'toggle';
  servoRow.querySelector('[data-servo-preset="failsafe"]').click();
  await sleep(20);
  const failsafePreset = servoRow.querySelector('.servoAxisEnabled').checked === false
    && servoRow.querySelector('.servoFailsafe').value === '1500'
    && [...buttonRows].every((row) => row.querySelector('.servoButtonEnabled').checked === false);
  servoRow.querySelector('[data-servo-preset="detach"]').click();
  await sleep(20);
  const detachPreset = servoRow.querySelector('.servoDetach').checked === true;
  result.servoPresets = { triggerPreset, buttonsPreset, togglesPreset, failsafePreset, detachPreset };
  result.outputVisualizers = {
    motorPwm: document.getElementById('motorBars').textContent.includes('PWM 0/0 disarmed'),
    servoFailsafe: document.getElementById('servoBars').textContent.includes('failsafe 1500 failsafe')
  };

  document.querySelector('[data-tab="drive"]').click();
  await sleep(50);
  document.querySelector('[data-drive-preset="tank"]').click();
  await sleep(20);
  const tankPreset = document.getElementById('driveMode').value === 'tank'
    && document.getElementById('mapLeftTankAxis').value === 'leftY'
    && document.getElementById('mapRightTankAxis').value === 'rightY'
    && document.getElementById('driveGyroAssist').checked === false;
  document.querySelector('[data-drive-preset="gyro"]').click();
  await sleep(20);
  const gyroPreset = document.getElementById('driveMode').value === 'arcade'
    && document.getElementById('driveGyroAssist').checked === true
    && Number(document.getElementById('driveGyroGain').value) === 0.015;
  document.querySelector('[data-drive-preset="invertible"]').click();
  await sleep(20);
  const invertiblePreset = document.getElementById('driveInvertible').checked === true
    && document.getElementById('driveAutoInvert').checked === true
    && document.getElementById('driveInvertButton').value === 'view'
    && document.getElementById('mapDriveInvertButton').value === 'view';
  document.querySelector('[data-drive-preset="skid"]').click();
  await sleep(20);
  const skidPreset = document.getElementById('driveMode').value === 'arcade'
    && document.getElementById('driveThrottleAxis').value === 'leftY'
    && document.getElementById('mapThrottleAxis').value === 'leftY'
    && document.getElementById('driveTurnAxis').value === 'leftX'
    && document.getElementById('leftMotor').value === '1'
    && document.getElementById('rightMotor').value === '2';
  result.drivePresets = { tankPreset, gyroPreset, invertiblePreset, skidPreset };
  document.getElementById('leftWheelForward').value = 'no';
  document.getElementById('rightWheelForward').value = 'yes';
  document.getElementById('driveWizardApplyBtn').click();
  await sleep(20);
  const requestsBeforeWizardSave = window.__requests.length;
  result.toasts.driveWizardSave = await click('driveWizardSaveBtn');
  result.driveWizard = {
    leftInverted: document.querySelector('[data-motor="0"] .motorInvert').checked === true,
    rightInverted: document.querySelector('[data-motor="1"] .motorInvert').checked === false,
    ackStored: JSON.parse(localStorage.getItem('antcoreSetupAck') || '{}').driveDirection === true,
    noMotorPulse: !window.__requests.slice(requestsBeforeWizardSave).some((request) => request.path === '/api/test/motor')
  };
  document.querySelector('[data-weapon-preset="spinner"]').click();
  await sleep(20);
  const spinnerPreset = document.getElementById('weaponEnabled').checked === true
    && document.getElementById('weaponProfile').value === 'spinner'
    && document.getElementById('weaponInput').value === 'rightTrigger'
    && document.getElementById('weaponToggle').value === 'false'
    && Number(document.getElementById('weaponRampUp').value) === 3
    && Number(document.getElementById('weaponRampDown').value) === 8;
  document.querySelector('[data-weapon-preset="reversible"]').click();
  await sleep(20);
  const reversiblePreset = document.getElementById('weaponProfile').value === 'reversible'
    && document.getElementById('weaponInput').value === 'rightY'
    && document.getElementById('mapWeaponInput').value === 'rightY'
    && document.getElementById('weaponToggle').value === 'false';
  document.querySelector('[data-weapon-preset="lifter"]').click();
  await sleep(20);
  const lifterPreset = document.getElementById('weaponProfile').value === 'lifter'
    && document.getElementById('weaponInput').value === 'rightBumper'
    && document.getElementById('weaponButtonPower').value === '1'
    && document.getElementById('weaponRequireArm').checked === true;
  document.querySelector('[data-weapon-preset="toggle"]').click();
  await sleep(20);
  const togglePreset = document.getElementById('weaponEnabled').checked === true
    && document.getElementById('weaponToggle').value === 'true'
    && document.getElementById('weaponInput').value === 'rightBumper';
  document.querySelector('[data-weapon-preset="disabled"]').click();
  await sleep(20);
  const disabledPreset = document.getElementById('weaponEnabled').checked === false;
  document.querySelector('[data-weapon-preset="verticalSpinner"]').click();
  await sleep(20);
  const verticalPreset = document.getElementById('weaponProfile').value === 'spinner'
    && document.getElementById('garageWeaponType').value === 'vertical spinner'
    && Number(document.getElementById('weaponMaxOutput').value) === 0.9;
  document.querySelector('[data-weapon-preset="horizontalSpinner"]').click();
  await sleep(20);
  const horizontalPreset = document.getElementById('weaponProfile').value === 'spinner'
    && document.getElementById('garageWeaponType').value === 'horizontal spinner'
    && Number(document.getElementById('weaponRampDown').value) === 12;
  document.querySelector('[data-weapon-preset="flipper"]').click();
  await sleep(20);
  const flipperPreset = document.getElementById('weaponProfile').value === 'lifter'
    && document.getElementById('garageWeaponType').value === 'flipper';
  document.querySelector('[data-weapon-preset="grabber"]').click();
  await sleep(20);
  const grabberPreset = document.getElementById('weaponProfile').value === 'lifter'
    && document.getElementById('garageWeaponType').value === 'grabber'
    && Number(document.getElementById('weaponMaxOutput').value) === 0.65;
  document.querySelector('[data-weapon-preset="pusher"]').click();
  await sleep(20);
  const pusherPreset = document.getElementById('weaponEnabled').checked === false
    && document.getElementById('garageWeaponType').value === 'pusher';
  result.weaponPresets = { spinnerPreset, reversiblePreset, lifterPreset, togglePreset, disabledPreset, verticalPreset, horizontalPreset, flipperPreset, grabberPreset, pusherPreset };

  document.querySelector('[data-tab="fpv"]').click();
  await sleep(50);
  result.toasts.saveCamera = await click('saveCameraBtn');
  result.toasts.firmwareNoFile = await click('firmwareUploadBtn');
  result.toasts.fsNoFile = await click('fsUploadBtn');
  document.querySelector('[data-tab="diagnostics"]').click();
  await sleep(50);
  result.toasts.exportBlackbox = await click('exportBlackboxBtn');
  result.toasts.clearBlackbox = await click('clearBlackboxBtn');
  result.toasts.reset = await click('resetConfigBtn');
  document.querySelector('[data-tab="fpv"]').click();
  await sleep(50);

  const y = document.querySelector('[data-pad="y"]');
  y.dispatchEvent(new PointerEvent('pointerdown', { pointerId: 7, bubbles: true }));
  await sleep(20);
  const yDown = state.buttons.y === true;
  y.dispatchEvent(new PointerEvent('pointerup', { pointerId: 7, bubbles: true }));
  await sleep(20);
  const yUp = state.buttons.y === false;

  const stick = document.getElementById('leftStick');
  const r = stick.getBoundingClientRect();
  stick.dispatchEvent(new PointerEvent('pointerdown', { pointerId: 8, clientX: r.right, clientY: r.top + r.height / 2, bubbles: true }));
  await sleep(20);
  const stickMoved = state.sticks.left.x > 0.85 && Math.abs(state.sticks.left.y) < 0.2;
  stick.dispatchEvent(new PointerEvent('pointerup', { pointerId: 8, bubbles: true }));
  await sleep(20);
  const stickReset = Math.abs(state.sticks.left.x) < 0.01 && Math.abs(state.sticks.left.y) < 0.01;

  const rightStickButton = document.querySelector('[data-pad="rightStickButton"]');
  rightStickButton.dispatchEvent(new PointerEvent('pointerdown', { pointerId: 9, bubbles: true }));
  await sleep(20);
  const auxDown = state.buttons.rightStickButton === true;
  rightStickButton.dispatchEvent(new PointerEvent('pointerup', { pointerId: 9, bubbles: true }));
  await sleep(20);
  const auxUp = state.buttons.rightStickButton === false;

  const leftTrigger = document.getElementById('leftTriggerPad');
  leftTrigger.value = '0.64';
  leftTrigger.dispatchEvent(new Event('input', { bubbles: true }));
  await sleep(20);
  const triggerSet = Math.abs(state.triggers.leftTrigger - 0.64) < 0.001;
  leftTrigger.dispatchEvent(new PointerEvent('pointerup', { pointerId: 10, bubbles: true }));
  await sleep(20);
  const triggerReset = state.triggers.leftTrigger === 0;

  window.__ws.sent = [];
  state.webControlEnabled = true;
  state.triggers.leftTrigger = 0.42;
  state.triggers.rightTrigger = 0.71;
  state.buttons.leftStickButton = true;
  sendControlFrame();
  await sleep(20);
  const frame = window.__ws.sent[window.__ws.sent.length - 1] || {};
  const frameTrigger = Math.abs(frame.leftTrigger - 0.42) < 0.001 && Math.abs(frame.rightTrigger - 0.71) < 0.001;
  const frameStickButton = frame.buttons?.leftStickButton === true;
  state.webControlEnabled = false;
  state.buttons.leftStickButton = false;
  state.triggers.leftTrigger = 0;
  state.triggers.rightTrigger = 0;

  result.virtualControls = {
    yDown, yUp, stickMoved, stickReset,
    auxDown, auxUp, triggerSet, triggerReset, frameTrigger, frameStickButton
  };
  result.requests = window.__requests;
  result.final = { armBadge: text('armBadge'), safe: text('safeLine'), padArm: text('padArmBtn'), headerArm: text('armBtn') };
  return result;
})()
"""


LAYOUT_AUDIT = r"""
(() => {
  document.querySelectorAll('.tab').forEach((el) => el.classList.toggle('active', el.dataset.tab === 'fpv'));
  document.querySelectorAll('.page').forEach((el) => el.classList.toggle('active', el.id === 'fpv'));
  document.getElementById('toast')?.classList.remove('show');
  const box = (selector) => {
    const r = document.querySelector(selector).getBoundingClientRect();
    return { left: r.left, right: r.right, top: r.top, bottom: r.bottom, width: r.width, height: r.height };
  };
  const left = box('.left-rail');
  const video = box('.fpv-stage');
  const right = box('.right-rail');
  const fit = getComputedStyle(document.getElementById('fpvStream')).objectFit;
  return {
    viewport: { width: innerWidth, height: innerHeight },
    scrollWidth: document.documentElement.scrollWidth,
    scrollHeight: document.documentElement.scrollHeight,
    left, video, right, objectFit: fit,
    checks: {
      leftBesideVideo: left.right <= video.left + 1,
      rightBesideVideo: video.right <= right.left + 1,
      videoBetweenRails: left.left < video.left && video.right < right.right,
      railsNotBelowVideo: left.top <= video.top + 2 && right.top <= video.top + 2,
      noHorizontalOverflow: document.documentElement.scrollWidth <= innerWidth + 2,
      bottomControlsInViewport: Math.max(left.bottom, video.bottom, right.bottom) <= innerHeight + 2,
      videoUsableWidth: video.width >= 118,
      streamFillsCenter: fit === 'cover'
    }
  };
})()
"""


class Browser:
  def __init__(self, chrome: Path, port: int) -> None:
    self.profile = Path(tempfile.mkdtemp(prefix="antcore-ui-audit-"))
    self.proc = subprocess.Popen(
      [
        str(chrome),
        "--headless=new",
        "--disable-gpu",
        "--no-first-run",
        "--disable-extensions",
        f"--remote-debugging-port={port}",
        "--remote-allow-origins=*",
        f"--user-data-dir={self.profile}",
        "about:blank",
      ],
      stdout=subprocess.DEVNULL,
      stderr=subprocess.DEVNULL,
    )
    self.port = port
    self.ws = self._connect()
    self.next_id = 0
    self.cdp("Page.enable")
    self.cdp("Runtime.enable")

  def _connect(self) -> websocket.WebSocket:
    pages: list[dict[str, Any]] = []
    for _ in range(50):
      try:
        with urllib.request.urlopen(f"http://127.0.0.1:{self.port}/json/list", timeout=0.5) as response:
          pages = [p for p in json.load(response) if p.get("type") == "page"]
        if pages:
          break
      except Exception:
        time.sleep(0.1)
    if not pages:
      raise RuntimeError("Chrome DevTools did not start")
    return websocket.create_connection(pages[0]["webSocketDebuggerUrl"], timeout=30, origin=f"http://127.0.0.1:{self.port}")

  def cdp(self, method: str, params: dict[str, Any] | None = None) -> dict[str, Any]:
    self.next_id += 1
    self.ws.send(json.dumps({"id": self.next_id, "method": method, "params": params or {}}))
    while True:
      msg = json.loads(self.ws.recv())
      if msg.get("id") == self.next_id:
        if "error" in msg:
          raise RuntimeError(f"{method}: {msg['error']}")
        return msg.get("result", {})

  def eval(self, expression: str, await_promise: bool = False) -> Any:
    result = self.cdp("Runtime.evaluate", {"expression": expression, "awaitPromise": await_promise, "returnByValue": True})
    return result["result"]["value"]

  def close(self) -> None:
    try:
      self.proc.terminate()
    finally:
      shutil.rmtree(self.profile, ignore_errors=True)


def chrome_path() -> Path:
  candidates = [
    Path(r"C:\Program Files\Google\Chrome\Application\chrome.exe"),
    Path(r"C:\Program Files (x86)\Microsoft\Edge\Application\msedge.exe"),
  ]
  for candidate in candidates:
    if candidate.exists():
      return candidate
  raise SystemExit("Chrome or Edge executable was not found")


def start_static_server(port: int) -> http.server.ThreadingHTTPServer:
  handler = functools.partial(http.server.SimpleHTTPRequestHandler, directory=str(DATA_DIR))
  server = http.server.ThreadingHTTPServer(("127.0.0.1", port), handler)
  threading.Thread(target=server.serve_forever, daemon=True).start()
  return server


def assert_checks(name: str, checks: dict[str, bool]) -> None:
  failed = [key for key, ok in checks.items() if not ok]
  if failed:
    raise SystemExit(f"{name} failed: {', '.join(failed)}")


def run_mock(port: int) -> dict[str, Any]:
  server = start_static_server(port + 1)
  browser = Browser(chrome_path(), port)
  try:
    browser.cdp("Page.addScriptToEvaluateOnNewDocument", {"source": MOCK_SCRIPT})
    browser.cdp("Page.navigate", {"url": f"http://127.0.0.1:{port + 1}/index.html"})
    time.sleep(2)
    result = browser.eval(BUTTON_AUDIT, await_promise=True)
    result["layout"] = {}
    for name, width, height in [("desktop_1280x720", 1280, 720), ("mobile_390x844", 390, 844)]:
      browser.cdp("Emulation.setDeviceMetricsOverride", {"width": width, "height": height, "deviceScaleFactor": 1, "mobile": width <= 500})
      layout = browser.eval(LAYOUT_AUDIT)
      assert_checks(name, layout["checks"])
      result["layout"][name] = layout
    result["driverIdentity"] = browser.eval("""
(async () => {
  // Simultaneous same-origin pages share storage, but must not share ownership.
  localStorage.setItem('antcoreClientId', state.clientId);
  const frame = document.createElement('iframe');
  frame.hidden = true;
  const ready = new Promise((resolve, reject) => {
    const timer = setTimeout(() => reject(new Error('second page did not load')), 5000);
    frame.onload = () => { clearTimeout(timer); resolve(); };
  });
  frame.src = location.href;
  document.body.appendChild(frame);
  try {
    await ready;
    const other = frame.contentWindow.eval('state.clientId');
    return { distinct: !!other && other !== state.clientId,
             legacyIdIgnored: other !== localStorage.getItem('antcoreClientId') };
  } finally { frame.remove(); localStorage.removeItem('antcoreClientId'); }
})()
""", await_promise=True)
    result["c3Capabilities"] = browser.eval("""
(() => {
  const status = structuredClone(window.__mockStatus);
  status.camera = { supported: false, ready: false, lastError: 'Camera disabled on XIAO ESP32-C3' };
  status.battery.supported = false;
  status.battery.enabled = false;
  status.battery.warn = status.battery.critical = status.battery.derating = false;
  status.safety.batteryCritical = status.safety.batteryWarn = false;
  renderStatus(status);
  const checks = {
    cameraSettingsDisabled: ['saveCameraBtn', 'cameraFrameSize', 'cameraQuality', 'cameraBrightness',
      'cameraContrast', 'cameraSaturation', 'cameraHmirror', 'cameraVflip'].every(id => $(id).disabled),
    streamsRemoved: ['fpvStream', 'fightStream', 'spectatorStream'].every(id => !$(id).hasAttribute('src')),
    cameraExplanation: $('fpvBlank').textContent.includes('unavailable on this board'),
    noCameraInFight: !preFightItems(status).some(item => item[0].includes('Camera')),
    noCameraInSetup: !$('setupChecklist').textContent.includes('Camera'),
    noCameraInWizard: !$('wizardSteps').textContent.includes('Camera'),
    noFpvSetupStep: !setupFlowSteps(status).some(step => step.id === 'fpv'),
    batteryUnavailable: $('batteryBadge').textContent === 'Battery N/A' && $('batteryDividerValue').textContent === 'N/A',
    batterySettingsDisabled: $('batteryEnabled').disabled && $('derateEnabled').disabled && !$('batteryEnabled').checked,
    benchStillAvailable: !$('benchMode').disabled && !$('saveBatteryBtn').disabled,
    batteryWarning: $('safetyBanners').textContent.includes('no automatic low-voltage protection'),
    noBatteryCheck: !preFightItems(status).some(item => item[0].includes('Battery')) && !setupFlowSteps(status).some(step => step.id === 'battery'),
    drivingAvailable: !$('takeControlBtn').disabled
  };
  renderStatus(window.__mockStatus);
  checks.s3CameraRestored = !$('saveCameraBtn').disabled && $('fpvStream').hasAttribute('src');
  checks.s3BatteryRestored = !$('batteryEnabled').disabled && $('batteryDividerValue').textContent === 'x3.00';
  return checks;
})()
""")
    assert_checks("C3 capabilities", result["c3Capabilities"])
    requests = result["requests"]
    checks = {
      "same-origin pages have distinct driver identities": result["driverIdentity"]["distinct"] and result["driverIdentity"]["legacyIdIgnored"],
      "initial dashboard values populated": result["initial"]["armBadge"] == "DISARMED"
      and "0.00 V" in result["initial"]["batteryBadge"]
      and result["initial"]["wifiBadge"].startswith("LAN")
      and "battery critical" in result["initial"]["safe"],
      "ota version values populated": result["initial"]["firmwareVersion"] == "1.1.0-modular"
      and result["initial"]["filesystemVersion"] == "2026-09-14-modular",
      "reset reason value populated": result["initial"]["reset"] == "software reset",
      "captive portal value populated": result["initial"]["captive"] == "ready",
      "header arm reports blocked reason": "Arm blocked: battery critical" in result["toasts"]["headerArm"],
      "fpv arm reports blocked reason": "Arm blocked: battery critical" in result["toasts"]["fpvArm"],
      "disarm button reports success": "Robot disarmed" in result["toasts"]["disarm"],
      "refresh button reports success": "Status refreshed" in result["toasts"]["refresh"],
      "scan button reports success": "BLE scan requested" in result["toasts"]["scan"],
      "forget button reports success": "Controller bonds cleared" in result["toasts"]["forget"],
      "all save buttons report success": all("Configuration saved" in result["toasts"][key] for key in ["saveDrive", "saveMapping", "saveCalibration", "saveServos", "saveBattery", "saveWifi", "saveSafety", "saveCamera"]),
      "pit mode button reports success": "Pit mode enabled" in result["toasts"]["pitMode"],
      "live output button reports success": "Live output tests enabled" in result["toasts"]["liveOutput"],
      "profile buttons report success": "Profile saved" in result["toasts"]["profileSave"]
      and "Profiles refreshed" in result["toasts"]["profileRefresh"]
      and "Profile loaded" in result["toasts"]["profileLoad"]
      and "Profile deleted" in result["toasts"]["profileDelete"],
      "setup flow renders and shortcuts work": result["setupFlow"]["rows"] >= 9
      and result["setupFlow"]["driveAckStored"]
      and "Open" in result["setupFlow"]["nextLabel"]
      and result["setupFlow"]["progressWidth"].endswith("%")
      and "Opened" in result["toasts"]["setupNext"]
      and "Configuration exported" in result["toasts"]["setupExport"],
      "setup QR renders AP URL": result["setupQr"]["url"] == "http://192.168.4.1/"
      and result["setupQr"]["badge"] == "ready"
      and result["setupQr"]["cells"] == 625
      and result["setupQr"]["darkCells"] > 200,
      "battery calibration readout populated": result["batteryCalibration"]["adc"] == "0.000 V"
      and result["batteryCalibration"]["pack"] == "0.00 V"
      and result["batteryCalibration"]["cell"] == "0.00 V"
      and result["batteryCalibration"]["divider"] == "x3.00"
      and result["batteryCalibration"]["calibration"] == "cal 1.00"
      and result["batteryCalibration"]["thresholds"] == "warn 7.0 V / critical 6.4 V"
      and result["batteryCalibration"]["derating"] == "off"
      and result["batteryCalibration"]["bench"] == "normal",
      "fight mode renders checklist debrief and hides config chrome": result["fightMode"]["activePage"]
      and result["fightMode"]["bodyClass"]
      and result["fightMode"]["disarmVisible"]
      and result["fightMode"]["checklistRows"] == 8
      and "Duration" in result["fightMode"]["debriefText"]
      and result["fightMode"]["topbarHidden"]
      and result["fightMode"]["exited"]
      and "Emergency disarm sent" in result["toasts"]["fightDisarm"],
      "garage metadata preview and save work": result["garage"]["previewName"] == "Mock Ant"
      and "vertical spinner" in result["garage"]["previewMeta"]
      and result["garage"]["cardCount"] >= 1
      and "Configuration saved" in result["toasts"]["saveGarage"],
      "controller trainer renders mapped controls": all(result["trainer"].values()),
      "battery pack notes save delete and sag render": result["packs"]["options"] >= 1
      and "Spare Pack" in result["packs"]["listText"]
      and result["packs"]["sag"].startswith("sag")
      and "pack saved" in result["toasts"]["savePack"]
      and "pack deleted" in result["toasts"]["deletePack"],
      "spectator mode is read-only": result["spectator"]["noButtons"]
      and result["spectator"]["battery"] == "0.00 V"
      and result["spectator"]["source"] == "none",
      "mapping learn captures axis and button": result["learn"]["learnedAxis"] and result["learn"]["learnedButton"],
      "calibration captures and resets": all(result["calibration"].values()),
      "self-right mapping and status render": all(result["selfRight"].values()),
      "servo toggle mode renders": all(result["servoToggle"].values()),
      "servo presets set common modes": all(result["servoPresets"].values()),
      "output visualizers show pwm and failsafe detail": all(result["outputVisualizers"].values()),
      "drive presets set mixer fields": all(result["drivePresets"].values()),
      "drive direction wizard flips inversion without motor pulse": all(result["driveWizard"].values()),
      "weapon presets set profile fields": all(result["weaponPresets"].values()),
      "ota buttons reject missing files clearly": result["toasts"]["firmwareNoFile"] == "Choose a .bin file first"
      and result["toasts"]["fsNoFile"] == "Choose a .bin file first",
      "blackbox buttons report success": "Blackbox exported" in result["toasts"]["exportBlackbox"]
      and "Blackbox cleared" in result["toasts"]["clearBlackbox"],
      "reset button reports success": "Factory defaults restored" in result["toasts"]["reset"],
      "virtual controls update state": all(result["virtualControls"].values()),
      "arm endpoint called twice": sum(1 for request in requests if request["path"] == "/api/arm" and request["method"] == "POST") >= 2,
      "config save endpoint called": sum(1 for request in requests if request["path"] == "/api/config" and request["method"] == "PUT") >= 4,
      "profile endpoints called": all(
        any(request["path"] == path for request in requests)
        for path in ["/api/profiles", "/api/profiles/save", "/api/profiles/load", "/api/profiles/delete"]
      ),
      "pack endpoints called": all(
        any(request["path"] == path for request in requests)
        for path in ["/api/packs", "/api/packs/save", "/api/packs/delete"]
      ),
      "blackbox endpoints called": all(
        any(request["path"] == path for request in requests)
        for path in ["/api/blackbox", "/api/blackbox/clear"]
      ),
      "pit endpoint called": any(request["path"] == "/api/pit" for request in requests),
      "live output endpoint called": any(request["path"] == "/api/test/live-output" for request in requests),
      "config export endpoint called": any(request["path"] == "/api/config/export" for request in requests),
    }
    result["checks"] = checks
    failed = [key for key, ok in checks.items() if not ok]
    if failed:
      print(json.dumps(result, indent=2))
      raise SystemExit(f"mock UI audit failed: {', '.join(failed)}")
    return result
  finally:
    browser.close()
    server.shutdown()


def run_live(url: str, port: int) -> dict[str, Any]:
  OUT_DIR.mkdir(parents=True, exist_ok=True)
  browser = Browser(chrome_path(), port)
  try:
    results: dict[str, Any] = {}
    for name, width, height in [("desktop_1280x720", 1280, 720), ("mobile_390x844", 390, 844)]:
      browser.cdp("Emulation.setDeviceMetricsOverride", {"width": width, "height": height, "deviceScaleFactor": 1, "mobile": width <= 500})
      browser.cdp("Page.navigate", {"url": url})
      time.sleep(2)
      result = browser.eval(LAYOUT_AUDIT)
      png = browser.cdp("Page.captureScreenshot", {"format": "png", "captureBeyondViewport": False, "fromSurface": True})["data"]
      (OUT_DIR / f"fpv_audit_{name}.png").write_bytes(__import__("base64").b64decode(png))
      assert_checks(name, result["checks"])
      results[name] = result
    qr = browser.eval("""
(() => ({
  url: document.getElementById('setupQrUrl')?.textContent || '',
  badge: document.getElementById('setupQrBadge')?.textContent || '',
  cells: document.querySelectorAll('#setupQr span').length,
  darkCells: document.querySelectorAll('#setupQr span.on').length
}))()
""")
    assert_checks("live_setup_qr", {
      "setup QR URL": qr["url"] == "http://192.168.4.1/",
      "setup QR badge": qr["badge"] == "ready",
      "setup QR cell count": qr["cells"] == 625,
      "setup QR dark cells": qr["darkCells"] > 200,
    })
    results["setupQr"] = qr
    return results
  finally:
    browser.close()


def main() -> None:
  parser = argparse.ArgumentParser()
  parser.add_argument("--url", default="", help="Live ESP URL. Omit for mock button audit.")
  parser.add_argument("--port", type=int, default=9340, help="DevTools port to use.")
  args = parser.parse_args()
  result = run_live(args.url, args.port) if args.url else run_mock(args.port)
  print(json.dumps(result, indent=2))


if __name__ == "__main__":
  main()
