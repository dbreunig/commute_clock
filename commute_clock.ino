/*
 COMMUTE CLOCK by Drew Breunig
 A analog clock which moves to reflect the current commute time.
 * 
 The following code is tested and running on an Adafruit M0 WiFi Feather,
 with a OLED Feather Wing attached. A microservo is connected to pin 10.
 * 
 To use, you will need to update the following items
 - Your wifi network and password (lines 26 & 27)
 - Add your Google Distance Matrix API key (line 87)
 - Add the lat,lon for your origin (line 87)
 - Add the lat,lon for your destination (line 87)
 * 
 */

#include <SPI.h>
#include <WiFi101.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <Servo.h>

////////////////////////////////////////////////////////////////////////
// Variables

// Wifi State
char ssid[] = "YOUR_WIFI_NETWORK";
char pass[] = "YOUR_WIFI_PASS";
int status = WL_IDLE_STATUS;

// Request & Response Management
String currentLine = "";
String currentTime = "";
boolean inDurationBlock = false;
int durationValue = -1;
int lastDurationValue = -1;

// Timeout Delay
const int updateInterval = (60 * 1000) * 5; // 5 minutes. 15 minutes would be better.
long lastConnectionTime = 0;
boolean lastConnected = false;

// Initialize the wifi client library
WiFiSSLClient client;

// Servo State // Adjust max and min time by hand below to make scale appropriate
int minCommute = 60 * 44;  // min time of 44 minutes
int maxCommute = 60 * 90; // max time of 1.5 hours

// Set up servo
Servo durationHand;

// Set up OLED // Comment out if just using clock hand
Adafruit_SSD1306 display = Adafruit_SSD1306();
// CHANGE THESE VALUES IF USING NON-M0 FEATHER
#define BUTTON_A 9
#define BUTTON_B 6
#define BUTTON_C 5
#define LED      13


////////////////////////////////////////////////////////////////////////
// Helpers

void printWifiStatus() {
  // print the SSID of the network you're attached to:
  Serial.print("SSID: ");
  Serial.println(WiFi.SSID());

  // print your WiFi shield's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm");
}

void getCommuteTime() {
  display.clearDisplay();
  if (client.connect(server, 443)) {

    // Make request
    Serial.println("Connecting to Google API");
    client.println("GET /maps/api/distancematrix/json?units=imperial&departure_time=now&traffic_model=best_guess&origins=0.0,0.0&destinations=0.0,0.0&key=YOUR_KEY_HERE");
    client.println("Host: https://maps.googleapis.com/maps/api/distancematrix");
    client.println("Connection: close");
    client.println();

    // Update state
    lastConnectionTime = millis();

    // Print state
    if (client.connected()) {
      Serial.println("Pinging Google's Distance Matrix...");
      Serial.println();
    }
  }
}

void parseLine() {
  currentLine.trim();
  if (currentLine.startsWith("\"duration_in_traffic\"")) { // Check to see if in duration dictionary
    inDurationBlock = true;
  } else if (inDurationBlock && currentLine.startsWith("\"value\"")) { // Check if at value object
    // Store old value
    lastDurationValue = durationValue;
    // Grab value object and save (there are 10 chars prior to the value)
    String valueString = currentLine.substring(9);
    durationValue = valueString.toInt();
    // Reset inDurationBlock flag
    inDurationBlock = false;
  }
  currentLine = "";
}

void updateDisplay(int minutesLeft) {
  // Format commute time
  int hours     = durationValue / (60 * 60);
  int mins      = (durationValue / 60) % 60;
  int secs      = durationValue % 60;
  
  // Update the display
  display.clearDisplay();
  display.setTextSize(2);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  char *velocity = " ";
  if ( durationValue > lastDurationValue && lastDurationValue != -1 ) {
    velocity = "+";
  }
  else if ( lastDurationValue != -1 ) {
    velocity = "-";
  }
  display.print(velocity);
  display.print(hours);
  display.print(":");
  display.print(mins);
  display.print(":");
  display.print(secs);
  display.print("\n");

  display.display();

  display.setTextSize(1);
  display.setTextColor(WHITE);
  for (int i = 0; i < minutesLeft; i++) {
    display.print(".");
  }
  
  display.display();
}

void moveDurationHand() {
  // Map the durationValue into the servo range
  // I've mounted my servo where 180 is the min and it moves up to 0. Reverse the 180, 0 below if you need to.
  int durationHandValue = map(durationValue, minCommute, maxCommute, 180, 0);
  Serial.print("Duration hand value: ");
  Serial.println(durationHandValue);
  durationHand.write(durationHandValue);
}

////////////////////////////////////////////////////////////////////////
// Setup

void setup() {
  
  // Configure pins for Adafruit ATWINC1500 Feather
  WiFi.setPins(8,7,4,2);

  // Initialize serial and wait for port to open:
  Serial.begin(9600);

  // Set-up OLED
  Serial.println("OLED FeatherWing test");
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);  // initialize with the I2C addr 0x3C (for the 128x32)
  Serial.println("OLED begun");
  // Show splash screen
  display.display();
  delay(1000);
  // Clear the buffer.
  display.clearDisplay();
  display.display();

  // Set-up buttons on the OLED wing (I am not using these...)
  pinMode(BUTTON_A, INPUT_PULLUP);
  pinMode(BUTTON_B, INPUT_PULLUP);
  pinMode(BUTTON_C, INPUT_PULLUP);

  // Set-up servo
  durationHand.attach(10);
  durationHand.write(90);

  // Set-up text display
  display.setTextSize(1);
  display.setTextColor(WHITE);
  display.setCursor(0,0);

  // Try to connect to WiFi network
  display.print("Connecting to SSID:\n");
  display.println(ssid);
  display.display();
  while (status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);
    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);
    // wait 10 seconds for connection:
    delay(10000);
  }
  Serial.println("Connected to wifi");
  display.println("Connected!");
  display.display();
  printWifiStatus();
  
  getCommuteTime();
}

////////////////////////////////////////////////////////////////////////
// Loop

void loop() {

  // Get response from Google and parse
  if (client.available()) {
    char c = client.read();
    Serial.print(c); // DEBUG
    // Store in current line
    currentLine += c;
    // Process when new line
    if (c == '\n') {
      parseLine();
    }
  }

  // Disconnect from Google if response is done
  if (!client.connected() && lastConnected) {
    updateDisplay(16);

    // Print status
    Serial.println("...disconnected");
    Serial.println();
    Serial.print("COMMUTE TIME: ");
    Serial.print(durationValue);
    Serial.println("\n");
    client.stop();

    // Delay next get
    for (int i = 15; i > 0; i--) {
      updateDisplay(i);
      moveDurationHand();
      delay(60000);
    }
  }

  // Get new data from Google & handle the response
  if (!client.connected() && (millis() - lastConnectionTime > updateInterval)) {
    getCommuteTime();
  }

  // Record state
  lastConnected = client.connected();
}
