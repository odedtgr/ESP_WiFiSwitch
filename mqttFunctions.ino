boolean connectMQTT(){
  if (mqttClient.connected()){
    return true;
  }  
  
  Serial.print("Connecting to MQTT server ");
  Serial.print(mqttServer);
  Serial.print(" as ");
  Serial.println(host);
  
  if (mqttClient.connect(host)) {
    Serial.println("Connected to MQTT broker");
    if(mqttClient.subscribe((char*)subTopic.c_str())){
      Serial.println("Subsribed to topic.");
    } else {
      Serial.println("NOT subsribed to topic!");      
    }
    return true;
  }
  else {
    Serial.println("MQTT connect failed! ");
    return false;
  }
}

void disconnectMQTT(){
  mqttClient.disconnect();
}

void mqtt_handler(){
  if (toPub==1){
    Debugln("DEBUG: Publishing state via MWTT");
    if(pubState()){
     toPub=0; 
    }
  }
  mqttClient.loop();
  delay(100); //let things happen in background
}

void mqtt_arrived(char* subTopic, byte* payload, unsigned int length) { // handle messages arrived 
  int i = 0;
  Serial.print("MQTT message arrived:  topic: " + String(subTopic));
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
        //Serial.println("Could not connect MQTT.");
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
        Serial.println("To publish state " + (String)buffer );  
      if (mqttClient.publish((char*)pubTopic.c_str(), buffer)) {
        Serial.println("Publish state OK");        
        return true;
      } else {
        Serial.println("Publish state NOK");        
        return false;
      }
     } else {
         Serial.println("Publish state NOK");
         Serial.println("No MQTT connection.");        
     }    
}
