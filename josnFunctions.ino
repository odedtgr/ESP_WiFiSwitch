bool processJson(String message) {
  Serial.println(" message: " + message);
  StaticJsonBuffer<BUFFER_SIZE> jsonBuffer;

  JsonObject& root = jsonBuffer.parseObject(message);

  if (!root.success()) {
    Serial.println("parseObject() failed");
    return false;
  }

//  if (root.containsKey("state")) {
//    if (strcmp(root["state"], on_cmd) == 0) {
//      stateOn = true;
//    }
//    else if (strcmp(root["state"], off_cmd) == 0) {
//      stateOn = false;
//    }
}
