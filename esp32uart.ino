#include <HardwareSerial.h>
#include <TinyGPS++.h>
#include <WiFi.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>

// GPS Module
HardwareSerial gpsSerial(1); // UART1 on pins 16(RX), 17(TX)
TinyGPSPlus gps;

// UART Communication with Arduino
#define ARDUINO_RX 21
#define ARDUINO_TX 22
HardwareSerial arduinoSerial(2); // UART2

// WiFi Credentials
const char* ssid = "Saurabh's S23";
const char* password = "soojabsdk";

// Flask Server
const String serverBaseUrl = "http://192.168.31.147:5000";

// Emergency Contacts
const String familyNumber = "+917235819991"; // Replace with actual number
const String defaultPoliceNumber = "100";
const String defaultHospitalNumber = "108";

// System State
enum SystemState {
  INIT,
  GPS_READY,
  WAITING_FOR_ARDUINO,
  ACCIDENT_DETECTED,
  SENDING_SOS,
  COMPLETE
};
SystemState currentState = INIT;

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
AccidentData accidentInfo;

// Timing variables
unsigned long lastAccidentCheck = 0;
const unsigned long accidentCheckInterval = 100; // ms

void setup() {
  Serial.begin(115200);
  
  // Initialize UART with Arduino
  arduinoSerial.begin(9600, SERIAL_8N1, ARDUINO_RX, ARDUINO_TX);
  
  // Initialize GPS
  gpsSerial.begin(9600, SERIAL_8N1, 16, 17);
  
  // Connect to WiFi
  connectToWiFi();
  
  // Check GPS connectivity
  if (checkGPS()) {
    currentState = GPS_READY;
    // Command Arduino to initialize its modules
    initializeArduino();
  } else {
    Serial.println("System halted due to GPS failure");
    while(1); // Stop execution
  }
}

void loop() {
  // Update GPS data
  while (gpsSerial.available() > 0) {
    gps.encode(gpsSerial.read());
  }

  switch(currentState) {
    case GPS_READY:
      monitorArduinoMessages();
      break;
      
    case ACCIDENT_DETECTED:
      handleAccident();
      break;
      
    case SENDING_SOS:
      // Waiting for completion
      break;
      
    case COMPLETE:
      // System has completed its task
      break;
  }
  
  delay(10); // Small delay to prevent watchdog timer issues
}

// WiFi Connection
void connectToWiFi() {
  Serial.println("Connecting to WiFi...");
  WiFi.begin(ssid, password);
  
  int attempts = 0;
  while (WiFi.status() != WL_CONNECTED && attempts < 20) {
    delay(500);
    Serial.print(".");
    attempts++;
  }
  
  if (WiFi.status() == WL_CONNECTED) {
    Serial.println("\nWiFi connected");
    Serial.print("IP Address: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("\nWiFi connection failed");
    // System can still work without WiFi (using default numbers)
  }
}

// GPS Functions
bool checkGPS() {
  Serial.println("Initializing GPS module...");
  unsigned long startTime = millis();
  bool gpsReady = false;
  
  while (millis() - startTime < 5000) { // 5 second timeout
    while (gpsSerial.available() > 0) {
      if (gps.encode(gpsSerial.read())) {
        if (gps.location.isValid()) {
          gpsReady = true;
          break;
        }
      }
    }
    if (gpsReady) break;
    delay(100);
  }
  
  if (gpsReady) {
    Serial.println("GPS module ready");
    return true;
  } else {
    Serial.println("GPS module not responding");
    return false;
  }
}

bool getCurrentLocation() {
  unsigned long startTime = millis();
  
  while (millis() - startTime < 10000) { // 10 second timeout
    while (gpsSerial.available() > 0) {
      gps.encode(gpsSerial.read());
    }
    
    if (gps.location.isValid() && gps.location.age() < 2000) { // Data less than 2 seconds old
      accidentInfo.latitude = gps.location.lat();
      accidentInfo.longitude = gps.location.lng();
      accidentInfo.mapLink = "https://maps.google.com/?q=" + 
                            String(accidentInfo.latitude, 6) + "," + 
                            String(accidentInfo.longitude, 6);
      
      Serial.print("Location updated: ");
      Serial.print(accidentInfo.latitude, 6);
      Serial.print(", ");
      Serial.println(accidentInfo.longitude, 6);
      return true;
    }
    delay(100);
  }
  
  Serial.println("Failed to get current location");
  return false;
}

// Arduino Communication
void initializeArduino() {
  Serial.println("Initializing Arduino modules...");
  arduinoSerial.println("INIT"); // Send initialization command
  
  unsigned long startTime = millis();
  bool arduinoReady = false;
  
  while (millis() - startTime < 5000) {
    if (arduinoSerial.available()) {
      String response = arduinoSerial.readStringUntil('\n');
      response.trim();
      
      if (response.equals("READY")) {
        arduinoReady = true;
        break;
      } else if (response.equals("ERROR")) {
        break;
      }
    }
    delay(100);
  }
  
  if (arduinoReady) {
    Serial.println("Arduino modules initialized");
    currentState = GPS_READY;
  } else {
    Serial.println("Arduino initialization failed - using ESP32 as backup");
    // System can still work with ESP32 as backup
    currentState = GPS_READY;
  }
}

void monitorArduinoMessages() {
  if (arduinoSerial.available()) {
    String message = arduinoSerial.readStringUntil('\n');
    message.trim();
    
    if (message.equals("ACCIDENT")) {
      Serial.println("Accident detected by Arduino");
      currentState = ACCIDENT_DETECTED;
    }
  }
}

// Accident Handling
void handleAccident() {
  // 1. Get current location
  if (!getCurrentLocation()) {
    // If we can't get current location, use last known
    if (gps.location.isValid()) {
      accidentInfo.latitude = gps.location.lat();
      accidentInfo.longitude = gps.location.lng();
      accidentInfo.mapLink = "https://maps.google.com/?q=" + 
                            String(accidentInfo.latitude, 6) + "," + 
                            String(accidentInfo.longitude, 6);
      Serial.println("Using last known location");
    } else {
      Serial.println("No location data available");
      accidentInfo.mapLink = "Location data unavailable";
    }
  }
  
  // 2. Fetch emergency services info (if WiFi available)
  if (WiFi.status() == WL_CONNECTED) {
    fetchEmergencyServicesInfo();
  } else {
    Serial.println("Using default emergency numbers");
    accidentInfo.hospitalName = "Nearest Hospital";
    accidentInfo.hospitalPhone = defaultHospitalNumber;
    accidentInfo.policeName = "Local Police";
    accidentInfo.policePhone = defaultPoliceNumber;
  }
  
  // 3. Send data to Arduino
  sendDataToArduino();
  
  // 4. If Arduino doesn't respond, send directly via ESP32
  if (!waitForArduinoConfirmation()) {
    Serial.println("Arduino not responding, sending SOS directly from ESP32");
    sendSOSDirectly();
  }
  
  currentState = COMPLETE;
  Serial.println("Emergency procedure complete");
}

bool fetchEmergencyServicesInfo() {
  if (WiFi.status() != WL_CONNECTED) {
    return false;
  }
  
  HTTPClient http;
  String url = serverBaseUrl + "/get_places?latitude=" + String(accidentInfo.latitude, 6) + 
               "&longitude=" + String(accidentInfo.longitude, 6);
  
  http.begin(url);
  int httpCode = http.GET();
  
  if (httpCode == HTTP_CODE_OK) {
    String payload = http.getString();
    http.end();
    
    // Parse JSON response
    DynamicJsonDocument doc(1024);
    deserializeJson(doc, payload);
    
    // Extract hospital info
    JsonObject hospital = doc["hospital"];
    accidentInfo.hospitalName = hospital["name"].as<String>();
    accidentInfo.hospitalPhone = hospital["phone"].as<String>();
    
    // Extract police info
    JsonObject police = doc["police"];
    accidentInfo.policeName = police["name"].as<String>();
    accidentInfo.policePhone = police["phone"].as<String>();
    
    return true;
  } else {
    http.end();
    return false;
  }
}

void sendDataToArduino() {
  String message = "SOS_DATA:" + String(accidentInfo.latitude, 6) + "," +
                   String(accidentInfo.longitude, 6) + "," +
                   accidentInfo.hospitalName + "," +
                   accidentInfo.hospitalPhone + "," +
                   accidentInfo.policeName + "," +
                   accidentInfo.policePhone + "," +
                   accidentInfo.mapLink;
  
  arduinoSerial.println(message);
  Serial.println("Sent SOS data to Arduino");
}

bool waitForArduinoConfirmation() {
  unsigned long startTime = millis();
  
  while (millis() - startTime < 10000) { // 10 second timeout
    if (arduinoSerial.available()) {
      String response = arduinoSerial.readStringUntil('\n');
      response.trim();
      
      if (response.equals("SOS_SENT")) {
        Serial.println("Arduino confirmed SOS sent");
        return true;
      }
    }
    delay(100);
  }
  
  return false;
}

void sendSOSDirectly() {
  // In a full implementation, you would connect SIM900A to ESP32 as backup
  // For this example, we'll just print what would be sent
  
  String message = "EMERGENCY! Accident detected at: " + accidentInfo.mapLink + 
                   "\nNearest Hospital: " + accidentInfo.hospitalName + 
                   " (" + accidentInfo.hospitalPhone + ")" +
                   "\nPolice: " + accidentInfo.policeName + 
                   " (" + accidentInfo.policePhone + ")";
  
  Serial.println("Would send SMS to " + familyNumber + ":\n" + message);
  
  // In actual implementation:
  // 1. Connect SIM900A to ESP32's spare UART
  // 2. Use similar AT commands as in Arduino code
  // 3. Send the message
}