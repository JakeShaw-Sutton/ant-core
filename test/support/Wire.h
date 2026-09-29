#pragma once
struct FakeWire { void begin(int, int) {} };
inline FakeWire Wire;
