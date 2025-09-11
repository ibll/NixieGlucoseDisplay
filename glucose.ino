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
#include "Font_5x7_Custom.h"
#include "icons.h"
#include "Omnixie_NTDB.h"

/* -------------------------------------------------------------------------- */

// Networking stuff

char _SSID[] = SECRET_SSID;
char _PASS[] = SECRET_PASS;

char serverAddress[] = "https://example.com/";
int  serverPort      = 443;

WiFiSSLClient client;
R4HttpClient  http;
IPAddress NO_IP(0,0,0,0);

// Built-in LED output

ArduinoLEDMatrix matrix;
String matrixText = "";

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
unsigned long previousRequestTime = -1;
const unsigned long strobeInterval = 5 * 60 * 1000;
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
      delay(500);
      matrix.loadFrame(Icon::wifi);
      delay(500);
    }
  }

  waitConnectWifi();

  nixieClock.setHVPower(true);
  nixieClock.setBrightness(0xff);
  nixieClock.display();
}

/* -------------------------------------------------------------------------- */
void loop() {
/* -------------------------------------------------------------------------- */
  currentMillis = millis();

  // Cathode poisoning prevention
  if (previousStrobeTime == -1 || currentMillis - previousStrobeTime >= strobeInterval) {
    CathodePoisoningPrevention(3, 100);
    previousStrobeTime = currentMillis;
  }

  // Recheck glucose
  if (previousRequestTime == -1 || currentMillis - previousRequestTime >= requestInterval) {
    getResponse();
    previousRequestTime = currentMillis;
  }

  delay(1000); // Wait a second
}

/* -------------------------------------------------------------------------- */
void getResponse() {
/* -------------------------------------------------------------------------- */
  waitConnectWifi();

  Serial.print("Getting... ");
  digitalWrite(LED_BUILTIN, HIGH);

  // Make request
  http.begin(client, serverAddress, serverPort);
  http.setTimeout(3000);
  http.addHeader("User-Agent: Arduino UNO R4 Wifi");
  // http.addHeader("Connection: close");
  int statusCode = http.GET();

  if (statusCode <= 0) {
    // Library error
    // matrixText = "Fetch Err: " + String(statusCode);
    matrixText = "F" + String(statusCode);
  } else if (statusCode != HTTP_CODE_OK) {
    // Server error
    // matrixText = "Server Err: " + String(statusCode);
    matrixText = "E" + String(statusCode);
  } else {
    // All good
    String body = http.getBody();
    body.trim();

    // Only get first line, the current value in mg/dL or No Data
    const int newlineIndex = body.indexOf('\n');
    if (newlineIndex != -1)
      body = body.substring(0, newlineIndex);
  
    // Display

    matrixText = body;
    nixieClock.setNumber(body.toInt(), tubeBitmask);
  }

  Serial.println(matrixText);

  // Built-in display
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textFont(Font_4x6);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println(matrixText + "    ");
  matrix.endText();
  matrix.endDraw();

  nixieClock.display();

  digitalWrite(LED_BUILTIN, LOW);
}

/* -------------------------------------------------------------------------- */
void waitConnectWifi() {
// Need to add cahtode slot machine to this 
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
    delay(500);
    matrix.loadFrame(Icon::wifi1);
    delay(500);
    matrix.loadFrame(Icon::wifi2);
    delay(500);
    matrix.loadFrame(Icon::wifi3);
    delay(500);

    // Cathode poisoning prevention, if it takes a long time to reconnect to wifi
    currentMillis = millis();

    if (previousStrobeTime == -1 || currentMillis - previousStrobeTime >= strobeInterval) {
      CathodePoisoningPrevention(3, 100);
      previousStrobeTime = currentMillis;

      nixieClock.setNumber(0, tubeBitmask);
      nixieClock.display();
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
void CathodePoisoningPrevention(unsigned int num, int msDelay) {
/* -------------------------------------------------------------------------- */
  if (num < 1) exit;

  Serial.println("Running Cathode Poisoning Prevention... ");
  nixieClock.setBrightness(0xff);

  for (byte n = 0; n < num; n++) {
    for (byte i = 0; i < 10; i++) {
      nixieClock.setNumber(i * 1111, tubeBitmask);
      nixieClock.display();

      matrix.beginDraw();
      matrix.stroke(0xFFFFFFFF);
      matrix.textFont(Font_4x6);
      matrix.beginText(0, 1, 0xFFFFFF);
      matrix.println(String(i * 111) + "  ");
      matrix.endText();
      matrix.endDraw();

      delay(msDelay);
    }
  }
}