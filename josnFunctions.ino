bool processJson(String message) {
  Debugln("jsonFunctions:  message: " + message);
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Debugln("jsonFunctions: parseObject() failed");
    return false;
  }
  if (strcmp(root["device_on"], "true") == 0){
    digitalWrite(OUTPIN, 1);
    Debugln("jsonFunctions: Switching light on"); 
  }else if (strcmp(root["device_on"], "false") == 0){
    digitalWrite(OUTPIN, 0);
    Debugln("jsonFunctions: Switching light off");  
  }
}
