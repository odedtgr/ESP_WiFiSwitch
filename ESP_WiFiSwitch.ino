/*
    This sketch is running a web server for configuring WiFI if can't connect or for controlling of one GPIO to switch a light/LED
    Also it supports to change the state of the light via MQTT message and gives back the state after change.
    The push button has to switch to ground. It has following functions: Normal press less than 1 sec but more than 50ms-> Switch light. Restart press: 3 sec -> Restart the module. Reset press: 20 sec -> Clear the settings in EEPROM
    While a WiFi config is not set or can't connect:
      http://server_ip will give a config page with
    While a WiFi config is set:
      http://server_ip/gpio -> Will display the GIPIO state and a switch form for it
      http://server_ip/gpio?state=0 -> Will change the GPIO directly and display the above aswell
      http://server_ip/cleareeprom -> Will reset the WiFi setting and rest to configure mode as AP
    server_ip is the IP address of the ESP8266 module, will be
    printed to Serial when the module is connected. (most likly it will be 192.168.4.1)
   To force AP config mode, press button 20 Secs!
    For several snippets used, the credit goes to:
    - https://github.com/esp8266
    - https://github.com/chriscook8/esp-arduino-apboot
    - https://github.com/knolleary/pubsubclient
    - https://github.com/vicatcu/pubsubclient <- Currently this needs to be used instead of the origin
    - https://gist.github.com/igrr/7f7e7973366fc01d6393
    - http://www.esp8266.com/viewforum.php?f=25
    - http://www.esp8266.com/viewtopic.php?f=29&t=2745
    - And the whole Arduino and ESP8266 comunity
*/

#define DEBUG
//#define WEBOTA
//debug added for information, change this according your needs

#ifdef DEBUG
#define Debug(x)    Serial.print(x)
#define Debugln(x)  Serial.println(x)
#define Debugf(...) Serial.printf(__VA_ARGS__)
#define Debugflush  Serial.flush
#else
#define Debug(x)    {}
#define Debugln(x)  {}
#define Debugf(...) {}
#define Debugflush  {}
#endif


#include <ESP8266WiFi.h>
#include <ESP8266mDNS.h>
#include <WiFiClient.h>
#include <ESP8266WebServer.h>
//#include <EEPROM.h>
#include <Ticker.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "FS.h"

extern "C" {
#include "user_interface.h" //Needed for the reset command
}

//***** Settings declare *********************************************************************************************************
String hostName = "WiFiSwitch"; //The MQTT ID -> MAC adress will be added to make it kind of unique
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
int iotMode = 1; //IOT mode: 0 = Web control, 1 = MQTT (No const since it can change during runtime)
//select GPIO's
#define OUTPIN 13 //output pin
#define INPIN 0  //12 input pin (push button)

#define RESTARTDELAY 3 //minimal time in sec for button press to reset
#define HUMANPRESSDELAY 50 // the delay in ms untill the press should be handled as a normal push by human. Button debounce. !!! Needs to be less than RESTARTDELAY & RESETDELAY!!!
#define RESETDELAY 10 //Minimal time in sec for button press to reset all settings and boot to config mode

#define MAX_JSON_SIZE 200

//##### Object instances #####
MDNSResponder mdns;
ESP8266WebServer server(80);
WiFiClient wifiClient;
PubSubClient mqttClient;
Ticker btn_timer;
Ticker otaTickLoop;

//##### Flags ##### They are needed because the loop needs to continue and cant wait for long tasks!
int rstNeed = 0; // Restart needed to apply new settings
int toPub = 0; // determine if state should be published.
int configToClear = 0; // determine if config should be cleared.
int otaFlag = 0;
boolean inApMode = 0;
//##### Global vars #####
int webtypeGlob;
int otaCount = 300; //imeout in sec for OTA mode
int current; //Current state of the button
unsigned long count = 0; //Button press time counter
String st; //WiFi Stations HTML list
String state; //State of light
char buf[40]; //For MQTT data recieve
char* host; //The DNS hostname
//To be read from Config file loadConfig()
String esid = ""; 
String epass = ""; 
String pubTopic ; //="HomeWise/test_light";
String subTopic ; //="HomeWise/out/test_light";;
String mqttServer = ""; //"oded.noip.me";
const char* otaServerIndex = "<form method='POST' action='/update' enctype='multipart/form-data'><input type='file' name='update'><input type='submit' value='Update'></form>";

//-------------- void's -------------------------------------------------------------------------------------
void setup() {
  Serial.begin(115200);
  delay(10);
  // prepare GPIO2
  pinMode(OUTPIN, OUTPUT);
  pinMode(INPIN, INPUT_PULLUP);
  btn_timer.attach(0.05, btn_handle);
  Debugln("DEBUG: Entering loadConfig()");
  if (!SPIFFS.begin()) {
    Debugln("Failed to mount file system");
  }

  uint8_t mac[6];
  WiFi.macAddress(mac);
  hostName += "-";
  hostName += macToStr(mac);
  String hostTemp = hostName;
  hostTemp.replace(":", "-");
  host = (char*) hostTemp.c_str();
  loadConfig();  //this is commented if parameters are static
  //loadConfigOld();
  Debugln("DEBUG: loadConfig() passed");

  // Connect to WiFi network
  if (esid==""){
    Debugln("setup: esid is empty.");
    setupAP(); 
  }else{
  initWiFi(); // TODO: test if wifi connects after AP stage. is this line needed?
  }
}


void btn_handle()
{
  if (!digitalRead(INPIN)) {
    ++count; // one count is 50ms
  } else {
    if (count > 1 && count < HUMANPRESSDELAY / 5) { //push between 50 ms and 1 sec
      Debug("button pressed ");
      Debug(count * 0.05);
      Debugln(" Sec.");

      Debug("Light is ");
      Debugln(digitalRead(OUTPIN));

      Debug("Switching light to ");
      Debugln(!digitalRead(OUTPIN));
      digitalWrite(OUTPIN, !digitalRead(OUTPIN));
      state = digitalRead(OUTPIN);
      if (iotMode == 1 && mqttClient.connected()) {
        toPub = 1;
        Debugln("DEBUG: toPub set to 1");
      }
    } else if (count > (RESTARTDELAY / 0.05) && count <= (RESETDELAY / 0.05)) { //pressed 3 secs (60*0.05s)
      Debug("button pressed ");
      Debug(count * 0.05);
      Debugln(" Sec. Restarting!");
      setOtaFlag(!otaFlag);
      ESP.reset();
    } else if (count > (RESETDELAY / 0.05)) { //pressed 20 secs
      Debug("button pressed ");
      Debug(count * 0.05);
      Debugln(" Sec.");
      Debugln(" Clear settings and resetting!");
      configToClear = 1;
    }
    count = 0; //reset since we are at high
  }
}



//-------------------------------- Main loop ---------------------------
void loop() {
  //Debugln("Main loop() begin");
  if (configToClear == 1) {
    //Debugln("DEBUG: loop() clear config flag set!");
    clearConfig() ? Debugln("Config cleared!") : Debugln("Config could not be cleared");
    delay(1000);
    ESP.reset();
  }
  //Debugln("DEBUG: config reset check passed");
  if (WiFi.status() == WL_CONNECTED && otaFlag) {
    if (otaCount <= 1) {
      Debugln("OTA mode time out. Reset!");
      setOtaFlag(0);
      ESP.reset();
      delay(100);
    }
    server.handleClient();
    delay(1);
  } else if (WiFi.status() == WL_CONNECTED || webtypeGlob == 1) {
    //Debugln("DEBUG: loop() wifi connected & webServer ");
    if (iotMode == 0 || webtypeGlob == 1) {
      //Debugln("DEBUG: loop() Web mode requesthandling ");
      server.handleClient();
      delay(1);
    } else if (iotMode == 1 && webtypeGlob != 1 && otaFlag != 1) {
      //Debugln("DEBUG: loop() MQTT mode requesthandling ");
      if (!connectMQTT()) {
        Debugln("mqtt Not connected!");
        delay(200);
      }else{
        //Debugln("mqtt handler");
        mqtt_handler();  
      }
    }
  } else {
    Debugln("Main loop - WiFi not connected");
    delay(60000);
    initWiFi(); //Try to connect again
  }
  //Debugln("main loop() end");
}
