#include "ArduinoGraphics.h"
#include "Arduino_LED_Matrix.h"
#include "WiFiS3.h"
#include "WiFiSSLClient.h"

#include "arduino_secrets.h"
#include "icons.h"
#include "parse_http.h"

/* -------------------------------------------------------------------------- */

char ssid[] = SECRET_SSID;  // your network SSID (name)
char pass[] = SECRET_PASS;  // your network password (use for WPA, or use as key for WEP)
int keyIndex = 0;           // your network key index number (needed only for WEP)
int status = WL_IDLE_STATUS;

// if you don't want to use DNS (and reduce your sketch size)
// use the numeric IP instead of the name for the server:
//IPAddress server(74,125,232,128);  // numeric IP for Google (no DNS)
char server[] = "example.com";
char request[] = "GET / HTTP/1.1";

// Initialize the Ethernet client library
// with the IP address and port of the server
// that you want to connect to (port 80 is default for HTTP):
WiFiSSLClient client;

// Prepare matrix output
ArduinoLEDMatrix matrix;
String matrixText = "";

// Server refetch
bool connected = false;
const unsigned long interval = 60000;
const unsigned long timeout =   3000;
unsigned long previousRequestTime = -1;

/* -------------------------------------------------------------------------- */
void setup() {
/* -------------------------------------------------------------------------- */
  Serial.begin(115200);
  matrix.begin();
  pinMode(LED_BUILTIN, OUTPUT);
  
  Serial.println("\n\nHello at 115200 baud!");

  // Check for the WiFi module:
  if (WiFi.status() == WL_NO_MODULE) {
    matrix.loadFrame(Icon::no_wifi);
    Serial.println("Communication with WiFi module failed!");
    while (true); // Don't continue
  }

  String fv = WiFi.firmwareVersion();
  if (fv < WIFI_FIRMWARE_LATEST_VERSION) {
    Serial.println("Please upgrade the Wifi firmware");
  }
}

/* -------------------------------------------------------------------------- */
void loop() {
/* -------------------------------------------------------------------------- */
  // Recheck glucose
  unsigned long currentMillis = millis();
  if (previousRequestTime == -1 || currentMillis - previousRequestTime >= interval) {
    previousRequestTime = currentMillis;
    makeRequest();
  }
  
  // Currently talking to server
  if (connected) {

    String body;
    if (readHttpBody(client, body)) {
      body.trim(); // Remove leading/trailing whitespace/newlines

      // Only store first line, the current value in mg/dL
      const int newlineIndex = body.indexOf('\n');
      if (newlineIndex != -1) {
        matrixText = body.substring(0, newlineIndex);
        Serial.print("Received ");
        Serial.println(matrixText);
      } else {
        matrixText = "Problem response";
        Serial.println("Problem response");
      }
    } else if (millis() > previousRequestTime + timeout) {
      matrixText = "No response";
      Serial.println("No response");
      connected = false;
      client.stop();
    }

    // If the server's disconnected, stop the client:
    if (!client.connected()) {
      connected = false;
      client.stop();
    }
  }

  // Print what's on serial, for funsies
  if (Serial.available()) {
    String line = Serial.readStringUntil('\n'); // reads until newline or timeout
    line.trim(); // removes \r and whitespace ends
    matrixText = line;
  }

  // Display
  matrix.beginDraw();
  matrix.stroke(0xFFFFFFFF);
  matrix.textScrollSpeed(75);
  matrix.textFont(Font_5x7);
  matrix.beginText(0, 1, 0xFFFFFF);
  matrix.println("  " + matrixText);
  matrix.endText(SCROLL_LEFT);
  matrix.endDraw();
}

/* -------------------------------------------------------------------------- */
void makeRequest() {
/* -------------------------------------------------------------------------- */
  // Make sure wifi is connected
  int current_status = WiFi.status();
  if (current_status != WL_CONNECTED) {
    status = current_status;
    waitConnectWifi();
  }

  if (client.connect(server, 443)) {
    connected = true;
    Serial.println("Making GET request...");

    // Make a HTTP request:
    client.println(request);
    client.print("Host: ");
    client.println(server);
    client.println("Connection: close");
    client.println();
  } else {
    matrixText = "Can't connect";
    Serial.println("Can't connect to server...");
  }
}

/* -------------------------------------------------------------------------- */
void waitConnectWifi() {
/* -------------------------------------------------------------------------- */
  while (status != WL_CONNECTED) {
    // Attempt to connect to WiFi network:
    Serial.print("Attempting to connect to SSID: ");
    Serial.println(ssid);

    matrix.loadFrame(Icon::no_wifi);

    // Connect to WPA/WPA2 network. Change this line if using open or WEP network:
    status = WiFi.begin(ssid, pass);

    // Display bouncing wifi animation, for 10 seconds
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
  printWifiStatus();
}

/* -------------------------------------------------------------------------- */
void printWifiStatus() {
/* -------------------------------------------------------------------------- */
  // print the SSID of the network you're attached to:
  Serial.print("\nSSID: ");
  Serial.println(WiFi.SSID());

  // print your board's IP address:
  IPAddress ip = WiFi.localIP();
  Serial.print("IP Address: ");
  Serial.println(ip);

  // print the received signal strength:
  long rssi = WiFi.RSSI();
  Serial.print("signal strength (RSSI):");
  Serial.print(rssi);
  Serial.println(" dBm\n");
}