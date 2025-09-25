/*
  Circuit:
  Connect the NTDB to Arduino Uno:
  --------------------------------
    NTDB        Arduino Pins
  --------------------------------
    GND         GND
    DC5V
    DATA        11
    OE          10
    STCP        8
    SHCP        12
    COLON       5 (Not In Use)
    ON/OFF      6 (HVEnable)
  --------------------------------
  Connect the 12V DC power to the NTDB board 
*/

#include <R4HttpClient.h>

#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"

#include "arduino_secrets.h"
#include "icons.h"
#include "Omnixie_NTDB.h"

/* -------------------------------------------------------------------------- */

// Networking stuff

char _SSID[] = SECRET_SSID;
char _PASS[] = SECRET_PASS;

char serverAddress[] = SECRET_ADDRESS;
int  serverPort      = SECRET_PORT;

WiFiSSLClient client;
R4HttpClient  http;
IPAddress NO_IP(0,0,0,0);

// Network response
String response = "";
String responseTime = "";

// Prepare outputs
ArduinoLEDMatrix matrix;
int output = -1;

// Constants for timing and configuration
#define NTDBcount  1
#define tubeBitmask 0b0111

// Timing constants
const unsigned long REQUEST_INTERVAL_MS = 60 * 1000;            // 60 seconds
const unsigned long STROBE_INTERVAL_MS = 6 * 60 * 1000;        // 6 minutes
const unsigned long FRAME_DISPLAY_MS = 100;                     // 100ms per frame
const unsigned long WIFI_RETRY_DELAY_MS = 500;                  // 500ms between wifi attempts
const unsigned long SETUP_DELAY_MS = 1000;                      // 1 second delays in setup
const unsigned long MAIN_LOOP_DELAY_MS = 1000;                  // 1 second main loop delay
const int HTTP_TIMEOUT_MS = 3000;                               // HTTP request timeout

// pin_DataIN, pin_STCP(latch), pin_SHCP(clock), pin_Blank(Output Enable; PWM pin preferred),
// HVEnable pin, Colon pin, number of Nixie Tube Driver Boards
// PWM Pins on Arduino Uno: 3, 5, 6, 9, 10, 11; PWM FREQUENCY 490 Hz (pins 5 and 6: 980 Hz)
Omnixie_NTDB nixieClock(11, 8, 12, 10, 6, 5, NTDBcount);

// Periodic events

unsigned long currentMillis;
const unsigned long requestInterval = REQUEST_INTERVAL_MS;
unsigned long previousRequestTime = -1;
const unsigned long strobeInterval = STROBE_INTERVAL_MS; // New blood sugar every 5 minutes, we should wait at least 6 for a new reading
unsigned long previousStrobeTime = -1;


/* -------------------------------------------------------------------------- */
void setup() {
/* -------------------------------------------------------------------------- */
  Serial.begin(115200);
  matrix.begin();
  pinMode(LED_BUILTIN, OUTPUT);

  Serial.println("\n\nHello at 115200 baud!");

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    matrix.loadFrame(Icon::noWifi);
    while (true);
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
    for (int i = 0; i < 5; i++) {
      matrix.loadFrame(Icon::wifiUpgrade);
      delay(WIFI_RETRY_DELAY_MS);
      matrix.loadFrame(Icon::wifi);
      delay(WIFI_RETRY_DELAY_MS);
    }
  }

  waitConnectWifi();

  nixieClock.setHVPower(true);
  nixieClock.setBrightness(0xff);
  nixieClock.setNumber(0, 0b0000);
  nixieClock.display();
}


/* -------------------------------------------------------------------------- */
void loop() {
/* -------------------------------------------------------------------------- */
  currentMillis = millis();

  // Recheck glucose
  if (previousRequestTime == -1 || currentMillis - previousRequestTime >= requestInterval) {
    int previousOutput = output;
    String previousResponseTime = responseTime;
    
    getResponse();
    previousRequestTime = currentMillis;

    matrix.beginDraw();
    matrix.stroke(0xFFFFFFFF);
    matrix.textFont(Font_4x6);
    matrix.beginText(0, 1, 0xFFFFFF);
    matrix.println(response + "    ");
    matrix.endText();
    matrix.endDraw();

    // Only update output if we have a valid numeric response
    int newOutput = response.toInt();
    if (response.length() > 0 && (newOutput > 0 || response == "0")) {
      output = newOutput;
    } else {
      Serial.println("Invalid response received: " + response);
      // Keep previous output value
    }

    // Animate transition
    if (previousOutput != output || previousResponseTime != responseTime) {
      animateSlotMachine(previousOutput, output);
      previousStrobeTime = currentMillis;
    }
  }

  // Fallback poisoning prevention
  if (previousStrobeTime == -1 || currentMillis - previousStrobeTime >= strobeInterval) {
    animateSlotMachine(output, output);
    previousStrobeTime = currentMillis;
  }

  delay(MAIN_LOOP_DELAY_MS); // Wait a second
}


// Populates response and responseTime
/* -------------------------------------------------------------------------- */
void getResponse() {
/* -------------------------------------------------------------------------- */
  waitConnectWifi();

  Serial.print("Getting... ");
  digitalWrite(LED_BUILTIN, HIGH);

  // Make request
  http.begin(client, serverAddress, serverPort);
  http.setTimeout(HTTP_TIMEOUT_MS);
  http.addHeader("User-Agent: Arduino UNO R4 Wifi");
  // http.addHeader("Connection: close");
  int statusCode = http.GET();

  if (statusCode <= 0) {
    // Library error
    response = "F" + String(statusCode);
  } else if (statusCode != HTTP_CODE_OK) {
    // Server error
    response = "E" + String(statusCode);
  } else {
    // All good
    String body = http.getBody();
    body.trim();

    if (body.length() == 0) {
      response = "Empty";
      Serial.println(response);
      digitalWrite(LED_BUILTIN, LOW);
      return;
    }

    // Parse lines
    response = "";
    responseTime = "";

    int start = 0;
    int lineNo = 0;
    while (start <= body.length()) {
      String line;
      int idx = body.indexOf('\n', start);
      if (idx == -1) { // last segment (no trailing newline)
        if (start < body.length()) {
          line = body.substring(start);
          line.trim();
        }
      } else {
        line = body.substring(start, idx);
        line.trim();
      }
      
      switch (lineNo) {
        case 0: // Blood glucose
          response = line;
          break;
        case 1: // Trend description (currently unused)
          break;
        case 2: // Time
          responseTime = line;
          break;
        default:
          // Ignore additional lines
          break;
      }

      if (idx == -1) break;

      start = idx + 1;
      lineNo++;
    }
    
    // Validate glucose reading
    int glucoseValue = response.toInt();
    if (glucoseValue < 0 || glucoseValue > 600) {  // Reasonable glucose range
      Serial.println("Warning: Glucose value out of expected range: " + String(glucoseValue));
    }
  }

  Serial.println(response);
  digitalWrite(LED_BUILTIN, LOW);
}


/* -------------------------------------------------------------------------- */
void waitConnectWifi() {
/* -------------------------------------------------------------------------- */
  if (WiFi.status() == WL_CONNECTED) return;
  
  matrix.loadFrame(Icon::noWifi);
  
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(_SSID);

  // Attempt to connect to WiFi network:
  WiFi.begin(_SSID, _PASS);
  
  // Display bouncing wifi animation until connected
  while (WiFi.status() != WL_CONNECTED || WiFi.localIP() == NO_IP) {
    matrix.loadFrame(Icon::wifi);
    delay(WIFI_RETRY_DELAY_MS);
    matrix.loadFrame(Icon::wifi1);
    delay(WIFI_RETRY_DELAY_MS);
    matrix.loadFrame(Icon::wifi2);
    delay(WIFI_RETRY_DELAY_MS);
    matrix.loadFrame(Icon::wifi3);
    delay(WIFI_RETRY_DELAY_MS);

    // Cathode poisoning prevention, if it takes a long time to reconnect to wifi
    currentMillis = millis();

    if (currentMillis - previousStrobeTime >= strobeInterval) {
      animateSlotMachine(output, 000);
      previousStrobeTime = currentMillis;
    }
  }

  // Connected, good to go!
  matrix.loadFrame(Icon::wifiGood);

  delay(SETUP_DELAY_MS);

  // Print connection info
  Serial.print("\tSSID: ");
  Serial.println(WiFi.SSID());
  Serial.print("\tIP Address: ");
  Serial.println(WiFi.localIP());
}


/* -------------------------------------------------------------------------- */
static inline void showFrame(uint8_t left, uint8_t mid, uint8_t right, uint8_t mask = 0b0111) {
/* -------------------------------------------------------------------------- */
  mask &= tubeBitmask; // crop to # of digits
  nixieClock.setNumber(join3(left, mid, right), mask);
  nixieClock.display();
  delay(FRAME_DISPLAY_MS);
}


/* -------------------------------------------------------------------------- */
static void animateSlotMachine(int from, int to) {
/* -------------------------------------------------------------------------- */
  // Input validation - clamp values to valid range
  if (to < 0) to = 0;
  if (to > 999) to = 999;
  if (from < -1) from = -1;  // Allow -1 for initial state
  if (from > 999) from = 999;

  uint8_t fd[3] = {0, 0, 0};
  uint8_t td[3] = {0, 0, 0};
  if (from >= 0) split3(from, fd);
  split3(to, td);

  Serial.println("Animating from " + String(from) + " to " + String(to) + "...");

  // Animate in
  for (uint8_t i = 0; i < 10; ++i) {
    if (from >= 0) showFrame(fd[0], fd[1], i);
    else showFrame(0, 0, i, 0b0001);
  }
  for (uint8_t i = 0; i < 10; ++i) {
    if (from >= 0) showFrame(fd[0], i, i);
    else showFrame(0, i, i, 0b0011);
  }

  // All cycling
  for (uint8_t i = 0; i < 10; ++i) showFrame(i, i, i);

  // Animate out
  for (uint8_t i = 0; i < 10; ++i) showFrame(i, i, td[2]);
  for (uint8_t i = 0; i < 10; ++i) showFrame(i, td[1], td[2]);
  
  // Finish
  showFrame(td[0], td[1], td[2]);
}


/* -------------------------------------------------------------------------- */
static inline void split3(int v, uint8_t d[3]) {
/* -------------------------------------------------------------------------- */
    d[0] = (v / 100) % 10; // hundreds
    d[1] = (v / 10) % 10;  // tens
    d[2] = v % 10;         // ones
}


/* -------------------------------------------------------------------------- */
static inline int join3(int leftHundreds, int midTens, int rightOnes) {
/* -------------------------------------------------------------------------- */
    return leftHundreds * 100 + midTens * 10 + rightOnes;
}
