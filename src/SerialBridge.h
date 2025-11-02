#pragma once

#include <HardwareSerial.h>
#include <Preferences.h>
#include "utils.h"

class SerialBridge {
  public:
    SerialBridge(String name, String code, HardwareSerial& hwSerial);
    SerialBridge(String name, String code, HWCDC& hwCdc);

    enum class SerialFormat : uint8_t {
      F5N1, F6N1, F7N1, F8N1,
      F5N2, F6N2, F7N2, F8N2,
      F5E1, F6E1, F7E1, F8E1,
      F5E2, F6E2, F7E2, F8E2,
      F5O1, F6O1, F7O1, F8O1,
      F5O2, F6O2, F7O2, F8O2,
      COUNT
    };

    enum class BridgeType : uint8_t {
      TCP_SERVER,
      TCP_CLIENT,
      BLUETOOTH,
      COUNT
    };

    void start();
    void setConfig(BridgeType bType, String host, ushort port, unsigned long baud, SerialFormat fmt, bool hasEcho, bool simulateEcho);

    String name() { return m_name; }
    String code() { return m_code; }
    BridgeType type() { return m_bridgeType; }
    String host() { return m_host; }
    ushort port() { return m_port; }
    unsigned long baud() { return m_baud; }
    SerialFormat format() { return m_fmt; }
    bool hasEcho() { return m_hasEcho; }
    bool simulateEcho() { return m_simulateEcho; }

    static inline String toString(SerialFormat fmt) { return enumToString(fmt, kFormatStr); }
    static inline const char* toCString(SerialFormat fmt) { return enumToCString(fmt, kFormatStr); }
    static inline SerialFormat fromFormatString(const String& s) { return stringToEnum(s, kFormatStr, SerialFormat::F8N1); }

    static inline String toString(BridgeType type) { return enumToString(type, kTypeStr); }
    static inline const char* toCString(BridgeType type) { return enumToCString(type, kTypeStr); }
    static inline BridgeType fromTypeString(const String& s) { return stringToEnum(s, kTypeStr, BridgeType::TCP_SERVER); }

  private:

    enum SerialType {
      HW_CDC,
      HW_SERIAL,
      SW_SERIAL
    };

    static constexpr const char* kFormatStr[] = {
      "5N1","6N1","7N1","8N1",
      "5N2","6N2","7N2","8N2",
      "5E1","6E1","7E1","8E1",
      "5E2","6E2","7E2","8E2",
      "5O1","6O1","7O1","8O1",
      "5O2","6O2","7O2","8O2",
    };
    static_assert(static_cast<size_t>(SerialBridge::SerialFormat::COUNT) == sizeof(kFormatStr)/sizeof(kFormatStr[0]), "mismatch");

    static constexpr const char* kTypeStr[] = {
      "TCP Server",
      "TCP Client",
      "Bluetooth"
    };
    static_assert(static_cast<size_t>(SerialBridge::BridgeType::COUNT) == sizeof(kTypeStr)/sizeof(kTypeStr[0]), "mismatch");

    static inline uint32_t toArduinoConfig(SerialFormat f) {
      switch (f) {
        case SerialFormat::F5N1: return SERIAL_5N1;
        case SerialFormat::F6N1: return SERIAL_6N1;
        case SerialFormat::F7N1: return SERIAL_7N1;
        case SerialFormat::F8N1: return SERIAL_8N1;
        case SerialFormat::F5N2: return SERIAL_5N2;
        case SerialFormat::F6N2: return SERIAL_6N2;
        case SerialFormat::F7N2: return SERIAL_7N2;
        case SerialFormat::F8N2: return SERIAL_8N2;
        case SerialFormat::F5E1: return SERIAL_5E1;
        case SerialFormat::F6E1: return SERIAL_6E1;
        case SerialFormat::F7E1: return SERIAL_7E1;
        case SerialFormat::F8E1: return SERIAL_8E1;
        case SerialFormat::F5E2: return SERIAL_5E2;
        case SerialFormat::F6E2: return SERIAL_6E2;
        case SerialFormat::F7E2: return SERIAL_7E2;
        case SerialFormat::F8E2: return SERIAL_8E2;
        case SerialFormat::F5O1: return SERIAL_5O1;
        case SerialFormat::F6O1: return SERIAL_6O1;
        case SerialFormat::F7O1: return SERIAL_7O1;
        case SerialFormat::F8O1: return SERIAL_8O1;
        case SerialFormat::F5O2: return SERIAL_5O2;
        case SerialFormat::F6O2: return SERIAL_6O2;
        case SerialFormat::F7O2: return SERIAL_7O2;
        case SerialFormat::F8O2: return SERIAL_8O2;
        default: return SERIAL_8N1;
      }
    }

    String m_name;
    String m_code;
    BridgeType m_bridgeType;
    String m_host;
    uint16_t m_port;
    unsigned long m_baud;
    SerialFormat m_fmt;
    bool m_hasEcho;
    bool m_simulateEcho;

    SerialType m_streamType;
    Stream* m_stream;

    void tcpServerTask();
    void tcpClientTask();
    void bluetoothTask();

    void pumpStreamToStream(Stream& in, Stream& out, uint8_t* buf, size_t cap);

    bool loadConfig();
    bool initStream();
};