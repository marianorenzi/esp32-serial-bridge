
#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <ArduinoLog.h>
#include <DEVNULL.h>
#include "UserInterface.h"
#include "WifiManager.h"

UserInterface userInterface;

void printPrefix(Print* _logOutput, int logLevel);
void printSuffix(Print* _logOutput, int logLevel);

void setup() 
{
#if ARDUINO_USB_CDC_ON_BOOT
  unsigned long t0 = millis();
  while (!Serial && millis() - t0 < 5000) { delay(10); } // wait up to 5s for monitor
#endif

  Log.setPrefix(printPrefix);
  Log.setSuffix(printSuffix);

#ifdef SERIAL_DEBUG
  Serial.begin(115200);
  Log.begin(LOG_LEVEL_VERBOSE, &Serial);
#else
  Log.begin(LOG_LEVEL_VERBOSE, new DEVNULL());
  auto serialBridge = new SerialBridge("USB-Serial Bridge", "serial", Serial);
  Log.setShowLevel(false);
  serialBridge->start();
  userInterface.addSerialBridge(*serialBridge);
#endif
  Log.setShowLevel(false);

  auto uart0Bridge = new SerialBridge("UART0 Bridge", "uart0", Serial0);
  uart0Bridge->start();
  userInterface.addSerialBridge(*uart0Bridge);
  
  wifiInitialize();
  userInterface.start();
}

void loop() {}

void printTimestamp(Print* _logOutput)
{
  // char timestamp[30];
  // snprintf(timestamp, 30, "%02u:%02u:%02u.%03u ", time.Hour(), time.Minute(), time.Second(), (uint16_t)millis() % 1000);
  _logOutput->print(millis());
}

void printLogLevel(Print* _logOutput, int logLevel)
{
  switch (logLevel) {
    default:
    case 0:_logOutput->print("[SILENT] " ); break;
    case 1:_logOutput->print("[FATAL] "  ); break;
    case 2:_logOutput->print("[ERROR] "  ); break;
    case 3:_logOutput->print("[WARNING] "); break;
    case 4:_logOutput->print("[INFO] "   ); break;
    case 5:_logOutput->print("[TRACE] "  ); break;
    case 6:_logOutput->print("[VERBOSE] "); break;
  }   
}

void printPrefix(Print* _logOutput, int logLevel)
{
  printTimestamp(_logOutput);
  printLogLevel (_logOutput, logLevel);
  // print thread name
  _logOutput->print("[");
  _logOutput->print(pcTaskGetName(NULL));
  _logOutput->print("] ");
}

void printSuffix(Print* _logOutput, int logLevel)
{
  _logOutput->print("");
}