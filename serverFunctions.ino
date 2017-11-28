void initWiFi() {
  Debugln();
  Debugln();
  Debugln("serverFunctions: initWifi() called");

  // test esid
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_STA);
  Debug("serverFunctions: Connecting to WiFi ");
  Debugln(esid);
  Debugln(epass);
  WiFi.begin((char*)esid.c_str(), (char*)epass.c_str());
  if ( testWifi() == 20 ) {
    launchWeb(0);
    return;
  }
}

int testWifi(void) {
  int c = 0;
  Debugln("Wifi test...");
  while ( c < 30 ) {
    if (WiFi.status() == WL_CONNECTED) {
      Debug("serverFunctions: testWifi() connected to WiFi with IP ");
      Debugln(WiFi.localIP());
      return (20);
    }
    delay(500);
    Debug(".");
    c++;
  }
  Debugln("serverFunctions: WiFi Connect timed out!");
  return (10);
}


void setupAP(void) {
  Debugln("serverFunctions: setupAP()");
  WiFi.mode(WIFI_STA);
  wifi_station_disconnect();
  WiFi.disconnect();
  delay(100);
  int n = WiFi.scanNetworks();
  Debugln("serverFunctions: scan done");
  if (n == 0) {
    Debugln("serverFunctions: no networks found");
    st = "<b>No networks found:</b>";
  } else {
    Debug(n);
    Debugln("serverFunctions:  Networks found");
    st = "<ul>";
    for (int i = 0; i < n; ++i)
    {
      // Print SSID and RSSI for each network found
      Debug(i + 1);
      Debug(": ");
      Debug(WiFi.SSID(i));
      Debug(" (");
      Debug(WiFi.RSSI(i));
      Debug(")");
      Debugln((WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " (OPEN)" : "*");

      // Print to web SSID and RSSI for each network found
      st += "<li>";
      st += i + 1;
      st += ": ";
      st += WiFi.SSID(i);
      st += " (";
      st += WiFi.RSSI(i);
      st += ")";
      st += (WiFi.encryptionType(i) == ENC_TYPE_NONE) ? " (OPEN)" : "*";
      st += "</li>";
      delay(10);
    }
    st += "</ul>";
  }
  Debugln("");
  WiFi.disconnect();
  delay(100);
  WiFi.mode(WIFI_AP);
  WiFi.softAP(host);
  WiFi.begin(host); // not sure if need but works
  Debug("serverFunctions: Access point started with name ");
  Debugln(host);
  inApMode = 1;
  launchWeb(1);
}

void launchWeb(int webtype) {
  Debugf("serverFunctions: launchWeb(%d)\n", webtype);
  //Start the web server or MQTT
  if (otaFlag == 1 && !inApMode) {//ota mode
    Debugln("serverFunctions: Starting OTA mode.");
    Debugf("serverFunctions: Sketch size: %u\n", ESP.getSketchSize());
    Debugf("Free size: %u\n", ESP.getFreeSketchSpace());
    MDNS.begin(host);
    server.on("/", HTTP_GET, []() {
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/html", otaServerIndex);
    });
    server.on("/update", HTTP_POST, []() {
      server.sendHeader("Connection", "close");
      server.sendHeader("Access-Control-Allow-Origin", "*");
      server.send(200, "text/plain", (Update.hasError()) ? "FAIL" : "OK");
      setOtaFlag(0);
      ESP.restart();
    }, []() {
      HTTPUpload& upload = server.upload();
      if (upload.status == UPLOAD_FILE_START) {
        //Serial.setDebugOutput(true);
        WiFiUDP::stopAll();
        Debugf("Update: %s\n", upload.filename.c_str());
        otaCount = 300;
        uint32_t maxSketchSpace = (ESP.getFreeSketchSpace() - 0x1000) & 0xFFFFF000;
        if (!Update.begin(maxSketchSpace)) { //start with max available size
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_WRITE) {
        if (Update.write(upload.buf, upload.currentSize) != upload.currentSize) {
          Update.printError(Serial);
        }
      } else if (upload.status == UPLOAD_FILE_END) {
        if (Update.end(true)) { //true to set the size to the current progress
          Debugf("Update Success: %u\nRebooting...\n", upload.totalSize);
        } else {
          Update.printError(Serial);
        }
        Serial.setDebugOutput(false);
      }
      yield();
    });
    server.begin();
    Debugf("Ready! Open http://%s.local in your browser\n", host);
    MDNS.addService("http", "tcp", 80);
    otaTickLoop.attach(1, otaCountown);
  } else { //not in ota mode
    if (webtype == 1 || iotMode == 0) { //in config mode or WebControle
      if (webtype == 1) { //in config mode
        webtypeGlob == 1;
        Debugln(WiFi.softAPIP());
        server.on("/", webHandleConfig);
        server.on("/a", webHandleConfigSave);
      } else { //in WebControle mode
        //setup DNS since we are a client in WiFi net
        if (!MDNS.begin(host)) {
          Debugln("serverFunctions: Error setting up MDNS responder!");
        } else {
          Debugln("serverFunctions: mDNS responder started");
          MDNS.addService("http", "tcp", 80);
        }
        Debugln(WiFi.localIP());
        server.on("/", webHandleRoot);
        server.on("/cleareeprom", webHandleClearRom);
        server.on("/gpio", webHandleGpio);
      }
      //server.onNotFound(webHandleRoot);
      server.begin();
      Debugln("serverFunctions: Web server started");
      webtypeGlob = webtype; //Store global to use in loop()
    } else if (webtype != 1 && iotMode == 1) { // in MQTT and not in config mode
      mqttClient.setServer((char*) mqttServer.c_str(), 1883);
      mqttClient.setCallback(mqtt_arrived);
      mqttClient.setClient(wifiClient);
      if (WiFi.status() == WL_CONNECTED) {
        if (!connectMQTT()) {
          Debugln("serverFunctions: Could not connect MQTT. Delay 2 sec.");
          delay(2000);
        }
      }
    }
  }
}

void webHandleConfig() {
  IPAddress ip = WiFi.softAPIP();
  String ipStr = String(ip[0]) + '.' + String(ip[1]) + '.' + String(ip[2]) + '.' + String(ip[3]);
  String s;

  s = "Configuration of " + hostName + " at ";
  s += ipStr;
  s += "<p>";
  s += st;
  s += "<form method='get' action='a'>";
  s += "<label>SSID: </label><input name='ssid' length=32><label> Pass: </label><input name='pass' type='password' length=64></br>";
  s += "The following is not ready yet!</br>";
  s += "<label>IOT mode: </label><input type='radio' name='iot' value='0'> HTTP<input type='radio' name='iot' value='1' checked> MQTT</br>";
  s += "<label>MQTT Broker IP/DNS: </label><input name='host' length=15></br>";
  s += "<label>MQTT Publish topic: </label><input name='pubtop' length=64></br>";
  s += "<label>MQTT Subscribe topic: </label><input name='subtop' length=64></br>";
  s += "<input type='submit'></form></p>";
  s += "\r\n\r\n";
  Debugln("serverFunctions: Sending 200");
  server.send(200, "text/html", s);
}

void webHandleConfigSave() {
  // /a?ssid=blahhhh&pass=poooo
  String s;
  s = "<p>Settings saved to eeprom and reset to boot into new settings</p>\r\n\r\n";
  server.send(200, "text/html", s);
  Debugln("serverFunctions: clearing EEPROM.");
  clearConfig();
  String qsid;
  qsid = server.arg("ssid");
  qsid.replace("%2F", "/");
  Debugln("serverFunctions: Got SSID: " + qsid);
  esid = (char*) qsid.c_str();

  String qpass;
  qpass = server.arg("pass");
  qpass.replace("%2F", "/");
  Debugln("serverFunctions: Got pass: " + qpass);
  epass = (char*) qpass.c_str();

  String qiot;
  qiot = server.arg("iot");
  Debugln("serverFunctions: Got iot mode: " + qiot);
  qiot == "0" ? iotMode = 0 : iotMode = 1 ;

  String qsubTop;
  qsubTop = server.arg("subtop");
  qsubTop.replace("%2F", "/");
  Debugln("serverFunctions: Got subtop: " + qsubTop);
  subTopic = (char*) qsubTop.c_str();

  String qpubTop;
  qpubTop = server.arg("pubtop");
  qpubTop.replace("%2F", "/");
  Debugln("serverFunctions: Got pubtop: " + qpubTop);
  pubTopic = (char*) qpubTop.c_str();

  mqttServer = (char*) server.arg("host").c_str();
  Debug("serverFunctions: Got mqtt Server: ");
  Debugln(mqttServer);

  Serial.print("Settings written ");
  saveConfig() ? Serial.println("sucessfully.") : Serial.println("not succesfully!");;
  Serial.println("Restarting!");
  delay(1000);
  ESP.reset();
}

void webHandleRoot() {
  String s;
  s = "<p>Hello from ESP8266";
  s += "</p>";
  s += "<a href=\"/gpio\">Controle GPIO</a><br />";
  s += "<a href=\"/cleareeprom\">Clear settings an boot into Config mode</a><br />";
  s += "\r\n\r\n";
  Debugln("serverFunctions: Sending 200");
  server.send(200, "text/html", s);
}

void webHandleClearRom() {
  String s;
  s = "<p>Clearing the config and reset to configure new wifi<p>";
  s += "</html>\r\n\r\n";
  Debugln("serverFunctions: Sending 200");
  server.send(200, "text/html", s);
  Debugln("serverFunctions: clearing config");
  clearConfig();
  delay(10);
  Debugln("serverFunctions: Done, restarting!");
  ESP.reset();
}

void webHandleGpio() {
  String s;
  // Set GPIO according to the request
  if (server.arg("state") == "1" || server.arg("state") == "0" ) {
    int state = server.arg("state").toInt();
    digitalWrite(OUTPIN, state);
    Debug("serverFunctions: Light switched via web request to  ");
    Debugln(state);
  }
  s = "Light is now ";
  s += (digitalRead(OUTPIN)) ? "on" : "off";
  s += "<p>Change to <form action='gpio'><input type='radio' name='state' value='1' ";
  s += (digitalRead(OUTPIN)) ? "checked" : "";
  s += ">On<input type='radio' name='state' value='0' ";
  s += (digitalRead(OUTPIN)) ? "" : "checked";
  s += ">Off <input type='submit' value='Submit'></form></p>";
  server.send(200, "text/html", s);
}

