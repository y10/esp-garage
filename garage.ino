#define DEBUG
#include <DNSServer.h>
#include <ESP8266WiFi.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h>
#include <ESP8266mDNS.h>
#include <ArduinoOTA.h>
#include <fauxmoESP.h>
#include <Ticker.h>
#include "GARAGE.h"
#include "GARAGE-HTTP.h"

const int RELAY_PIN = D1;
const int LED_PIN = 2;

DNSServer dns;
AsyncWebServer httpServer(80);
AsyncWiFiManager wifiManager(&httpServer, &dns);
fauxmoESP fauxmo;
GarageHttp httpApi;
Ticker ticker;

void setup(void){
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  Serial.begin(9600);

  setupWifi();
  setupOTA();
  setupAlexa();
  setupHttp();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop(void){
  ArduinoOTA.handle();
  fauxmo.handle();
  delay(1000);
}

void setupWifi()
{
  String hostname("Switch-");
  hostname += String(ESP.getChipId(), HEX);
  if (!wifiManager.autoConnect(hostname.c_str()))
  {
    Serial.println("[MAIN] failed to connect and hit timeout");
    ESP.reset();
  }

  //if you get here you have connected to the WiFi
  Serial.println("[MAIN] connected to Wifi");
}

void setupOTA()
{
  Serial.println("[OTA] Setup OTA");
  // OTA
  // An important note: make sure that your project setting of Flash size is at least double of size of the compiled program. Otherwise OTA fails on out-of-memory.
  ArduinoOTA.onStart([]() {
    Serial.println("[OTA] OTA: Start");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("[OTA] OTA: End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("OTA progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    char errormsg[100];
    sprintf(errormsg, "OTA: Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      strcpy(errormsg + strlen(errormsg), "Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      strcpy(errormsg + strlen(errormsg), "Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      strcpy(errormsg + strlen(errormsg), "Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      strcpy(errormsg + strlen(errormsg), "Receive Failed");
    else if (error == OTA_END_ERROR)
      strcpy(errormsg + strlen(errormsg), "End Failed");
    Serial.println(errormsg);
  });
  ArduinoOTA.begin();
}

void setupAlexa()
{
  // Setup Alexa devices
  fauxmo.addDevice("Garage");
  Serial.print("[MAIN] Added alexa device: ");

  fauxmo.onSet([](unsigned char device_id, const char *device_name, bool state) {
    Serial.printf("[MAIN] Set Device #%d (%s) state: %s\n", device_id, device_name, state ? "ON" : "OFF");
    Garage.toggle();
  });
  
  fauxmo.onGet([](unsigned char device_id, const char *device_name) {
    Serial.printf("[MAIN] Get Device #%d (%s) state: %s\n", device_id, device_name, NULL);
    return NULL;
  });  
}

void setupHttp()
{
  // Setup Web UI
  Serial.println("[MAIN] Setup http server.");
  httpApi.setup(httpServer);
  httpServer.begin();

  Garage.onToggle([&]() {
    toggle();
  });
  Garage.onOpen([&]() {
    switchOn();
  });
  Garage.onClose([&]() {
    switchOff();
  });
}

void toggle() {

  switchOn();

  ticker.once_ms(500, []{

    switchOff();
    
  });
}

void switchOn() {
  Serial.println("Toggle switch to ON...");
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
}

void switchOff() {
  Serial.println("Toggle switch to OFF...");
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(RELAY_PIN, LOW);
}
