boolean connectMQTT(){
  if (mqttClient.connected()){
    return true;
  }  
  
  Debug("mqttFunctions: Connecting to MQTT server ");
  Debug(mqttServer);
  Debug(" as ");
  Debugln(host);
  
  if (mqttClient.connect(host)) {
    Debugln("mqttFunctions: Connected to MQTT broker");
    if(mqttClient.subscribe((char*)subTopic.c_str())){
      Debugln("mqttFunctions: Subsribed to topic.");
    } else {
      Debugln("mqttFunctions: NOT subsribed to topic!");      
    }
    return true;
  }
  else {
    Debugln("mqttFunctions: MQTT connect failed! ");
    return false;
  }
}

void disconnectMQTT(){
  mqttClient.disconnect();
}

void mqtt_handler(){
  if (toPub==1){
    Debugln("mqttFunctions: Publishing state via MQTT");
    if(pubState()){
     toPub=0; 
    }
  }
  mqttClient.loop();
  delay(100); //let things happen in background
}

void mqtt_arrived(char* subTopic, byte* payload, unsigned int length) { // handle messages arrived 
  int i = 0;
  Debugln("mqttFunctions: MQTT message arrived:  topic: " + String(subTopic));
    // create character buffer with ending null terminator (string)
  for(i=0; i<length; i++) {    
    buf[i] = payload[i];
  }
  buf[i] = '\0';
  String msgString = String(buf);
  processJson(msgString);
  toPub = 1;  
}

boolean pubState(){ //Publish the current state of the light    
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  if (!connectMQTT()){
      delay(100);
      if (!connectMQTT){                            
        Debugln("mqttFunctions: Could not connect MQTT.");
        //Serial.println("Publish state NOK");
        return false;
      }
    }
    if (mqttClient.connected()){      
        String device_state = (digitalRead(OUTPIN))?"true":"false";
        JsonObject& root = jsonBuffer.createObject();
        root["device_on"] = device_state;
        char buffer[root.measureLength() + 1];
        root.printTo(buffer, sizeof(buffer));
        Debugln("mqttFunctions: To publish state " + (String)buffer );  
      if (mqttClient.publish((char*)pubTopic.c_str(), buffer)) {
        Debugln("mqttFunctions: Publish state OK");        
        return true;
      } else {
        Debugln("mqttFunctions: Publish state NOK");        
        return false;
      }
     } else {
         Debugln("mqttFunctions: Publish state NOK");
         Debugln("mqttFunctions: No MQTT connection.");        
     }    
}
