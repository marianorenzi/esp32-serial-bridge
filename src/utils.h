#pragma once

#include <Arduino.h>
#include <stdlib.h>

template <typename EnumT, size_t N>
String enumToString(EnumT value, const char* const (&names)[N]) {
  uint8_t i = static_cast<uint8_t>(value);
  return (i < N) ? String(names[i]) : String("?");
}

template <typename EnumT, size_t N>
const char* enumToCString(EnumT value, const char* const (&names)[N]) {
  uint8_t i = static_cast<uint8_t>(value);
  return (i < N) ? names[i] : "?";
}

template <typename EnumT, size_t N>
EnumT stringToEnum(const String& s, const char* const (&names)[N], EnumT defaultVal) {
  for (uint8_t i = 0; i < N; ++i) {
    if (s.equalsIgnoreCase(names[i]))
      return static_cast<EnumT>(i);
  }
  return defaultVal;
}

static inline unsigned long toULong(const String& s, int base = 10) {
  const char* buf = s.c_str();
  if (!buf) return 0;
  return strtoul(buf, nullptr, base);
}