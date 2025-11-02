#include <Arduino.h>
#include <ArduinoLog.h>
#include <WiFi.h>
#include <ESPmDNS.h>
#include <Preferences.h>
#include <vector>

#include "WifiManager.h"

#define DEFAULT_HOSTNAME  "SerialServer"

Preferences prefs;

void wifiInitialize() 
{
  String hostname;
  wifiGetConfig(NULL, NULL, &hostname);

  // try to connect with stored credentials, fire up an access point if they don't work.
  WiFi.mode(WIFI_STA);
	WiFi.hostname(hostname);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.begin();

  WiFi.waitForConnectResult(5000);
	
	if (WiFi.status() == WL_CONNECTED) {
		Log.infoln("Wifi connected! IP address: %s", WiFi.localIP().toString());

		if (!MDNS.begin(hostname)) {
			Log.warningln("Error setting up MDNS responder!");
		}
	} else {
		Log.infoln("Creating access point...");
		WiFi.mode(WIFI_AP);
    WiFi.setSleep(false);
		WiFi.softAPConfig(IPAddress(192, 168, 1, 1), IPAddress(192, 168, 1, 1), IPAddress(255, 255, 255, 0));
		WiFi.softAP(hostname);
    WiFi.waitForConnectResult(5000);
	}
}

void wifiConnect(const String& ssid, const String& pass) {
  Log.infoln("Connecting to %s:%s", ssid, pass);
  wifiSetConfig(&ssid, &pass);
  WiFi.softAPdisconnect(true);
  WiFi.mode(WIFI_STA);
  WiFi.setSleep(false);
  WiFi.setAutoReconnect(true);
  WiFi.persistent(true);
  WiFi.begin(ssid.c_str(), pass.c_str());
  WiFi.persistent(false);
  WiFi.waitForConnectResult(5000);
}

void wifiGetConfig(String* ssid, String* pass, String* hostname) {
  Preferences prefs;
  if (!prefs.begin("wifi", true)) {
    Log.warningln("Unable to open Wiress Preferences");
  }
  if (ssid) *ssid = prefs.getString("ssid", "");
  if (pass) *pass = prefs.getString("pass", "");
  if (hostname) *hostname = prefs.getString("hostname", DEFAULT_HOSTNAME);
  prefs.end();
}

void wifiSetConfig(const String* ssid, const String* pass, const String* hostname) {
  Preferences prefs;
  if (!prefs.begin("wifi", false)) {
    Log.warningln("Unable to open Wiress Preferences");
  }
  if (ssid) prefs.putString("ssid", *ssid);
  if (pass) prefs.putString("pass", *pass);
  if (hostname) prefs.putString("hostname", *hostname);
  prefs.end();
}

std::vector<String> wifiGetAvailableNetworks() {
  int n = WiFi.scanNetworks();
  
  Log.infoln("Scan complete. Found %d networks:", n);
  for (int i = 0; i < n; ++i) {
    Log.infoln("%2d: %s (RSSI: %d dBm)%s",
      i + 1,
      WiFi.SSID(i).c_str(),
      WiFi.RSSI(i),
      (WiFi.encryptionType(i) == WIFI_AUTH_OPEN) ? " OPEN" : "");
  }

  // free memory used by scan results
  WiFi.scanDelete();

  return std::vector<String>();
}

String wifiGetHostname() {
  String hostname;
  wifiGetConfig(NULL, NULL, &hostname);
  return hostname;
}