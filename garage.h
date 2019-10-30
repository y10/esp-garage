#ifndef GARAGE_H
#define GARAGE_H

#include <Arduino.h>
#include <ArduinoJson.h> //https://github.com/bblanchon/ArduinoJson
#include <vector>
#include <functional>
#define WEB_LOG(message) Garage.log(message)

typedef std::function<void()> Delegate;

class GarageClass
{
private:
  volatile bool isDoorOpen;
  String host_name;
  String disp_name;
  String safe_name;
  String mqtt_host;
  String mqtt_port;
  String mqtt_user;
  String mqtt_pwrd;

  Delegate onToggleCalback;
  Delegate onResetCalback;
  Delegate onRestartCalback;
  Delegate onOpenCalback;
  Delegate onCloseCalback;

  AsyncEventSource* webEvents;

public:
  GarageClass() : isDoorOpen(false)
  {
    host_name = "garage-" + String(ESP.getChipId(), HEX);
    mqtt_host = "home";
    mqtt_port = 1883;
    mqtt_user = "homeassistant";
    mqtt_pwrd = "P@$$w0rd";
    disp_name = "Garage";
    safe_name = "garage";
  }

  const String hostname() const {
    return host_name;
  }

  const String dispname() const {
    return disp_name;
  }

  const String safename() const{
    return safe_name;
  }

  void dispname(const char* name){
    if (strlen(name) > 0)
    {
      if (!disp_name.equals(name))
      {
        disp_name = name;
      }
      if (!safe_name.equals(disp_name))
      {
        safe_name = name;
        safe_name.replace(" ", "_");
        safe_name.toLowerCase();
      }
    }
  }

  const String mqtthost() const {
    return mqtt_host;
  }

  void mqtthost(const char* host){
    if (strlen(host) > 0) mqtt_host = host;
  }

  const String mqttport() const {
    return mqtt_port;
  }

  void mqttport(int port){
    if (port > 0) mqtt_port = port;
  }

  const String mqttuser() const {
    return mqtt_user;
  }

  void mqttuser(const char* user){
    if (strlen(user) > 0) mqtt_user = user;
  }

  const String mqttpwrd() const {
    return mqtt_pwrd;
  }

  void mqttpwrd(const char* pwrd){
    mqtt_pwrd = pwrd;
  }

  void setupMQTT(AsyncMqttClient& mqttClient)
  {
    IPAddress mqtt_ip;

    if (mqtt_ip.fromString(mqtt_host))
    {
      mqttClient.setServer(mqtt_ip, mqtt_port.toInt());
    }
    else
    {
      mqttClient.setServer(mqtt_host.c_str(), mqtt_port.toInt());
    }
    
    mqttClient.setKeepAlive(5);
    mqttClient.setCredentials(mqtt_user.c_str(), (mqtt_pwrd.length() == 0) ? nullptr : mqtt_pwrd.c_str());
    mqttClient.setClientId(host_name.c_str());
  }

  void setupLog(AsyncEventSource* source)
  {
    webEvents = source;
  }

  void load()
  {
    //read configuration from FS json
    Serial.println("[SETTINGS] mounting FS...");

    if (SPIFFS.begin())
    {
      Serial.println("[SETTINGS] mounted file system");
      if (SPIFFS.exists("/config.json"))
      {
        //file exists, reading and loading
        Serial.println("[SETTINGS] reading config file");
        File configFile = SPIFFS.open("/config.json", "r");
        if (configFile)
        {
          Serial.println("[SETTINGS] opened config file");
          size_t size = configFile.size();
          // Allocate a buffer to store contents of the file..0

          std::unique_ptr<char[]> buf(new char[size]);

          configFile.readBytes(buf.get(), size);
          DynamicJsonBuffer jsonBuffer;
          JsonObject &json = jsonBuffer.parseObject(buf.get());
          if (json.success())
          {
            Serial.println("[SETTINGS] parsed json");
            json.printTo(Serial);

            if (json.containsKey("disp_name")) dispname(json["disp_name"]);
            if (json.containsKey("mqtt_host")) mqtthost(json["mqtt_host"]);
            if (json.containsKey("mqtt_port")) mqttport(json["mqtt_port"]);
            if (json.containsKey("mqtt_user")) mqttuser(json["mqtt_user"]);
            if (json.containsKey("mqtt_pwrd")) mqttpwrd(json["mqtt_pwrd"]);
          }
          else
          {
            Serial.println("[SETTINGS] failed to load json config");
          }
        }
      }
    }
    else
    {
      Serial.println("[SETTINGS] failed to mount FS");
    }
  }

  void save()
  {
    Serial.println("[MAIN] saving config");
    DynamicJsonBuffer jsonBuffer;
    JsonObject &json = jsonBuffer.createObject();

    json["disp_name"] = disp_name.c_str();
    json["mqtt_host"] = mqtt_host.c_str();
    json["mqtt_port"] = mqtt_port.toInt();
    json["mqtt_user"] = mqtt_user.c_str();
    json["mqtt_pwrd"] = mqtt_pwrd.c_str();

    File configFile = SPIFFS.open("/config.json", "w");
    if (!configFile)
    {
      Serial.println("[MAIN] failed to open config file for writing");
    }

    json.printTo(Serial);
    Serial.println();
    json.printTo(configFile);
    configFile.close();
  }

  void log(String message)
  {
    if (webEvents) 
    {
      webEvents->send(message.c_str(), "log");
    }

    Serial.println(message);
  }

  void onToggle(Delegate callback)
  {
    onToggleCalback = callback;
  }

  void onRestart(Delegate callback)
  {
    onRestartCalback = callback;
  }

  void onReset(Delegate callback)
  {
    onResetCalback = callback;
  }

  void onOpen(Delegate callback)
  {
    onOpenCalback = callback;
  }

  void onClose(Delegate callback)
  {
    onCloseCalback = callback;
  }

  String toJSON()
  {
    return "{\r\n \"open\":" + (String)isDoorOpen + "\r\n}";
  }

  void toggle()
  {
    if (onToggleCalback) onToggleCalback();
  }

  bool isOpen()
  {
    return isDoorOpen;
  }

  void open()
  {
    if (onOpenCalback)
    {
      onOpenCalback();
      isDoorOpen = true;
    }
  }

  void close()
  {
    if (onCloseCalback) 
    {
      onCloseCalback();
      isDoorOpen = false;
    }
  }

  void restart()
  {
    if (onRestartCalback) onRestartCalback();
  }

  void reset()
  {
    if (onResetCalback) onResetCalback();
  }
};

extern GarageClass Garage = GarageClass();

#endif
