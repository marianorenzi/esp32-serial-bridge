#include <Arduino.h>
#include <ESPUI.h>
#include <freertos/FreeRTOS.h>

#include "SerialBridge.h"
#include "UserInterface.h"
#include "WifiManager.h"
#include "utils.h"

// friend functions (ui callbacks)
void tcpTypeChangedCallback(Control *sender, int type, void* arg);
void submittedBridgeDetailsCallback(Control *sender, int type, void* arg);
void submittedWifiDetailsCallback(Control *sender, int type, void* arg);
void nullCallback(Control *sender, int type, void* arg);

// This is the main function which builds our GUI
void UserInterface::start() 
{
	ESPUI.setVerbosity(Verbosity::Quiet);
  addWifiSettingsTab();
	ESPUI.begin("Serial Bridge");
	xTaskCreate((TaskFunction_t)(&UserInterface::task), "UserInterface", 2048, this, 1, nullptr);
}

void UserInterface::task() 
{
	for (;;) {
		delay(500);
	}
}

void UserInterface::addSerialBridge(SerialBridge& bridge)
{
	if (bridge.name().length() == 0 || bridge.code().length() == 0) return;
	if (m_bridges.find(bridge.code()) != m_bridges.end()) return;

	// 
	auto [it, _] = m_bridges.emplace(bridge.code(), BridgeSettings{ &bridge, this });
	BridgeSettings* settings = &it->second;

	// bridge tab
  auto tab = ESPUI.addControl(Tab, "", bridge.name().c_str());

	// tcp settings
	ESPUI.addControl(Separator, "Bridge Settings", "", None, tab);
	int bridgeTypeControl = ESPUI.addControl(Select, "Type", SerialBridge::toString(bridge.type()), Wetasphalt, tab, tcpTypeChangedCallback, (void*)settings);
	for (uint8_t i = 0; i < static_cast<uint8_t>(SerialBridge::BridgeType::COUNT); ++i) {
		const char* cStr = SerialBridge::toCString(static_cast<SerialBridge::BridgeType>(i));
		ESPUI.addControl(Option, cStr, cStr, None, bridgeTypeControl);
	}
	int tcpHostControl = ESPUI.addControl(Text, "Host", bridge.host(), None, tab, nullCallback, (void*)settings);
	int tcpPortControl = ESPUI.addControl(Number, "Port", String(bridge.port()), None, tab, nullCallback, (void*)settings);

	// serial settings
	ESPUI.addControl(Separator, "Serial Settings", "", None, tab);
	int serialBaudrateControl = ESPUI.addControl(Number, "Baudrate", String(bridge.baud()), None, tab, nullCallback, (void*)settings);
	int serialFormatControl = ESPUI.addControl(Select, "Format", SerialBridge::toString(bridge.format()), Wetasphalt, tab, nullCallback, (void*)settings);
	for (uint8_t i = 0; i < static_cast<uint8_t>(SerialBridge::SerialFormat::COUNT); ++i) {
		const char* cStr = SerialBridge::toCString(static_cast<SerialBridge::SerialFormat>(i));
		ESPUI.addControl(Option, cStr, cStr, None, serialFormatControl);
	}
	int hasEcho = ESPUI.addControl(Switcher, "Has Echo", bridge.hasEcho() ? "1" : "0", None, tab, nullCallback, (void*)settings);
	int simulateEcho = ESPUI.addControl(Switcher, "Simulate Echo", bridge.simulateEcho() ? "1" : "0", None, tab, nullCallback, (void*)settings);

	// save button
	ESPUI.addControl(Button, "Save", "Save", Peterriver, tab, submittedBridgeDetailsCallback, (void*)settings);

	settings->bridgeTypeControl = bridgeTypeControl;
	settings->tcpHostControl = tcpHostControl;
	settings->tcpPortControl = tcpPortControl;
	settings->serialBaudrateControl = serialBaudrateControl;
	settings->serialFormatControl = serialFormatControl;
	settings->serialHasEchoControl = hasEcho;
	settings->serialSimulateEchoControl = simulateEcho;

	// 
	tcpTypeChangedCallback(nullptr, 0, (void*)settings);
}

void UserInterface::addWifiSettingsTab() 
{
  String ssid, pass, hostname;
  wifiGetConfig(&ssid, &pass, &hostname);
	auto wifitab = ESPUI.addControl(Tab, "", "WiFi Credentials");
	this->m_ssidControl = ESPUI.addControl(Text, "SSID", ssid.c_str(), Alizarin, wifitab);
	//Note that adding a "Max" control to a text control sets the max length
	ESPUI.addControl(Max, "", "32", None, this->m_ssidControl);
	this->m_passwordControl = ESPUI.addControl(Text, "Password", pass.c_str(), Alizarin, wifitab);
	ESPUI.addControl(Max, "", "64", None, this->m_passwordControl);
	ESPUI.addControl(Button, "Save", "Save", Peterriver, wifitab, submittedWifiDetailsCallback, (void*)this);
}

void tcpTypeChangedCallback(Control *sender, int type, void* arg)
{
	UserInterface::BridgeSettings* bridgeSettings = (UserInterface::BridgeSettings*)arg;

	if (ESPUI.getControl(bridgeSettings->bridgeTypeControl)->value == SerialBridge::toString(SerialBridge::BridgeType::TCP_CLIENT)) {
		ESPUI.updateVisibility(bridgeSettings->tcpHostControl, true);
	} else {
		ESPUI.updateVisibility(bridgeSettings->tcpHostControl, false);
	}
}

void switchChangedCallback(Control *sender, int type, void* arg)
{
	UserInterface::BridgeSettings* bridgeSettings = (UserInterface::BridgeSettings*)arg;
}

void nullCallback(Control *sender, int type, void* arg) {}

void submittedBridgeDetailsCallback(Control *sender, int type, void* arg)
{
	if (type != B_UP) return;

	UserInterface::BridgeSettings* bridgeSettings = (UserInterface::BridgeSettings*)arg;
	SerialBridge::BridgeType bType = SerialBridge::fromTypeString(ESPUI.getControl(bridgeSettings->bridgeTypeControl)->value);
	String host = ESPUI.getControl(bridgeSettings->tcpHostControl)->value;
	ushort port = ESPUI.getControl(bridgeSettings->tcpPortControl)->value.toInt();
	ulong baud = toULong(ESPUI.getControl(bridgeSettings->serialBaudrateControl)->value);
	SerialBridge::SerialFormat fmt = SerialBridge::fromFormatString(ESPUI.getControl(bridgeSettings->serialFormatControl)->value);
	bool hasEcho = ESPUI.getControl(bridgeSettings->serialHasEchoControl)->value == "0" ? false : true;
	bool simulateEcho = ESPUI.getControl(bridgeSettings->serialSimulateEchoControl)->value == "0" ? false : true;

	// update bridge settings
	bridgeSettings->bridge->setConfig(bType, host, port, baud, fmt, hasEcho, simulateEcho);
}

void submittedWifiDetailsCallback(Control *sender, int type, void* arg)
{
	if (type != B_UP) return;

	UserInterface* ui = (UserInterface*)arg;
	wifiConnect(ESPUI.getControl(ui->m_ssidControl)->value, ESPUI.getControl(ui->m_passwordControl)->value);
}