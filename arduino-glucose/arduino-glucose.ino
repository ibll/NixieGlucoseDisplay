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

#include <WiFiS3.h>
#include <ArduinoHttpClient.h>

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
HttpClient  http(client, serverAddress, serverPort);
IPAddress NO_IP(0,0,0,0);

// Network response
String response = "";
String responseTime = "";

// Prepare outputs
ArduinoLEDMatrix matrix;
int output = 0;

// Nixie driver

#define NTDBcount  1
#define tubeBitmask 0b0111
// pin_DataIN, pin_STCP(latch), pin_SHCP(clock), pin_Blank(Output Enable; PWM pin preferred),
// HVEnable pin, Colon pin, number of Nixie Tube Driver Boards
// PWM Pins on Arduino Uno: 3, 5, 6, 9, 10, 11; PWM FREQUENCY 490 Hz (pins 5 and 6: 980 Hz)
Omnixie_NTDB nixieClock(11, 8, 12, 10, 6, 5, NTDBcount);

// Periodic events

unsigned long currentMillis;
const unsigned long requestInterval = 60 * 1000;
unsigned long previousRequestTime = -requestInterval;

const unsigned long strobeInterval = 6 * 60 * 1000; // New blood sugar every 5 minutes, we should wait at least 6 for a new reading
unsigned long previousStrobeTime = 0;

const unsigned long wifiRetryInterval = 12000;
unsigned long previousWifiRetry = 0;


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
      delay(500);
      matrix.loadFrame(Icon::wifi);
      delay(500);
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
  if (currentMillis - previousRequestTime >= requestInterval) {
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

    output = response.toInt();

    // Animate transition
    if (previousOutput != output || previousResponseTime != responseTime) {
      animateSlotMachine(previousOutput, output);
      previousStrobeTime = currentMillis;
    }
  }

  // Fallback poisoning prevention
  if (currentMillis - previousStrobeTime >= strobeInterval) {
    previousStrobeTime = currentMillis;
    animateSlotMachine(output, output);
  }

  delay(1000); // Wait a second
}


// Read the http body
/* -------------------------------------------------------------------------- */
static void parsePayload(const String &body, String &glucose, String &timestamp) {
/* -------------------------------------------------------------------------- */
  glucose = "";
  timestamp = "";

  int start = 0;
  int lineNo = 0;

  if (body.length() == 0) return;

  while (start <= body.length()) {
    int idx = body.indexOf('\n', start);
    String line;

    if (idx == -1) { // last segment (no trailing newline)
      if (start < body.length()) {
        line = body.substring(start);
        line.trim();
      } else {
        break;
      }
    } else {
      line = body.substring(start, idx);
      line.trim();
    }

    if (lineNo == 0) glucose = line;
    if (lineNo == 2) timestamp = line;

    if (idx == -1) break;
    start = idx + 1;
    lineNo++;
  }
}


// Populates response and responseTime
/* -------------------------------------------------------------------------- */
void getResponse() {
/* -------------------------------------------------------------------------- */
  waitConnectWifi();

  digitalWrite(LED_BUILTIN, HIGH);
  Serial.print("Getting... ");

  response = "";
  responseTime = "";

  // Make request
  http.setHttpResponseTimeout(10000);

  do {
    int reqErr = http.get("/");
    if (reqErr != 0) {
      response = "A" + String(reqErr);
      Serial.print("Request Error: "); Serial.println(reqErr);
      break;
    }

    int statusCode = http.responseStatusCode();
    if (statusCode < 0) {
      response = "B" + String(statusCode);
      Serial.print("Response Error: "); Serial.println(statusCode);
      break;
    }

    if (statusCode != 200) {
      response = "C" + String(statusCode);
      Serial.print("HTTP Error: "); Serial.println(statusCode);
      break;
    }

    String body = http.responseBody();
    body.trim();

    if (body.length() == 0) {
      response = "MTY";
      break;
    }

    parsePayload(body, response, responseTime);

    if (response.length() == 0) {
      response = "BAD";
    }

  } while (false);

  http.stop();
  digitalWrite(LED_BUILTIN, LOW);

  if (responseTime.length() > 0) {
    Serial.println(response + " @" + responseTime);
  } else {
    Serial.println(response);
  }
}


/* -------------------------------------------------------------------------- */
void waitConnectWifi() {
/* -------------------------------------------------------------------------- */
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != NO_IP) return;

  matrix.loadFrame(Icon::noWifi);

  Serial.print("Attempting to connect to SSID: ");
  Serial.println(_SSID);

  previousWifiRetry = 0;

  // Display bouncing wifi animation until connected
  while (WiFi.status() != WL_CONNECTED || WiFi.localIP() == NO_IP) {
    currentMillis = millis();

    if (previousWifiRetry == 0 || (currentMillis - previousWifiRetry >= wifiRetryInterval)) {
      // Attempt to connect to WiFi network:
      WiFi.begin(_SSID, _PASS);
      previousWifiRetry = currentMillis;
    }

    matrix.loadFrame(Icon::wifi);
    delay(500);
    matrix.loadFrame(Icon::wifi1);
    delay(500);
    matrix.loadFrame(Icon::wifi2);
    delay(500);
    matrix.loadFrame(Icon::wifi3);
    delay(500);

    // Cathode poisoning prevention, if it takes a long time to reconnect to wifi
    if (currentMillis - previousStrobeTime >= strobeInterval) {
      animateSlotMachine(output, 0);
      previousStrobeTime = currentMillis;
    }
  }

  // Connected, good to go!
  matrix.loadFrame(Icon::wifiGood);

  delay(1000);

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
  delay(100);
}


/* -------------------------------------------------------------------------- */
static void animateSlotMachine(int from, int to) {
/* -------------------------------------------------------------------------- */

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
