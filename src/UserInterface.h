#pragma once

#include <ESPUI.h>
#include <map>

#include "SerialBridge.h"

class UserInterface {
  public:
    UserInterface() {}

    void addSerialBridge(SerialBridge& bridge);

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

    void addWifiSettingsTab();
    void task();

    friend void tcpTypeChangedCallback(Control *sender, int type, void* arg);
    friend void submittedBridgeDetailsCallback(Control *sender, int type, void* arg);
    friend void submittedWifiDetailsCallback(Control *sender, int type, void* arg);
    friend void nullCallback(Control *sender, int type, void* arg);
};