#include <ArduinoLog.h>
#include <HardwareSerial.h>
#include <WiFi.h>
#include <freertos/FreeRTOS.h>
#include <mutex>

#include "SerialBridge.h"

#if HAS_CLASSIC_BT
  #include <BluetoothSerial.h>
  BluetoothSerial SerialBT;
#endif

#if HAS_BLE
  #include <NimBLEDevice.h>
  #include "NuPacket.hpp"
  #include "NuStream.hpp"
#endif

std::mutex g_bleMutex;
bool g_bleInitialized = false;
void initBle(String name);

SerialBridge::SerialBridge(String name, String code, HardwareSerial& hwSerial) :
m_name(name),
m_code(code),
m_streamType(HW_SERIAL),
m_stream(&hwSerial) {}

SerialBridge::SerialBridge(String name, String code, HWCDC& hwCdc) :
m_name(name),
m_code(code),
m_streamType(HW_CDC),
m_stream(&hwCdc) {}

void SerialBridge::start()
{
  // load config
  if (!loadConfig()) {
    // return;
  }

  // create bridge task
  if (m_bridgeType == BridgeType::TCP_SERVER) {
    xTaskCreate((TaskFunction_t)(&SerialBridge::tcpServerTask), "TcpServerBridge", 2048, this, 1, nullptr);
  } else if (m_bridgeType == BridgeType::TCP_CLIENT) {
    xTaskCreate((TaskFunction_t)(&SerialBridge::tcpClientTask), "TcpClientBridge", 2048, this, 1, nullptr);
  } else if (m_bridgeType == BridgeType::BLUETOOTH) {
    xTaskCreate((TaskFunction_t)(&SerialBridge::bluetoothTask), "BluetoothBridge", 2048, this, 1, nullptr);
  } else if (m_bridgeType == BridgeType::BLE) {
    xTaskCreate((TaskFunction_t)(&SerialBridge::bleTask), "BLEBridge", 4096, this, 1, nullptr);
  } else {
    Log.errorln("SerialBridge(%s) unknown bridge type, cannot start", m_code.c_str());
  }
}

bool SerialBridge::loadConfig()
{
  Preferences prefs;
  bool ret = true;

  if (!prefs.begin(m_code.c_str(), true)) {
    Log.warningln("Unable to load %s Preferences", m_code.c_str());
    ret = false;
  }

  m_bridgeType = static_cast<BridgeType>(prefs.getUChar("type", static_cast<uint8_t>(BridgeType::TCP_SERVER)));
  m_host = prefs.getString("host", "");
  m_port = prefs.getUShort("port", 3000);
  m_baud = prefs.getULong("baud", 9600);
  m_fmt = static_cast<SerialFormat>(prefs.getUChar("fmt", static_cast<uint8_t>(SerialFormat::F8N1)));
  m_hasEcho = prefs.getBool("hecho", false);
  m_simulateEcho = prefs.getBool("secho", false);
  prefs.end();

  // log loaded config
  Log.infoln("Loaded %s Preferences", m_code.c_str());
  Log.noticeln("Type: %s", toCString(m_bridgeType));
  Log.noticeln("Host: %s", m_host.c_str());
  Log.noticeln("Port: %u", m_port);
  Log.noticeln("Baud: %u", m_baud);
  Log.noticeln("Fmt: %s", toCString(m_fmt));
  Log.noticeln("Has Echo: %s", m_hasEcho ? "true" : "false");
  Log.noticeln("Simulate Echo: %s", m_simulateEcho ? "true" : "false");

  return ret;
}

void SerialBridge::setConfig(BridgeType bType, String host, ushort port, unsigned long baud, SerialFormat fmt, bool hasEcho, bool simulateEcho)
{
  Preferences prefs;

  if (!prefs.begin(m_code.c_str(), false)) {
    Log.warningln("Unable to save %s Preferences", m_code.c_str());
  }

  Log.infoln("Saving %s Preferences", m_code.c_str());
  Log.noticeln("Type: %s", toCString(bType));
  Log.noticeln("Host: %s", host.c_str());
  Log.noticeln("Port: %u", port);
  Log.noticeln("Baud: %u", baud);
  Log.noticeln("Fmt: %s", toCString(fmt));
  Log.noticeln("Has Echo: %s", hasEcho ? "true" : "false");
  Log.noticeln("Simulate Echo: %s", simulateEcho ? "true" : "false");

  prefs.putUChar("type", static_cast<uint8_t>(bType));
  prefs.putString("host", host);
  prefs.putUShort("port", port);
  prefs.putULong("baud", baud);
  prefs.putUChar("fmt", static_cast<uint8_t>(fmt));
  prefs.putBool("hecho", hasEcho);
  prefs.putBool("secho", simulateEcho);
  prefs.end();
}

bool SerialBridge::initStream()
{
  // begin stream with it's corresponding call
  if (m_streamType == HW_CDC) {
    Log.infoln("SerialBridge(%s) initializing HWCDC Serial...", m_code.c_str());
    static_cast<HWCDC*>(m_stream)->begin(m_baud);
  } else if (m_streamType == HW_SERIAL) {
    Log.infoln("SerialBridge(%s) initializing HW Serial...", m_code.c_str());
    static_cast<HardwareSerial*>(m_stream)->begin(m_baud, toArduinoConfig(m_fmt));
  } else {
    Log.errorln("SerialBridge(%s) unknown stream type, skipping initialization...", m_code.c_str());
    return false;
  }

  return true;
}

void SerialBridge::tcpServerTask()
{
  Log.infoln("TcpServer(%s) started task...", m_code.c_str());

  initStream();
  
  WiFiServer server(m_port, 1);
  uint8_t buffer[512];

  for (;;) 
  {
    // wait for WiFi
    while (WiFi.status() != WL_CONNECTED) { delay(250); }
    server.begin();
    server.setNoDelay(true);

    while (WiFi.status() == WL_CONNECTED) { 
      WiFiClient client = server.accept();
      if (!client) { delay(20); continue; }
      client.setNoDelay(true);
      
      Log.infoln("TcpServer(%s) accepted client from %s:%u", m_code.c_str(), client.remoteIP().toString().c_str(), client.remotePort());
      
      // serve this single client until it disconnects
      while (client.connected() && WiFi.status() == WL_CONNECTED) {
        // TCP -> Serial
        pumpStreamToStream(client, *m_stream, buffer, sizeof(buffer));
        // Serial -> TCP
        pumpStreamToStream(*m_stream, client, buffer, sizeof(buffer));
        
        // small delay to yield cpu
        if (client.available() == 0 && m_stream->available() == 0) delay(2);
      }
      
      Log.infoln("TcpServer(%s) client disconnected", m_code.c_str());
      
      client.stop();
      delay(10);
    }
  }
}

void SerialBridge::tcpClientTask()
{
  Log.infoln("TcpClient(%s) started task...", m_code.c_str());

  initStream();

  WiFiClient client;
  uint8_t buffer[512];

  for (;;) {
    // wait for WiFi
    if (WiFi.status() != WL_CONNECTED) { delay(250); }

    // connect (with simple backoff)
    if (!client.connected()) {
      client.stop();
      client.setNoDelay(true);
      if (client.connect(m_host.c_str(), m_port)) {
        Log.infoln("TcpClient(%s) connected to %s:%u", m_code.c_str(), m_host.c_str(), m_port);
      } else delay(2000);
      continue;
    }

    // TCP -> Serial
    pumpStreamToStream(client, *m_stream, buffer, sizeof(buffer));
    // Serial -> TCP
    pumpStreamToStream(*m_stream, client, buffer, sizeof(buffer));

    // if link dropped, loop will reconnect
    if (!client.connected() || WiFi.status() != WL_CONNECTED) {
      Log.infoln("TcpClient(%s) connection lost", m_code.c_str());
      client.stop();
      delay(100);
      continue;
    }

    // small delay to yield cpu
    if (client.available() == 0 && m_stream->available() == 0) delay(2);
  }
}

void SerialBridge::bluetoothTask()
{
  Log.infoln("Bluetooth(%s) started task...", m_code.c_str());

#if !HAS_BLUETOOTH
  Log.infoln("Bluetooth(%s) not supported by this device...", m_code.c_str());
  while (1);
#endif

  for (;;) {
    delay(500);
  }
}

void SerialBridge::bleTask()
{
  Log.infoln("BLE(%s) started task...", m_code.c_str());

#if !HAS_BLE
  Log.infoln("BLE(%s) not supported by this device...", m_code.c_str());
  while (1);
#endif

  initStream();
  initBle("Serial Bridge");

  uint8_t buffer[512];
  NordicUARTStream bleSerial;
  bleSerial.start();

  for (;;) {
    // wait for connection
    while (!bleSerial.isConnected()) { delay(500); }
    Log.infoln("BLE(%s) connected to peer", m_code.c_str());

    while (bleSerial.isConnected()) {
      // BLE -> Serial
      pumpStreamToStream(bleSerial, *m_stream, buffer, sizeof(buffer));
      // Serial -> BLE
      pumpStreamToStream(*m_stream, bleSerial, buffer, sizeof(buffer));
    }

    Log.infoln("BLE(%s) peer disconnected", m_code.c_str());
  }
}

void SerialBridge::pumpStreamToStream(Stream& in, Stream& out, uint8_t* buf, size_t cap) {
  int avail = in.available();
  while (avail > 0) {
    size_t n = ((size_t)avail > cap) ? cap : (size_t)avail;
    int r = in.readBytes(buf, n);
    if (r > 0) out.write(buf, (size_t)r);
    avail = in.available();
  }
}

void initBle(String name)
{
  std::lock_guard<std::mutex> lock(g_bleMutex);
  if (g_bleInitialized) return;
  NimBLEDevice::init(name.c_str());
  NimBLEDevice::getAdvertising()->setName(name.c_str());
  NordicUARTService::allowMultipleInstances = true;
  g_bleInitialized = true;
}