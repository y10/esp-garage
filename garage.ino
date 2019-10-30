#define ESP8266

#include <FS.h> //this needs to be first, or it all crashes and burns...
#include <ESP8266WiFi.h> 
#include <ESP8266mDNS.h>
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>
#include <ESPAsyncWiFiManager.h> 
#include <AsyncMqttClient.h>
#include <ArduinoOTA.h>
#include <fauxmoESP.h>
#include <Ticker.h>
#include "GARAGE.h"
#include "GARAGE-HTTP.h"

#define RELAY_PIN D1
#define LED_PIN 2

DNSServer dns;
AsyncWebServer httpServer(80);
AsyncEventSource webEvents("/events");
AsyncWiFiManager wifiManager(&httpServer, &dns);
WiFiEventHandler wifiConnectHandler;
WiFiEventHandler wifiDisconnectHandler;
AsyncMqttClient mqttClient;
Ticker mqttReconnectTimer;
fauxmoESP fauxmo;
GarageHttp httpApi;
Ticker ticker;

void setup(void){
  pinMode(LED_BUILTIN, OUTPUT);
  pinMode(RELAY_PIN, OUTPUT);
  Serial.begin(9600);

  setupGarage();
  setupWifi();
  setupHttp();
  setupOTA();
  setupAlexa();
  setupMQTT();

  pinMode(LED_PIN, OUTPUT);
  digitalWrite(LED_BUILTIN, HIGH);
}

void loop(void){
  ArduinoOTA.handle();
  fauxmo.handle();
}

void setupGarage()
{
  Garage.setupLog(&webEvents);
  Garage.load();
}

void setupWifi()
{
  static AsyncWiFiManagerParameter custom_disp_name("disp_name", "Display Name", Garage.dispname().c_str(), 50);
  wifiManager.addParameter(&custom_disp_name);

  static AsyncWiFiManagerParameter custom_mqtt_host("mqtt_host", "MQTT Host or IP", Garage.mqtthost().c_str(), 50);
  wifiManager.addParameter(&custom_mqtt_host);

  static AsyncWiFiManagerParameter custom_mqtt_port("mqtt_port", "MQTT Port", Garage.mqttport().c_str(), 10);
  wifiManager.addParameter(&custom_mqtt_port);

  static AsyncWiFiManagerParameter custom_mqtt_user("mqtt_user", "MQTT User", Garage.mqttuser().c_str(), 50);
  wifiManager.addParameter(&custom_mqtt_user);

  static AsyncWiFiManagerParameter custom_mqtt_pwrd("mqtt_pwrd", "MQTT Password", Garage.mqttpwrd().c_str(), 50);
  wifiManager.addParameter(&custom_mqtt_pwrd);

  //set callback that gets called when connecting to previous WiFi fails, and enters Access Point mode
  wifiManager.setAPCallback([](AsyncWiFiManager *myWiFiManager)
  {
    WEB_LOG("[MAIN] Entered config mode");
    WEB_LOG(WiFi.softAPIP().toString());
    //if you used auto generated SSID, print it
    WEB_LOG(myWiFiManager->getConfigPortalSSID());
    //entered config mode, make led toggle faster
    ticker.attach(0.2, []{
      //toggle state
      int state = digitalRead(BUILTIN_LED); // get the current state of GPIO pin
      digitalWrite(BUILTIN_LED, !state);    // set pin to the opposite state
    });
  });
  
  //set config save notify callback
  wifiManager.setSaveConfigCallback([](){
    Garage.dispname(custom_disp_name.getValue());
    Garage.mqtthost(custom_mqtt_host.getValue());
    Garage.mqttport(atoi(custom_mqtt_port.getValue()));
    Garage.mqttuser(custom_mqtt_user.getValue());
    Garage.mqttpwrd(custom_mqtt_pwrd.getValue());
    Garage.save();
  });

  wifiManager.setConfigPortalTimeout(300); // wait 5 minutes for Wifi config and then return

  if (!wifiManager.autoConnect(Garage.hostname().c_str()))
  {
    WEB_LOG("[MAIN] failed to connect and hit timeout");
    ESP.reset();
  }

  //if you get here you have connected to the WiFi
  WEB_LOG("[MAIN] connected to Wifi");
}

void setupOTA()
{
  WEB_LOG("[OTA] Setup OTA");
  // OTA
  // An important note: make sure that your project setting of Flash size is at least double of size of the compiled program. Otherwise OTA fails on out-of-memory.
  ArduinoOTA.onStart([]() {
    WEB_LOG("[OTA] OTA: Start");
  });
  ArduinoOTA.onEnd([]() {
    WEB_LOG("[OTA] OTA: End");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    WEB_LOG("OTA progress: " + (progress / (total / 100)));
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
    WEB_LOG(errormsg);
  });
  ArduinoOTA.setHostname(Garage.hostname().c_str());
  ArduinoOTA.begin();
}

void setupAlexa()
{
  // Setup Alexa devices
  fauxmo.addDevice(Garage.dispname().c_str(), "controllee");
  WEB_LOG("[MAIN] Added alexa device: ");

  fauxmo.onSet([](unsigned char device_id, const char *device_name, bool state) {
    Serial.printf("[MAIN] Set Device #%d (%s) state: %s\n", device_id, device_name, state ? "ON" : "OFF");
    Garage.toggle();
  });
  
  fauxmo.onGet([](unsigned char device_id, const char *device_name) {
    Serial.printf("[MAIN] Get Device #%d (%s) state: %s\n", device_id, device_name, "NULL");
    return false;
  });  
}

void setupHttp()
{
  // Setup Web UI
  WEB_LOG("[MAIN] Setup http server.");
  httpApi.setup(httpServer);
  httpServer.addHandler(&webEvents);
  httpServer.begin();

  Garage.onToggle([&]() {
    toggle();
  });
  Garage.onOpen([&]() {
    toggle();
  });
  Garage.onClose([&]() {
    toggle();
  });
  Garage.onReset([](){
    WEB_LOG("[MAIN] Factory reset requested.");

    WiFi.disconnect(true);
    SPIFFS.format();
    ESP.restart();

    delay(5000);
  });
  Garage.onRestart([](){
    WEB_LOG("[MAIN] Restarting...");
    ESP.restart();
  });
}


void setupMQTT()
{
  WEB_LOG("[MQTT] Setup MQTT");
  
  WiFi.onStationModeGotIP([](const WiFiEventStationModeGotIP& event) {
    WEB_LOG("Connected to Wi-Fi.");
    mqttClient.connect();
  });

  WiFi.onStationModeDisconnected([](const WiFiEventStationModeDisconnected& event) {
    WEB_LOG("Disconnected from Wi-Fi.");
    mqttReconnectTimer.detach(); // ensure we don't reconnect to MQTT while reconnecting to Wi-Fi
  });

  mqttClient.onConnect([](bool sessionPresent) {
    WEB_LOG("Connected to MQTT.\n\r  Session present: " + String(sessionPresent));
    
    uint16_t packetIdSub = mqttClient.subscribe(String("cmnd/" + Garage.safename() + "/TOGGLE").c_str(), 2);
    WEB_LOG("Subscribing at QoS 2, packetId: " + String(packetIdSub));
    
    uint16_t packetIdPub2 = mqttClient.publish(String("tele/" + Garage.safename() + "/LWT").c_str(), 2, true, "Online");
    WEB_LOG("Publishing at QoS 2, packetId: " + String(packetIdPub2));
  });

  mqttClient.onDisconnect([](AsyncMqttClientDisconnectReason reason) {
    WEB_LOG("Disconnected from MQTT.");

    if (WiFi.isConnected()) {
      mqttReconnectTimer.once(10, [](){
        WEB_LOG("Connecting to MQTT...");
        mqttClient.connect();
      });
    }
  });

  mqttClient.onMessage([](char* topic, char* payload, AsyncMqttClientMessageProperties properties, size_t len, size_t index, size_t total) {
    
    String message;
    for (int i = 0; i < len; i++) {
      message += (char)payload[i];
    }

    WEB_LOG((String)"Command received." +
    "\r\n  topic: " + String(topic) +
    "\r\n  message: " + message +
    "\r\n  qos: " + String(properties.qos) +
    "\r\n  dup: " + String(properties.dup) + 
    "\r\n  retain: " + String(properties.retain));

    if (strcmp(topic, String("cmnd/" + Garage.safename() + "/TOGGLE").c_str()) == 0) {
      
      toggle();

      if (message == "C") {
        mqttClient.publish(String("stat/" + Garage.safename() + "/RESULT").c_str(), 1, true, "Closed");
      } else {
        mqttClient.publish(String("stat/" + Garage.safename() + "/RESULT").c_str(), 1, true, "Opened");
      }
    }

  });
  Garage.setupMQTT(mqttClient);
  WEB_LOG("Connecting to MQTT...");
  mqttClient.connect();
}

void toggle() {

  switchOn();

  ticker.once_ms(500, []{
    
    switchOff();

  });
}

void switchOn() {
  WEB_LOG("Toggle switch to ON...");
  digitalWrite(LED_BUILTIN, LOW);
  digitalWrite(RELAY_PIN, HIGH);
}

void switchOff() {
  WEB_LOG("Toggle switch to OFF...");
  digitalWrite(LED_BUILTIN, HIGH);
  digitalWrite(RELAY_PIN, LOW);
}
