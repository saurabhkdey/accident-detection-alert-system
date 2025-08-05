#include <SoftwareSerial.h>

// GSM Module
#define GSM_RX 2
#define GSM_TX 3
SoftwareSerial gsmSerial(GSM_RX, GSM_TX); // RX, TX

// Accelerometer Pins
#define X_PIN A0
#define Y_PIN A1
#define Z_PIN A2

// Accelerometer Parameters
const float ACCIDENT_THRESHOLD = 3.0; // 3g threshold for two-wheeler accident
const float STABLE_THRESHOLD = 1.2;   // Normal range (including gravity)
const unsigned long DEBOUNCE_TIME = 5000; // 5 seconds between accident detections

// System State
enum ArduinoState {
  WAITING_INIT,
  MONITORING,
  ACCIDENT_DETECTED,
  SENDING_SOS,
  COMPLETE
};
ArduinoState currentState = WAITING_INIT;

// Accident Data
struct AccidentData {
  float latitude;
  float longitude;
  String hospitalName;
  String hospitalPhone;
  String policeName;
  String policePhone;
  String mapLink;
};
AccidentData receivedData;

// Timing variables
unsigned long lastAccidentTime = 0;
bool accidentFlag = false;

void setup() {
  Serial.begin(9600); // Communication with ESP32
  
  // Initialize GSM
  gsmSerial.begin(9600);
  delay(1000); // Let GSM module initialize
  
  // Initialize accelerometer pins
  pinMode(X_PIN, INPUT);
  pinMode(Y_PIN, INPUT);
  pinMode(Z_PIN, INPUT);
  
  // Wait for initialization command from ESP32
  while (currentState == WAITING_INIT) {
    if (Serial.available()) {
      String command = Serial.readStringUntil('\n');
      command.trim();
      
      if (command.equals("INIT")) {
        initializeModules();
      }
    }
    delay(100);
  }
}

void loop() {
  switch(currentState) {
    case MONITORING:
      monitorAccelerometer();
      monitorESP32Messages();
      break;
      
    case ACCIDENT_DETECTED:
      notifyESP32();
      currentState = MONITORING; // Wait for ESP32 response
      break;
      
    case SENDING_SOS:
      sendEmergencyMessages();
      currentState = COMPLETE;
      break;
      
    case COMPLETE:
      // Do nothing, system has completed its task
      break;
  }
  
  delay(50); // Small delay to prevent flooding
}

void initializeModules() {
  Serial.println("Initializing modules...");
  
  // Check GSM connectivity
  bool gsmReady = checkGSM();
  
  // Check accelerometer
  bool accelReady = (analogRead(X_PIN) > 0 && analogRead(Y_PIN) > 0 && analogRead(Z_PIN) > 0);
  
  if (gsmReady && accelReady) {
    Serial.println("READY");
    currentState = MONITORING;
    Serial.println("All modules initialized");
    testSMS(); // Test SMS functionality after initialization
  } else {
    Serial.println("ERROR");
    Serial.print("GSM Ready: "); Serial.println(gsmReady);
    Serial.print("Accel Ready: "); Serial.println(accelReady);
    Serial.println("Module initialization failed");
  }
}

bool checkGSM() {
  // Clear any existing data in buffer
  while(gsmSerial.available()) {
    gsmSerial.read();
  }
  
  // Send AT command to check GSM
  gsmSerial.println("AT");
  delay(500);
  
  if(!waitForGSMResponse("OK", 2000)) {
    Serial.println("GSM not responding to AT command");
    return false;
  }
  
  // Check SIM card status
  gsmSerial.println("AT+CPIN?");
  if(!waitForGSMResponse("+CPIN: READY", 2000)) {
    Serial.println("SIM card not ready");
    return false;
  }
  
  // Check network registration
  gsmSerial.println("AT+CREG?");
  if(!waitForGSMResponse("+CREG: 0,1", 2000) && !waitForGSMResponse("+CREG: 0,5", 2000)) {
    Serial.println("Not registered to network");
    return false;
  }
  
  return true;
}

bool waitForGSMResponse(const char* expectedResponse, unsigned long timeout) {
  unsigned long startTime = millis();
  String response = "";
  
  while(millis() - startTime < timeout) {
    while(gsmSerial.available()) {
      char c = gsmSerial.read();
      response += c;
      
      if(response.indexOf(expectedResponse) >= 0) {
        return true;
      }
    }
  }
  
  // Print debug info if timeout occurs
  if (response.length() > 0) {
    Serial.print("GSM Response: ");
    Serial.println(response);
  } else {
    Serial.println("No response from GSM module");
  }
  
  return false;
}

void monitorAccelerometer() {
  // Read raw values
  int xRaw = analogRead(X_PIN);
  int yRaw = analogRead(Y_PIN);
  int zRaw = analogRead(Z_PIN);
  
  // Convert to voltage (3.3V reference)
  float xVoltage = xRaw * (3.3 / 1023.0);
  float yVoltage = yRaw * (3.3 / 1023.0);
  float zVoltage = zRaw * (3.3 / 1023.0);
  
  // Convert to acceleration (g)
  float xAccel = (xVoltage - 1.65) / 0.3; // 300mV/g sensitivity
  float yAccel = (yVoltage - 1.65) / 0.3;
  float zAccel = (zVoltage - 1.65) / 0.3;
  
  // Calculate resultant acceleration (excluding gravity)
  float resultant = sqrt(xAccel*xAccel + yAccel*yAccel + zAccel*zAccel);
  
  // Check for accident (with debounce)
  if (resultant > ACCIDENT_THRESHOLD && 
      (millis() - lastAccidentTime > DEBOUNCE_TIME || lastAccidentTime == 0)) {
    Serial.print("Accident detected! Acceleration: ");
    Serial.println(resultant);
    lastAccidentTime = millis();
    currentState = ACCIDENT_DETECTED;
  }
}

void monitorESP32Messages() {
  if (Serial.available()) {
    String message = Serial.readStringUntil('\n');
    message.trim();
    
    if (message.startsWith("SOS_DATA:")) {
      parseSOSData(message);
      currentState = SENDING_SOS;
    }
  }
}

void notifyESP32() {
  Serial.println("ACCIDENT");
  Serial.println("Notified ESP32 about accident");
}

void parseSOSData(String data) {
  // Remove "SOS_DATA:" prefix
  data = data.substring(9);
  
  // Find positions of first two commas (for lat/long)
  int firstComma = data.indexOf(',');
  int secondComma = data.indexOf(',', firstComma + 1);
  
  // Extract latitude and longitude
  receivedData.latitude = data.substring(0, firstComma).toFloat();
  receivedData.longitude = data.substring(firstComma + 1, secondComma).toFloat();
  
  // The remaining data contains text fields
  String remainingData = data.substring(secondComma + 1);
  
  // Find the positions of the remaining delimiters
  int parts[4];
  parts[0] = remainingData.indexOf(',');
  for (int i = 1; i < 4; i++) {
    parts[i] = remainingData.indexOf(',', parts[i-1] + 1);
  }
  
  // Extract all text fields
  receivedData.hospitalName = remainingData.substring(0, parts[0]);
  receivedData.hospitalPhone = remainingData.substring(parts[0] + 1, parts[1]);
  receivedData.policeName = remainingData.substring(parts[1] + 1, parts[2]);
  receivedData.policePhone = remainingData.substring(parts[2] + 1, parts[3]);
  receivedData.mapLink = remainingData.substring(parts[3] + 1);
  
  // Debug output
  Serial.println("Parsed Data:");
  Serial.print("Lat: "); Serial.println(receivedData.latitude, 6);
  Serial.print("Long: "); Serial.println(receivedData.longitude, 6);
  Serial.print("Hospital: "); Serial.println(receivedData.hospitalName);
  Serial.print("H Phone: "); Serial.println(receivedData.hospitalPhone);
  Serial.print("Police: "); Serial.println(receivedData.policeName);
  Serial.print("P Phone: "); Serial.println(receivedData.policePhone);
  Serial.print("Map: "); Serial.println(receivedData.mapLink);
}

void sendEmergencyMessages() {
  // Format concise emergency message to fit SMS limits
  String message = "ACCIDENT ALERT!\nVehicle Number: PB08XX1234\n";
  message += "Loc: " + String(receivedData.latitude, 6) + "," + String(receivedData.longitude, 6);
  message += "\nMap: " + receivedData.mapLink;
  message += "\nHospital: " + receivedData.hospitalPhone;
  message += "\nPolice: " + receivedData.policePhone;

  // Try sending with multiple attempts
  for (int attempt = 1; attempt <= 3; attempt++) {
    if (sendSMS("+917235819991", message)) { // Replace with actual emergency number
      Serial.println("SOS_SENT");
      return;
    }
    Serial.print("Attempt "); Serial.print(attempt); Serial.println(" failed");
    
    // Check network status between attempts
    if (!isNetworkAvailable()) {
      Serial.println("No network available. Retrying...");
    }
    
    delay(attempt * 5000); // Exponential backoff (5s, 10s, 15s)
  }
  
  Serial.println("SOS_FAILED");
}

bool sendSMS(String number, String message) {
  // Clear any pending data in the buffer
  while(gsmSerial.available()) {
    gsmSerial.read();
  }
  
  // Set SMS mode to text
  gsmSerial.println("AT+CMGF=1");
  delay(500);
  if(!waitForGSMResponse("OK", 2000)) {
    Serial.println("Failed to set text mode");
    return false;
  }
  
  // Set recipient number
  gsmSerial.print("AT+CMGS=\"");
  gsmSerial.print(number);
  gsmSerial.println("\"");
  delay(500);
  if(!waitForGSMResponse(">", 3000)) {
    Serial.println("Failed to set recipient");
    return false;
  }
  
  // Ensure message fits in one SMS
  int maxChunkSize = 150; // Conservative limit
  if(message.length() > maxChunkSize) {
    message = message.substring(0, maxChunkSize);
    Serial.println("Message truncated to fit SMS limit");
  }
  
  // Send message with proper delays
  gsmSerial.print(message);
  delay(100); // Short delay between characters
  
  // End with Ctrl+Z
  gsmSerial.write(26);
  delay(2000); // Important delay before checking response
  
  // Wait for confirmation with extended timeout
  if(waitForGSMResponse("+CMGS:", 15000)) {
    Serial.println("SMS sent successfully");
    return true;
  } else {
    Serial.println("SMS sending failed - no confirmation received");
    return false;
  }
}

bool isNetworkAvailable() {
  gsmSerial.println("AT+CREG?");
  if(waitForGSMResponse("+CREG: 0,1", 2000) || waitForGSMResponse("+CREG: 0,5", 2000)) {
    return true;
  }
  return false;
}

void testSMS() {
  Serial.println("Testing SMS functionality...");
  if(sendSMS("+917235819991", "System test message")) { // Replace with test number
    Serial.println("TEST_SMS_SUCCESS");
  } else {
    Serial.println("TEST_SMS_FAILED");
    
    // Additional diagnostics
    Serial.println("Running diagnostic checks...");
    gsmSerial.println("AT+CSQ");
    waitForGSMResponse("OK", 2000);
    gsmSerial.println("AT+COPS?");
    waitForGSMResponse("OK", 2000);
  }
}