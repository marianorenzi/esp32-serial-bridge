#pragma once

#include <Arduino.h>

void wifiInitialize();
void wifiConnect(const String& ssid, const String& pass);
void wifiGetConfig(String* ssid, String* pass, String* hostname = NULL);
void wifiSetConfig(const String* ssid, const String* pass, const String* hostname = NULL);
String wifiGetHostname();