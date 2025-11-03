#pragma once

#include <ESPUI.h>
#include <map>

#include "SerialBridge.h"
#include "PrintUtils.h"

class UserInterface {
  public:
    UserInterface() : m_logWs("/logws"), m_wsPrint(&m_logWs) {}

    void addSerialBridge(SerialBridge& bridge);
    Print* logPrint() { return &m_wsPrint; }

    void start();

    struct BridgeSettings {
      SerialBridge* bridge;
      UserInterface* ui;
      int bridgeTypeControl;
      int tcpHostControl;
      int tcpPortControl;
      int serialBaudrateControl;
      int serialFormatControl;
      int serialHasEchoControl;
      int serialSimulateEchoControl;
    };

  private:

    std::map<String, BridgeSettings> m_bridges;
    int m_ssidControl;
    int m_passwordControl;
    AsyncWebSocket m_logWs;
    WebSocketPrint m_wsPrint;

    void addWifiSettingsTab();
    void addLogsTab();
    void task();

    friend void tcpTypeChangedCallback(Control *sender, int type, void* arg);
    friend void submittedBridgeDetailsCallback(Control *sender, int type, void* arg);
    friend void submittedWifiDetailsCallback(Control *sender, int type, void* arg);
    friend void nullCallback(Control *sender, int type, void* arg);
};