#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <string>
#include <type_traits>
#include <vector>
using std::max;
using std::min;
class String {
 public:
  std::string value;
  String() = default;
  String(const char* s): value(s ? s : "") {}
  String(const std::string& s): value(s) {}
  template<class T, typename std::enable_if<std::is_arithmetic<T>::value,int>::type = 0>
  String(T n): value(std::to_string(n)) {}
  const char* c_str() const { return value.c_str(); }
  size_t length() const { return value.size(); }
  bool startsWith(const char* s) const { return value.rfind(s, 0) == 0; }
  int indexOf(const char* s) const { auto p=value.find(s); return p==std::string::npos ? -1 : int(p); }
  friend String operator+(const String& a, const String& b) { return a.value+b.value; }
  friend bool operator==(const String& a,const String& b) { return a.value==b.value; }
  friend bool operator!=(const String& a,const String& b) { return !(a==b); }
};
inline uint32_t fakeMillis = 1000;
inline uint32_t millis() { return fakeMillis; }
inline int fakeAdcReads = 0;
inline int fakeAdcAttenuations = 0;
inline void analogSetPinAttenuation(int, int) { ++fakeAdcAttenuations; }
inline uint32_t analogReadMilliVolts(int) { ++fakeAdcReads; return 2500; }
#define ADC_11db 3
inline std::array<int, 16> fakeDuty{};
inline std::array<int, 16> fakePinLevels{};
inline std::array<int, 16> fakePinChannels = [] { std::array<int, 16> a{}; a.fill(-1); return a; }();
inline std::vector<std::array<int, 16>> fakeOutputHistory;
inline void recordOutputs() {
  auto levels = fakePinLevels;
  for (size_t pin = 0; pin < levels.size(); ++pin)
    if (fakePinChannels[pin] >= 0) levels[pin] = fakeDuty[fakePinChannels[pin]];
  fakeOutputHistory.push_back(levels);
}
inline void digitalWrite(int pin, int value) { fakePinLevels[pin] = value; recordOutputs(); }
inline void pinMode(int,int) {}
inline void ledcSetup(int,int,int) {}
inline void ledcAttachPin(int pin,int channel) { fakePinChannels[pin] = channel; recordOutputs(); }
inline void ledcDetachPin(int pin) { fakePinChannels[pin] = -1; recordOutputs(); }
inline void ledcWrite(int channel,int duty) { fakeDuty[channel]=duty; recordOutputs(); }
#define LOW 0
#define OUTPUT 1
#define D0 0
#define D1 1
#define D2 2
#define D3 3
#define D4 4
#define D5 5
#define D6 6
#define D7 7
#define D8 8
#define D9 9
#define D10 10
using portMUX_TYPE = int;
#define portMUX_INITIALIZER_UNLOCKED 0
