#include <Arduino.h>
#include <ESPUI.h>
#include <freertos/FreeRTOS.h>

#include "SerialBridge.h"
#include "UserInterface.h"
#include "WifiManager.h"
#include "utils.h"

const char* g_logHtml = R"HTML(
<!doctype html><meta charset="utf-8">
<title>Live Logs</title>
<style>
  body{margin:0;background:#111;color:#ddd;font:13px/1.35 monospace}
  #bar{display:flex;gap:.5rem;padding:.5rem;background:#222;position:sticky;top:0}
  #view{white-space:pre; padding:.5rem; max-height:calc(100vh - 42px); overflow:auto}
  .dim{color:#888}
</style>
<div id="bar">
  <button id="pause">Pause</button>
  <button id="clear">Clear</button>
  <label><input id="autoscroll" type="checkbox" checked> autoscroll</label>
  <input id="filter" placeholder="filter (regex)" size="28">
  <span class="dim" id="stat"></span>
</div>
<pre id="view"></pre>
<script>
const view=document.getElementById('view');
const pauseBtn=document.getElementById('pause');
const clearBtn=document.getElementById('clear');
const autoscroll=document.getElementById('autoscroll');
const filter=document.getElementById('filter');
const stat=document.getElementById('stat');
let paused=false, rx=null;
filter.addEventListener('input',()=>rx=filter.value?new RegExp(filter.value):null);
pauseBtn.onclick=()=>{paused=!paused; pauseBtn.textContent=paused?'Resume':'Pause';};
clearBtn.onclick=()=>{view.textContent='';};
function append(l){ if(rx && !rx.test(l)) return; view.append(l); if(autoscroll.checked) view.scrollTop=view.scrollHeight; }
fetch('/logs/history').then(r=>r.json()).then(a=>a.forEach(append));
const proto=location.protocol==='https:'?'wss:':'ws:';
const ws=new WebSocket(proto+'//'+location.host+'/logws');
ws.onopen = ()=> stat.textContent='connected';
ws.onclose= ()=> stat.textContent='disconnected';
ws.onmessage=(ev)=>{ if(!paused) append(ev.data); };
</script>
<script>
document.addEventListener('DOMContentLoaded', () => {
  try {
    const pdoc = window.parent.document;

    // Find the <iframe> element in the parent that corresponds to *this* window
    const myFrameEl = Array.from(pdoc.getElementsByTagName('iframe'))
      .find(f => f.contentWindow === window);
    if (!myFrameEl) return;

    // Remove classes from the closest ancestor that has both `two` and `columns`
    const hostWithCols = myFrameEl.closest('.two.columns');
    if (hostWithCols) hostWithCols.classList.remove('two', 'columns');

  } catch (e) {
    // Will throw SecurityError if cross-origin
    console.debug('Could not access parent document:', e);
  }
});
</script>
)HTML";

// friend functions (ui callbacks)
void tcpTypeChangedCallback(Control *sender, int type, void* arg);
void submittedBridgeDetailsCallback(Control *sender, int type, void* arg);
void restartCallback(Control *sender, int type, void* arg);
void submittedWifiDetailsCallback(Control *sender, int type, void* arg);
void nullCallback(Control *sender, int type, void* arg);

// This is the main function which builds our GUI
void UserInterface::start() 
{
	ESPUI.setVerbosity(Verbosity::Quiet);
	addWifiSettingsTab();
	addLogsTab();
	ESPUI.begin("Serial Bridge");
	xTaskCreate((TaskFunction_t)(&UserInterface::task), "UserInterface", 2048, this, 1, nullptr);
}

void UserInterface::task() 
{
	// ESPUI WebServer
	AsyncWebServer* server = ESPUI.WebServer();

	m_logWs.onEvent([](AsyncWebSocket*, AsyncWebSocketClient* c, AwsEventType t, void*, uint8_t*, size_t) {
		if (t == WS_EVT_CONNECT) c->text("[logws connected]\n");
	}
	);

	// register log websocket
	server->addHandler(&m_logWs);

	// html page
	server->on("/logs", HTTP_GET, [](AsyncWebServerRequest* req) {
			auto resp = req->beginResponse(200, "text/html", g_logHtml);
        	req->send(resp);
		}
	);

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
	auto save = ESPUI.addControl(Button, "Save", "Save", Peterriver, tab, submittedBridgeDetailsCallback, (void*)settings);
	ESPUI.addControl(Button, "", "Restart", Peterriver, save, restartCallback, nullptr);
	
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

void UserInterface::addLogsTab() 
{
	auto tab = ESPUI.addControl(Tab, "", "Logs");
	ESPUI.addControl(Label, "", "<iframe src=\"/logs\" style=\"width:100\%;height:70vh;border:0;border-radius:8px;overflow:hidden\"></iframe>", None, tab);
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

void restartCallback(Control *sender, int type, void* arg)
{
	if (type != B_UP) return;
	ESP.restart();
}

void submittedWifiDetailsCallback(Control *sender, int type, void* arg)
{
	if (type != B_UP) return;
	
	UserInterface* ui = (UserInterface*)arg;
	wifiConnect(ESPUI.getControl(ui->m_ssidControl)->value, ESPUI.getControl(ui->m_passwordControl)->value);
}