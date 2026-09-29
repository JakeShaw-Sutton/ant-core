#pragma once
class ESP32PWM {
 public:
  inline static int reservedTimer = -1;
  static void allocateTimer(int timer) { reservedTimer = timer; }
};
class Servo {
 public:
  bool attached=false;
  int attaches=0, detaches=0, writes=0, pulse=0;
  void setPeriodHertz(int) {}
  int attach(int,int,int) { attached=true; attaches++; return 1; }
  void detach() { attached=false; detaches++; }
  void writeMicroseconds(int value) { writes++; pulse=value; }
};
