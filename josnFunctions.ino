bool processJson(String message) {
  Serial.println(" message: " + message);
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }
  if (strcmp(root["device_on"], "true") == 0){
    digitalWrite(OUTPIN, 1);
    Serial.print("Switching light on "); 
  }else if (strcmp(root["device_on"], "false") == 0){
    digitalWrite(OUTPIN, 0);
    Serial.print("Switching light off"); 
  }
}
