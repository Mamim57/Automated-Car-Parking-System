#include <Wire.h> 
#include <LiquidCrystal_I2C.h> 
#include <Servo.h> 
#include <SPI.h> 
#include <MFRC522.h> 
 
 
 
 
#define SS_PIN 10 
#define RST_PIN 9 
#define servoPin 2 
 
 
 
 
// WiFi / Middleware config  
String ssid = "YOUR_WIFI_NAME"; 
String password =  "YOUR_WIFI_PASSWORD"; 
String host = "mrjisan.pythonanywhere.com";  // Middleware host 
String firebasePath = "/postData";           // Middleware endpoint 
 
 
 
 
MFRC522 rfid(SS_PIN, RST_PIN); 
Servo myservo; 
LiquidCrystal_I2C lcd(0x27, 16, 2); 
 
 
 
 
int IR1 = 4; // Entry sensor (IR at entrance) 
int IR2 = 3; // Exit sensor (IR at exit) 
 
 
 
 
int Slot = 4; // Current available slots 
const int MaxSlots = 4; // Maximum slots 
 
 
 
 
bool carEntering = false; 
bool carExiting = false; 
 
 
 
 
String allowedUID = "F6 A9 23 03"; // Allowed card UID (keep format like "F6 A9 23 03") 
String lastUID = ""; // Store last scanned UID (simple tracking) 
 
 
 
 
void setup() { 
  // Use 115200 for ESP AT reliability; change if your ESP uses different baud 
  Serial.begin(115200); 
  delay(2000); 
 
 
 
 
  // Initialize LCD, servo, sensors, RFID 
  lcd.init(); 
  lcd.backlight(); 
  pinMode(IR1, INPUT); 
  pinMode(IR2, INPUT); 
  myservo.attach(servoPin); 
  myservo.write(100); // Close gate initially 
 
 
 
 
  lcd.setCursor(0, 0); 
  lcd.print("    ARDUINO    "); 
  lcd.setCursor(0, 1); 
  lcd.print(" PARKING SYSTEM "); 
  delay(2000); 
  lcd.clear(); 
 
 
 
 
  SPI.begin(); 
  rfid.PCD_Init(); 
 
 
 
 
  // Initialize ESP / WiFi via AT commands 
  sendCommand("AT+RST", 2000); 
  delay(1000); 
  sendCommand("AT+CWMODE=1", 1000); 
  connectWiFi(); 
  delay(1000); 
} 
 
 
 
 
void loop() { 
  // Check if a car is entering (IR1 low first -> then RFID scan) 
  if (digitalRead(IR1) == LOW && !carEntering && !carExiting) { 
    carEntering = true; 
    lcd.clear(); 
    lcd.setCursor(0, 0); 
    lcd.print("Car Detected!"); 
    delay(500); 
 
 
 
 
    if (Slot > 0) { 
      String scannedUID = rfunc(); // Scan RFID and get UID (or empty if wrong) 
      Serial.print("Scanned UID: "); 
      Serial.println(scannedUID); 
      if (scannedUID.length() > 0) { 
        // Allowed — open gate and register entry 
        smoothOpenGate(); 
        Slot--; // decrement 
        lastUID = scannedUID; // track this car for exit event 
 
 
 
 
        // Send entry event to middleware 
        sendToMiddleware(scannedUID, "entry"); 
 
 
 
 
        lcd.clear(); 
        lcd.setCursor(0, 0); 
        lcd.print("Gate Opened!"); 
        delay(2000); 
 
 
 
 
        // Wait for car to pass IR2 (assume IR2 is further inside) 
        while (digitalRead(IR2) == HIGH) { delay(100); } // wait until car hits IR2 
        while (digitalRead(IR2) == LOW) { delay(100); }  // wait until car leaves IR2 
 
 
 
 
        delay(1000); 
        smoothCloseGate(); 
        lcd.clear(); 
        lcd.setCursor(0, 0); 
        lcd.print("Gate Closed!"); 
        delay(1500); 
      } else { 
        // Wrong card 
        lcd.clear(); 
        lcd.setCursor(0, 0); 
        lcd.print("Wrong card!"); 
        delay(1500); 
        lcd.clear(); 
      } 
    } else { 
      lcd.clear(); 
      lcd.setCursor(0, 0); 
      lcd.print("    SORRY :(    "); 
      lcd.setCursor(0, 1); 
      lcd.print("  Parking Full  "); 
      delay(2000); 
      lcd.clear(); 
    } 
 
 
 
 
    carEntering = false; 
  } 
 
 
 
 
  // Check if a car is exiting (IR2 low first -> then passes IR1) 
  if (digitalRead(IR2) == LOW && !carExiting && !carEntering) { 
    if (Slot == MaxSlots) { 
      // Nothing to exit 
      lcd.clear(); 
      lcd.setCursor(0, 0); 
      lcd.print(" No Cars Left! "); 
      delay(1500); 
      lcd.clear(); 
    } else { 
      carExiting = true; 
      smoothOpenGate(); 
      if (Slot < MaxSlots) Slot++; // increment available slots 
 
 
 
 
      // Send exit event to middleware with lastUID if available 
      if (lastUID.length() > 0) { 
        sendToMiddleware(lastUID, "exit"); 
        // After exit, clear lastUID (simple approach). If multiple cars, this is simplistic. 
        lastUID = ""; 
      } else { 
        // No UID known — still send unknown exit (if you want) 
        sendToMiddleware("UNKNOWN", "exit"); 
      } 
 
 
 
 
      lcd.clear(); 
      lcd.setCursor(0, 0); 
      lcd.print("Car Exiting..."); 
      delay(1500); 
 
 
 
 
      // Wait for the car to pass IR1 
      while (digitalRead(IR1) == HIGH) { delay(100); } // wait until car reaches IR1 
      while (digitalRead(IR1) == LOW) { delay(100); }  // wait until car leaves IR1 
 
 
 
 
      delay(500); 
      smoothCloseGate(); 
      lcd.clear(); 
      lcd.setCursor(0, 0); 
      lcd.print("Gate Closed!"); 
      delay(1500); 
      carExiting = false; 
    } 
  } 
 
 
 
 
  // Regular display update 
  lcd.setCursor(0, 0); 
  lcd.print("    WELCOME!    "); 
  lcd.setCursor(0, 1); 
  lcd.print("Slot Left: "); 
  lcd.print(Slot); 
  delay(400); 
} 
 
 
 
 
// ---------- Gate functions ---------- 
void smoothOpenGate() { 
  for (int pos = 100; pos >= 0; pos -= 1) { 
    myservo.write(pos); 
    delay(8); 
  } 
} 
 
 
 
 
void smoothCloseGate() { 
  for (int pos = 0; pos <= 100; pos += 1) { 
    myservo.write(pos); 
    delay(8); 
  } 
} 
 
 
 
 
// ---------- RFID scan function ---------- 
// Returns UID string like "F6 A9 23 03" if allowed, otherwise returns empty string 
String rfunc() { 
  lcd.clear(); 
  lcd.setCursor(4, 0); 
  lcd.print("Welcome!"); 
  lcd.setCursor(1, 1); 
  lcd.print("Put your card"); 
  delay(100); 
 
 
 
 
  // wait for card 
  unsigned long start = millis(); 
  while (true) { 
    if (rfid.PICC_IsNewCardPresent() && rfid.PICC_ReadCardSerial()) { 
      break; 
    } 
    // Timeout after 8 seconds to avoid blocking forever 
    if (millis() - start > 8000) { 
      lcd.clear(); 
      lcd.setCursor(0, 0); 
      lcd.print("Scan Timeout"); 
      delay(800); 
      lcd.clear(); 
      return ""; // timeout 
    } 
    delay(200); 
  } 
 
 
 
 
  // Build UID string 
  String ID = ""; 
  for (byte i = 0; i < rfid.uid.size; i++) { 
    if (i > 0) ID += " "; 
    String part = String(rfid.uid.uidByte[i], HEX); 
    if (part.length() == 1) part = "0" + part; 
    ID += part; 
  } 
  ID.toUpperCase(); 
 
 
 
 
  lcd.clear(); 
  lcd.setCursor(0, 0); 
  lcd.print("Scanning.."); 
  delay(300); 
 
 
 
 
  // Compare with allowed UID (we ignore leading/trailing spaces) 
  // Make sure formats match: allowedUID defined as "F6 A9 23 03" 
  if (ID == allowedUID || ID == "24 06 C1 01" || ID == "F7 13 41 02" || 
ID == "A2 71 3F 02") { 
    lcd.clear(); 
    lcd.setCursor(0, 0); 
    lcd.print("Allowed."); 
    delay(800); 
    lcd.clear(); 
    return ID; // success 
  } else { 
    lcd.clear(); 
    lcd.setCursor(0, 0); 
    lcd.print("Wrong card!"); 
    delay(1000); 
    lcd.clear(); 
    return ""; // not allowed 
  } 
} 
 
 
 
 
 
void connectWiFi() { 
  String cmd = "AT+CWJAP=\"" + ssid + "\",\"" + password + "\""; 
  String resp = sendCommand(cmd, 15000);  
  
} 
 
 
 
 
String sendCommand(String command, int timeout) { 
  String response = ""; 
  Serial.println(command); 
  long int time = millis(); 
  while ((time + timeout) > millis()) { 
    while (Serial.available()) { 
      char c = Serial.read(); 
      response += c; 
    } 
  } 
  // small delay to ensure response consumption 
  delay(50); 
  return response; 
} 
 
 
 
 
// Send JSON with fields: uid, event (entry/exit), timestamp (millis) 
void sendToMiddleware(String uid, String evType) { 
  // String owner = "Shahid Al Mamim"; 
  uid.toUpperCase();    
  uid.trim();           
  String owner = "UNKNOWN"; 
 
  if (uid == "F6 A9 23 03") { 
    owner = "MRJ"; 
  } 
  else if (uid == "24 06 C1 01") { 
    owner = "SAM"; 
  } 
  else if (uid == "F7 13 41 02") { 
    owner = "LAJ"; 
  } 
  else if (uid == "A2 71 3F 02") { 
    owner = "NZN"; 
  } 
 
  // Prepare JSON 
  // unsigned long ts = millis(); 
  unsigned long ms = millis(); 
 
  unsigned long seconds = ms / 1000; 
  unsigned long minutes = (seconds / 60) % 60; 
  unsigned long hours   = (seconds / 3600) % 24; 
  seconds = seconds % 60; 
 
  char timeBuffer[9]; // "HH:MM:SS" 
  sprintf(timeBuffer, "%02lu:%02lu:%02lu", hours, minutes, seconds); 
 
  String ts = String(timeBuffer); 
 
 
  
  String jsonData = "{\"uid\":\"" + uid + "\",\"event\":\"" + evType + 
"\",\"owner\":\"" + owner + "\",\"timestamp\":\"" + ts + "\"}"; 
 
 
 
 
  // Start TCP connection to middleware (port 80) 
  String cmdStart = "AT+CIPSTART=\"TCP\",\"" + host + "\",80"; 
  sendCommand(cmdStart, 5000); 
  delay(200); 
 
 
 
 
  // Build HTTP POST request 
  String httpRequest = "POST " + firebasePath + " HTTP/1.1\r\n"; 
  httpRequest += "Host: " + host + "\r\n"; 
  httpRequest += "Content-Type: application/json\r\n"; 
  httpRequest += "Content-Length: " + String(jsonData.length()) + 
"\r\n\r\n"; 
  httpRequest += jsonData; 
 
 
 
 
  // Tell ESP8266 how many bytes will be sent 
  String sendLenCmd = "AT+CIPSEND=" + String(httpRequest.length()); 
  sendCommand(sendLenCmd, 3000); 
  delay(200); 
 
 
 
 
  // Send HTTP request bytes 
  Serial.print(httpRequest); 
  delay(800); // give ESP time to transmit 
  sendCommand("AT+CIPCLOSE", 1000); 
}