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

#define HTTP_RESPONSE_TIMEOUT 10000
WiFiSSLClient client;
HttpClient http(client, serverAddress, serverPort);
IPAddress NO_IP(0,0,0,0);

// Response buffers

#define RESPONSE_LEN      12
#define RESPONSE_TIME_LEN 48
#define HTTP_BODY_LEN     256
char response[RESPONSE_LEN];
char responseTime[RESPONSE_TIME_LEN];
char previousResponseTime[RESPONSE_TIME_LEN];
char body[HTTP_BODY_LEN];

// Prepare outputs

ArduinoLEDMatrix matrix;
bool useMatrix = true;
int output = -1;

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

const unsigned long wifiRetryInterval = 15000;
unsigned long previousWifiRetry = 0;


/* -------------------------------------------------------------------------- */
void setup() {
/* -------------------------------------------------------------------------- */
  // Prepare standard outputs
  Serial.begin(115200);
  if (useMatrix) matrix.begin(); // Shows current reading/error
  pinMode(LED_BUILTIN, OUTPUT); // Indicates when running getResponse

  Serial.println("\n\nHello at 115200 baud!");

  if (WiFi.status() == WL_NO_MODULE) {
    Serial.println("Communication with WiFi module failed!");
    if (useMatrix) matrix.loadFrame(Icon::noWifi);
    while (true);
  }

  if (WiFi.firmwareVersion() < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the firmware");
    for (int i = 0; i < 5; i++) {
      if (useMatrix) matrix.loadFrame(Icon::wifiUpgrade);
      delay(500);
      if (useMatrix) matrix.loadFrame(Icon::wifi);
      delay(500);
    }
  }

  waitConnectWifi();

  http.setHttpResponseTimeout(HTTP_RESPONSE_TIMEOUT);

  nixieClock.setHVPower(true);
  nixieClock.setBrightness(0xff);
  nixieClock.setNumber(0, 0b0000);
  nixieClock.display();
}


/* -------------------------------------------------------------------------- */
void loop() {
/* -------------------------------------------------------------------------- */
  currentMillis = millis();

  // Recheck glucose if long enough since previous
  if (currentMillis - previousRequestTime >= requestInterval) {
    int previousOutput = output;
    strncpy(previousResponseTime, responseTime, RESPONSE_TIME_LEN - 1);
    previousResponseTime[RESPONSE_TIME_LEN - 1] = '\0';

    // Try to get glucose reading and time of reading
    // Glucose/error information stored to response
    // Reading time, if any, stored to responseTime
    getResponse();

    // Repsonse reading might take a while, update again
    currentMillis = millis();
    // Mark that we've gotten the reading and we won't need one for a bit
    previousRequestTime = currentMillis;

    // Display output from response
    if (useMatrix) {
      matrix.beginDraw();
      matrix.stroke(0xFFFFFFFF);
      matrix.textFont(Font_4x6);
      matrix.beginText(0, 1, 0xFFFFFF);
      matrix.print(response);
      matrix.println("    ");
      matrix.endText();
      matrix.endDraw();
    }

    // Get output as a number for display on nixies. Errors with text show as 0's
    output = atoi(response);

    // Animate transition on nixies
    if (previousOutput != output || strcmp(previousResponseTime, responseTime) != 0) {
      animateSlotMachine(previousOutput, output);
      previousStrobeTime = currentMillis;
    }
  }

  // Fallback poisoning prevention
  if (currentMillis - previousStrobeTime >= strobeInterval) {
    previousStrobeTime = currentMillis;
    animateSlotMachine(output, output);
  }

  // Wait a second
  delay(1000);
}


// Read the http body
/* -------------------------------------------------------------------------- */
static void parsePayload(const char *body, char *glucose, char *timestamp) {
/* -------------------------------------------------------------------------- */
  glucose[0] = '\0';
  timestamp[0] = '\0';

  int lineStart = 0;
  int lineNo = 0;
  int bodyLen = strlen(body);

  // Find next newline character
  while (lineStart < bodyLen) {
    int newlineIndex = -1;
    for (int i = lineStart; i < bodyLen; i++) {
      if (body[i] == '\n') {
        newlineIndex = i;
        break;
      }
    }

    // If no newline character found, end of body is end of line
    int lineEnd = (newlineIndex == -1) ? bodyLen : newlineIndex;
    int lineLen = lineEnd - lineStart;

    // Extract glucose value
    if (lineNo == 0 && lineLen < RESPONSE_LEN) {
      strncpy(glucose, body + lineStart, lineLen);
      glucose[lineLen] = '\0';
    }

    // Extract timestamp
    if (lineNo == 2 && lineLen < RESPONSE_TIME_LEN) {
      strncpy(timestamp, body + lineStart, lineLen);
      timestamp[lineLen] = '\0';
    }

    // Exit on last line
    if (newlineIndex == -1) break;

    // Otherwise, move start of next line past newline character
    lineStart = newlineIndex + 1;
    lineNo++;
  }
}


// Populates response and responseTime
/* -------------------------------------------------------------------------- */
void getResponse() {
/* -------------------------------------------------------------------------- */
  // Ensure WiFi is connected
  waitConnectWifi();

  // Indicate fetch start on LED
  digitalWrite(LED_BUILTIN, HIGH);
  Serial.print("Getting... ");

  // Mark input buffers empty
  response[0] = '\0';
  responseTime[0] = '\0';

  // Do-while-false allows for breaking to clean-exit code
  do {
    // Make request
    int reqErr = http.get("/");
    if (reqErr != 0) {
      // Network/connection error
      snprintf(response, sizeof(response), "A%d", reqErr);
      Serial.print("Request Error: ");
      Serial.println(reqErr);
      break;
    }

    int statusCode = http.responseStatusCode();
    if (statusCode < 0) {
      // Response parsing error
      snprintf(response, sizeof(response), "B%d", statusCode);
      Serial.print("Response Error: ");
      Serial.println(statusCode);
      break;
    }

    if (statusCode != 200) {
      // Server error
      snprintf(response, sizeof(response), "C%d", statusCode);
      Serial.print("HTTP Error: ");
      Serial.println(statusCode);
      break;
    }

    // Only want to handle the body,
    // so move read head after headers
    http.skipResponseHeaders();

    // Read entire response into fixed buffer while data can be read
    int bodyIdx = 0;
    while (http.available() && bodyIdx < (sizeof(body) - 1))
      body[bodyIdx++] = http.read();
    body[bodyIdx] = '\0';

    // If response is empty, set an error
    if (bodyIdx == 0) {
      strncpy(response, "MTY", sizeof(response) - 1);
      response[sizeof(response) - 1] = '\0';
      break;
    }

    // Parse body for glucose and timestamp
    // Updates response and responseTime
    parsePayload(body, response, responseTime);

    // If parsing failed, raise error
    if (response[0] == '\0') {
      strncpy(response, "BAD", sizeof(response) - 1);
      response[sizeof(response) - 1] = '\0';
    }

  } while (false);

  // Close HTTP connection
  http.stop();

  // Indicate fetch done
  digitalWrite(LED_BUILTIN, LOW);

  // Print result
  if (responseTime[0] != '\0') {
    Serial.print(response);
    Serial.print(" @");
    Serial.println(responseTime);
  } else {
    Serial.println(response);
  }
}


/* -------------------------------------------------------------------------- */
void waitConnectWifi() {
/* -------------------------------------------------------------------------- */
  // WiFi already connected, return
  if (WiFi.status() == WL_CONNECTED && WiFi.localIP() != NO_IP) return;

  // Indicate connection lost
  if (useMatrix) matrix.loadFrame(Icon::noWifi);
  Serial.print("Attempting to connect to SSID: ");
  Serial.println(_SSID);

  previousWifiRetry = 0; // When did we last try to connect to the wifi?
  int wifiRetryAttempts = 0; // How many times have we retried?

  // Loop until connected successfully
  while (WiFi.status() != WL_CONNECTED || WiFi.localIP() == NO_IP) {
    currentMillis = millis();

    // Try to connect if we've waited long enough since the last attempt
    if (previousWifiRetry == 0 || (currentMillis - previousWifiRetry >= wifiRetryInterval)) {
      wifiRetryAttempts++;

      // Fully reset wifi stack after a string of failures
      if (wifiRetryAttempts % 10 == 0) {
        Serial.println("Resetting WiFi stack...");
        WiFi.disconnect();
        delay(1000);
      }

      // Attempt to connect to WiFi network:
      WiFi.begin(_SSID, _PASS);
      previousWifiRetry = currentMillis;
    }

    if (useMatrix) {
      // Display bouncing wifi animation until connected
      matrix.loadFrame(Icon::wifi);
      delay(500);
      matrix.loadFrame(Icon::wifi1);
      delay(500);
      matrix.loadFrame(Icon::wifi2);
      delay(500);
      matrix.loadFrame(Icon::wifi3);
      delay(500);
    } else {
      // Just wait
      delay(2000);
    }

    // Cathode poisoning prevention, if it takes a long time to reconnect to wifi
    if (currentMillis - previousStrobeTime >= strobeInterval) {
      animateSlotMachine(output, 0);
      previousStrobeTime = currentMillis;
    }
  }

  // Connected, good to go!
  if (useMatrix) matrix.loadFrame(Icon::wifiGood);

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

  Serial.print("Animating from ");
  Serial.print(from);
  Serial.print(" to ");
  Serial.print(to);
  Serial.println("...");

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
