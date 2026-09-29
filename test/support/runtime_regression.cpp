#include <cassert>
#include <cstdio>
#include <cstring>
#include "app/robot_state.h"
#include "app/config_state.h"
#include "app/commands_state.h"
#include "app/inputs_state.h"
#include "app/peripherals_state.h"
#include "antcore_app_robot.h"
#include "antcore_app_inputs.h"
#include "antcore_output_test.h"
#include "antcore_control_owner.h"
#include "antcore_file_store.h"
#include "antcore_outputs.h"

namespace antcore_config { bool isDefaultAdminPin(const AppConfig&) { return false; } }
namespace antcore_app {
ConfigState configState;
CommandsState commandsState;
InputsState inputsState;
PeripheralsState peripheralsState;
bool configIsValid(String*) { return true; }
void addLog(const char*, const String&, bool) {}
void updateXboxStateFromController(uint32_t) {}
bool deliverDuringSnapshot = false;
antcore::WebControlSnapshot webControlSnapshot() {
 if (deliverDuringSnapshot) {
  deliverDuringSnapshot=false;
  fakeMillis++;
  auto previous=inputsState.webControlOwner.snapshot();
  inputsState.webControlOwner.publish(previous.connectionId,previous.clientId,previous.frame,fakeMillis);
 }
 return inputsState.webControlOwner.snapshot();
}
bool xboxControlFresh(uint32_t now) {
 return inputsState.bleReady && inputsState.xbox.isConnected() && inputsState.xboxState.lastMs &&
        now-inputsState.xboxState.lastMs <= XBOX_CONTROL_STALE_MS;
}
bool hasFreshControlSource() {
 const auto web=webControlSnapshot(); const auto now=millis();
 return antcore::selectControlSource(web,xboxControlFresh(now),now)!=antcore::ControlSource::None;
}
}
using namespace antcore_app;

void reset() {
 configState.cfg=AppConfig();
 configState.cfg.security.authEnabled=false;
 robotState=RobotState();
 inputsState=InputsState();
 commandsState=CommandsState();
 peripheralsState=PeripheralsState();
 fakeMillis=1000;
 fakeDuty.fill(0);
 inputsState.bleReady=true;
 inputsState.xbox.connected=true;
 inputsState.xboxState.lastMs=fakeMillis;
}
void publishWeb(uint32_t connection=1) {
 inputsState.webControlOwner.claim(connection,"page",fakeMillis);
 ControlState frame;
 inputsState.webControlOwner.publish(connection,"page",frame,fakeMillis);
}
void assertStopped() {
 for (int duty:fakeDuty) assert(duty==0);
 for (float target:robotState.motorTarget) assert(target==0);
}
void servoDisarm() {
 reset();
 configState.cfg.servos[0].detachOnDisarm=true;
 initPins();
 assert(!robotState.servoAttached[0]);
 armRobot("test");
 updateOutputs();
 assert(robotState.servoAttached[0]);
 disarmRobot("stop");
 const int writes=robotState.servos[0].writes;
 const int attaches=robotState.servos[0].attaches;
 for(int i=0;i<50;i++) {fakeMillis++;updateOutputs();}
 assert(!robotState.servoAttached[0]);
 assert(robotState.servos[0].writes==writes);
 assert(robotState.servos[0].attaches==attaches);
}
void neutralTrim() {
 reset();
 for(auto& motor:configState.cfg.motors){motor.trim=0.25f;motor.rampPerSecond=0;}
 armRobot("test");updateOutputs();assertStopped();
 inputsState.xboxState.leftY=-0.1f;
 fakeMillis++;updateOutputs();
 assert(robotState.motorTarget[0]<0);
 assert(robotState.motorTarget[1]<0);
 inputsState.xboxState.leftY=0;
 fakeMillis++;updateOutputs();assertStopped();
}
void cancelledTestsNeverResume() {
 reset();publishWeb();
 robotState.outputTest.liveOutputEnabled=true;
 armRobot("test");
 assert(antcore_output_test::queueMotorTest(robotState.outputTest,true,false,false,1,0.5f,2000,fakeMillis).ok);
 assert(antcore_output_test::queueServoTest(robotState.outputTest,true,false,false,configState.cfg.servos,1,1900,2000,fakeMillis).ok);
 fakeMillis++;updateOutputs();
 assert(robotState.motorTarget[0]>0);
 disarmRobot("stop");assertStopped();
 assert(!robotState.outputTest.motorActive && !robotState.outputTest.servoActive);
 fakeMillis++;armRobot("test");updateOutputs();
 assert(robotState.armed);assertStopped();
 assert(robotState.servoOutUs[0]==1500);
}
void timeoutCannotUseBackup() {
 reset();publishWeb();
 configState.cfg.weapon.enabled=true;
 configState.cfg.weapon.toggle=true;
 configState.cfg.control.actions[0].enabled=true;
 std::strcpy(configState.cfg.control.actions[0].button,"a");
 std::strcpy(configState.cfg.control.actions[0].action,"robotArmToggle");
 inputsState.xboxState.a=true;  // A held backup button must not arm on handover.
 armRobot("test");armWeapon("test");
 robotState.weaponToggle=true;
 fakeMillis++;updateOutputs();
 assert(robotState.motorTarget[2]>0);
 fakeMillis+=WEB_CONTROL_STALE_MS+1;
 inputsState.xboxState.lastMs=fakeMillis;
 inputsState.xboxState.leftY=1;
 updateOutputs();assertStopped();
 assert(!robotState.weaponToggle);
 fakeMillis+=WEB_CONTROL_DISARM_MS;
 inputsState.xboxState.lastMs=fakeMillis;
 updateOutputs();
 assert(!robotState.armed && !robotState.weaponArmed);assertStopped();
 fakeMillis+=WEB_DRIVER_LOCK_MS;
 inputsState.xboxState.lastMs=fakeMillis;
 updateOutputs();
 assert(!robotState.armed);assertStopped();
}
void ownerReleaseAndReconnect() {
 reset();publishWeb();armRobot("test");
 assert(inputsState.webControlOwner.release(1));
 publishWeb(2);
 fakeMillis++;updateOutputs();
 assert(!robotState.armed);assertStopped();
}
void tankInversion() {
 reset();
 std::strcpy(configState.cfg.drive.mode,"tank");
 configState.cfg.drive.deadband=0;
 configState.cfg.drive.expo=0;
 inputsState.xboxState.leftY=0.8f;
 inputsState.xboxState.rightY=0.4f;
 armRobot("test");
 robotState.driveInverted=true;
 updateOutputs();
 assert(std::fabs(robotState.motorTarget[0]+0.4f)<0.001f);
 assert(std::fabs(robotState.motorTarget[1]+0.8f)<0.001f);
 robotState.driveInverted=false;
 configState.cfg.drive.autoInvertWithImu=true;
 peripheralsState.imuTelemetry.present=true;
 peripheralsState.imuTelemetry.az=-1;
 fakeMillis++;updateOutputs();
 assert(std::fabs(robotState.motorTarget[0]+0.4f)<0.001f);
 assert(std::fabs(robotState.motorTarget[1]+0.8f)<0.001f);
 peripheralsState.imuTelemetry.az=1;
 fakeMillis++;updateOutputs();
 assert(std::fabs(robotState.motorTarget[0]-0.8f)<0.001f);
 assert(std::fabs(robotState.motorTarget[1]-0.4f)<0.001f);
}
void frameArrivalDuringLoop() {
 reset();publishWeb();armRobot("test");
 deliverDuringSnapshot=true;
 updateOutputs();
 assert(robotState.armed);
}
void atomicStorage() {
 const String replacement="complete new configuration";
 for(const char* path:{"/config.json","/profiles/default.json","/packs.json"}) {
  for(int crash=0;crash<10;crash++) {
   fs::FS fs;fs.disk.files[path]="old configuration";fs.disk.failAt=crash;
   try {antcore_storage::writeTextAtomic(fs,path,replacement);} catch(const PowerLoss&) {}
   assert(fs.disk.files.count(path)==1);
   assert(fs.disk.files[path]=="old configuration" || fs.disk.files[path]==replacement.value);
  }
  for(int failure=0;failure<3;failure++) {
   fs::FS fs;fs.disk.files[path]="old configuration";
   fs.disk.partial=failure==0;fs.disk.renameFails=failure==1;fs.disk.openFails=failure==2;
   assert(!antcore_storage::writeTextAtomic(fs,path,replacement));
   assert(fs.disk.files[path]=="old configuration");
  }
  fs::FS fs;fs.disk.files[path]="old configuration";
  assert(antcore_storage::writeTextAtomic(fs,path,replacement));
  assert(fs.disk.files[path]==replacement.value);
 }
 // PlatformIO LittleFS images permit 32-byte filenames. Appending .tmp to a
 // valid full-length profile name must be avoidable without losing atomicity.
 const char* longPath="/profiles/benchcheck-722cd20-49f2c0e0.json";
 const char* shortTemporary="/profiles/.profile-49f2c0e0.tmp";
 assert(strlen(strrchr(longPath,'/')+1)==32);
 {
  fs::FS fs;fs.disk.maxNameLength=32;fs.disk.files[longPath]="old configuration";
  assert(!antcore_storage::writeTextAtomic(fs,longPath,replacement));
  assert(fs.disk.files[longPath]=="old configuration");
  assert(antcore_storage::writeTextAtomic(fs,longPath,replacement,shortTemporary));
  assert(fs.disk.files[longPath]==replacement.value);
 }
 for(int crash=0;crash<10;crash++) {
  fs::FS fs;fs.disk.maxNameLength=32;fs.disk.files[longPath]="old configuration";fs.disk.failAt=crash;
  try {antcore_storage::writeTextAtomic(fs,longPath,replacement,shortTemporary);} catch(const PowerLoss&) {}
  assert(fs.disk.files.count(longPath)==1);
  assert(fs.disk.files[longPath]=="old configuration" || fs.disk.files[longPath]==replacement.value);
 }
 for(const char* invalidTemporary:{longPath,"/other/.profile.tmp","/profiles/","/profiles/.","/profiles/.."}) {
  fs::FS fs;fs.disk.files[longPath]="old configuration";
  assert(!antcore_storage::writeTextAtomic(fs,longPath,replacement,invalidTemporary));
  assert(fs.disk.step==0 && fs.disk.files[longPath]=="old configuration");
 }
}
void pwmRouting() {
 initMotorOutputs();
 fakeOutputHistory.clear();
 for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
  writeMotorRaw(i, 0.5f);
  assert(fakeOutputHistory.back()[MOTOR_A_PINS[i]] == MOTOR_PWM_MAX / 2);
  assert(fakeOutputHistory.back()[MOTOR_B_PINS[i]] == 0);
 }
 // Reverse all three while other motors are running; then stop and restart.
 for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
  writeMotorRaw(i, -1.0f);
  assert(fakeOutputHistory.back()[MOTOR_A_PINS[i]] == 0);
  assert(fakeOutputHistory.back()[MOTOR_B_PINS[i]] == MOTOR_PWM_MAX);
 }
 stopAllMotorPins();
 for (uint8_t i = 0; i < MOTOR_COUNT; ++i) writeMotorRaw(i, 0.25f);
 stopAllMotorPins();
 for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
  assert(fakeOutputHistory.back()[MOTOR_A_PINS[i]] == 0);
  assert(fakeOutputHistory.back()[MOTOR_B_PINS[i]] == 0);
 }
#if ANTCORE_SHARED_MOTOR_PWM
 assert(ESP32PWM::reservedTimer == 2);
 for (const auto& levels : fakeOutputHistory) {
  for (uint8_t i = 0; i < MOTOR_COUNT; ++i)
   assert(levels[MOTOR_A_PINS[i]] == 0 || levels[MOTOR_B_PINS[i]] == 0);
 }
 for (uint8_t i = 0; i < MOTOR_COUNT; ++i) {
  assert(fakePinChannels[MOTOR_A_PINS[i]] == -1);
  assert(fakePinChannels[MOTOR_B_PINS[i]] == -1);
 }
#endif
}
void capabilities() {
 // All eleven I/O roles must remain distinct on either board.
 const int pins[] = {MOTOR_A_PINS[0], MOTOR_A_PINS[1], MOTOR_A_PINS[2],
  MOTOR_B_PINS[0], MOTOR_B_PINS[1], MOTOR_B_PINS[2], SERVO_PINS[0], SERVO_PINS[1],
  PIN_BATTERY_SENSE, PIN_I2C_SDA, PIN_I2C_SCL};
 for (int i = 0; i < 11; ++i) for (int j = 0; j < i; ++j) assert(pins[i] != pins[j]);
#if ANTCORE_SHARED_MOTOR_PWM
 assert(PIN_BATTERY_SENSE == D8 && MOTOR_B_PINS[0] == D1 && !ANTCORE_HAS_BATTERY_SENSE);
 bool ready = true; String error; CameraStreamStats stats; stats.frames = 42;
 assert(!initCameraRuntime(CameraConfig(), ready, error, stats, nullptr, nullptr));
 assert(!ready && error.length() > 0 && stats.frames == 0);
#else
 assert(PIN_BATTERY_SENSE == D8 && BATTERY_DIVIDER_MULTIPLIER == 3.0f);
#endif
}
void batteryCapability() {
 reset();
 auto& battery = peripheralsState.battery;
 // Even an old/imported config cannot cause C3 to read or drive the divider pin.
 configState.cfg.battery.enabled = true;
 configState.cfg.battery.derateEnabled = true;
 battery.critical = true; battery.warn = true; battery.derating = true;
 sampleBatteryTelemetry(PIN_BATTERY_SENSE, configState.cfg.battery, battery);
#if ANTCORE_HAS_BATTERY_SENSE
 assert(fakeAdcReads == 1 && fakeAdcAttenuations == 1);
 assert(std::fabs(battery.packVolts - 7.5f) < 0.001f);
#else
 assert(fakeAdcReads == 0 && fakeAdcAttenuations == 0);
 assert(!battery.critical && !battery.warn && !battery.derating && battery.packVolts == 0);
 assert(!BatteryConfig().enabled);
 initPins();
 assert(fakePinChannels[D8] == -1);
 for (const auto& levels : fakeOutputHistory) assert(levels[D8] == 0);
 armRobot("test");
 assert(robotState.armed);
 disarmRobot("test");
 configState.cfg.battery.benchMode = true;
 armRobot("test");
 assert(!robotState.armed);  // No ADC does not disable the bench interlock.
#endif
}
int main(int argc,char** argv) {
 assert(argc==2);
 if(!strcmp(argv[1],"servo")) servoDisarm();
 else if(!strcmp(argv[1],"trim")) neutralTrim();
 else if(!strcmp(argv[1],"cancel")) cancelledTestsNeverResume();
 else if(!strcmp(argv[1],"timeout")) timeoutCannotUseBackup();
 else if(!strcmp(argv[1],"reconnect")) ownerReleaseAndReconnect();
 else if(!strcmp(argv[1],"storage")) atomicStorage();
 else if(!strcmp(argv[1],"tank")) tankInversion();
 else if(!strcmp(argv[1],"arrival")) frameArrivalDuringLoop();
 else if(!strcmp(argv[1],"pwm")) pwmRouting();
 else if(!strcmp(argv[1],"capabilities")) capabilities();
 else if(!strcmp(argv[1],"battery")) batteryCapability();
 else return 2;
 puts("PASS");
}
