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

#include <ArduinoHttpClient.h>
#include <WiFiS3.h>

#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"

#include "arduino_secrets.h"
#include "Font_5x7_Custom.h"
#include "icons.h"
// #include "Omnixie_NTDB.h"

/* -------------------------------------------------------------------------- */

// Networking stuff

char ssid[] = SECRET_SSID;
char pass[] = SECRET_PASS;

char serverAddress[] = "example.com";
int port = 443;

WiFiSSLClient wifi;
HttpClient client = HttpClient(wifi, serverAddress, port);
int status = WL_IDLE_STATUS;

// Built-in LED output

ArduinoLEDMatrix matrix;
String matrixText = "";

// Nixie driver

// #define NTDBcount  1
// #define tubeBitmask = 0b1000
// pin_DataIN, pin_STCP(latch), pin_SHCP(clock), pin_Blank(Output Enable; PWM pin preferred),
// HVEnable pin, Colon pin, number of Nixie Tube Driver Boards
// PWM Pins on Arduino Uno: 3, 5, 6, 9, 10, 11; PWM FREQUENCY 490 Hz (pins 5 and 6: 980 Hz)
// Omnixie_NTDB nixieClock(11, 8, 12, 10, 6, 5, NTDBcount);

// Periodic events

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

  waitConnectWifi();

  // nixieClock.setHVPower(true);
  // nixieClock.setBrightness(0xff);
  // nixieClock.display();
}

/* -------------------------------------------------------------------------- */
void loop() {
/* -------------------------------------------------------------------------- */
  unsigned long currentMillis = millis();

  // Cathode poisoning prevention
  if (previousStrobeTime == -1 || currentMillis - previousStrobeTime >= strobeInterval) {
    previousStrobeTime = currentMillis;
    CathodePoisoningPrevention(3, 100);
  }

  // Recheck glucose
  if (previousRequestTime == -1 || currentMillis - previousRequestTime >= requestInterval) {
    previousRequestTime = currentMillis;
    getResponse();
  }

  // Built-in display
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(75);
  matrix.textFont(Font_5x7_Big_Zero);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println("  " + matrixText);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

/* -------------------------------------------------------------------------- */
void getResponse() {
/* -------------------------------------------------------------------------- */
  Serial.print("Getting... ");
  digitalWrite(LED_BUILTIN, HIGH);

  int getCode = client.get("/");
  int statusCode = client.responseStatusCode();
  String body = client.responseBody();

  digitalWrite(LED_BUILTIN, LOW);

  if (getCode != 0) {
    // Read error
    matrixText = "Fetch Err: " + String(getCode);
    Serial.println(matrixText);
    return;
  } else if (statusCode != 200) {
    // Other
    matrixText = "Server Err: " + String(statusCode);

    Serial.println(matrixText);
    return;
  }

  // Only get first line, the current value in mg/dL or No Data

  body.trim();
  const int newlineIndex = body.indexOf('\n');
  if (newlineIndex == -1) {
    matrixText = body;
    // nixieClock.setNumber(0, 0b1000);
  } else {
    matrixText = body.substring(0, newlineIndex);
    // nixieClock.setNumber(body.substring(0, newlineIndex).toInt(), 0b1000);
  }
  Serial.println("Received " + matrixText);
  // nixieClock.display();
}

/* -------------------------------------------------------------------------- */
void waitConnectWifi() {
/* -------------------------------------------------------------------------- */
  while (status != WL_CONNECTED) {
    matrix.loadFrame(Icon::no_wifi);
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);

    // Attempt to connect to WiFi network:
    status = WiFi.begin(ssid, pass);

    // Display bouncing wifi animation
    for (int i = 0; i < 5; i++) {
      matrix.loadFrame(Icon::wifi);
      delay(500);
      matrix.loadFrame(Icon::wifi1);
      delay(500);
      matrix.loadFrame(Icon::wifi2);
      delay(500);
      matrix.loadFrame(Icon::wifi3);
      delay(500);
    }
  }

  // Connected, good to go!
  matrix.loadFrame(Icon::wifi_good);

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
  for (byte n = 0; n < num; n++) {
    for (byte i = 0; i < 10; i++) {
      // nixieClock.setNumber(i * 1111, 0b1000);
      // nixieClock.display();

      matrix.beginDraw();
      matrix.stroke(0xFFFFFFFF);
      matrix.textFont(Font_5x7_Big_Zero);
      matrix.beginText(0, 1, 0xFFFFFF);
      matrix.println(String(i * 111) + "  ");
      matrix.endText();
      matrix.endDraw();

      delay(msDelay);
    }
  }
  delay(1000);
}