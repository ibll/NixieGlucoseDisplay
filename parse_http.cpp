#include "parse_http.h"

// Read a line (up to and including '\n') with timeout (ms)
/* -------------------------------------------------------------------------- */
String readLineWithTimeout(Client &client, unsigned long timeout) {
/* -------------------------------------------------------------------------- */
  String line = "";
  unsigned long start = millis();
  while (millis() - start < timeout) {
    while (client.available()) {
      char c = (char)client.read();
      line += c;
      if (c == '\n')
        return line;
    }
    delay(1);
  }
  
  return line; // may be partial or empty on timeout
}

// Read HTTP response body; returns true on success and fills 'body'
/* -------------------------------------------------------------------------- */
bool readHttpBody(Client &client, String &body, unsigned long timeout) {
/* -------------------------------------------------------------------------- */
  body = "";
  int contentLength = -1;

  // Read status line (discard or inspect if needed)
  String line = readLineWithTimeout(client, timeout);
  if (line.length() == 0)
    return false; // nothing received

  // Read headers
  unsigned long headerStart = millis();
  while (millis() - headerStart < timeout) {
    line = readLineWithTimeout(client, timeout);
    if (line.length() == 0)
      break; // timeout or no more data
    // blank line (CRLF) marks end of headers; line may be "\r\n" or "\n"
    if (line == "\r\n" || line == "\n")
      break;

    // Lowercase copy for case-insensitive header search
    String lower = line;
    lower.toLowerCase();

    // Get length of content
    if (lower.startsWith("content-length:")) {
      // extract number after colon
      int colonPos = line.indexOf(':');
      if (colonPos >= 0) {
        String val = line.substring(colonPos + 1);
        val.trim();
        contentLength = val.toInt();
      }
    }
  }

  // Read body
  unsigned long start = millis();
  if (contentLength >= 0) {
    body.reserve(contentLength + 1);
    int readSoFar = 0;
    while (readSoFar < contentLength && millis() - start < timeout) {
      while (client.available() && readSoFar < contentLength) {
        char c = (char)client.read();
        body += c;
        readSoFar++;
      }
      if (readSoFar >= contentLength)
        break;
      delay(1);
    }
    return (readSoFar == contentLength);
  } else {
    // No Content-Length: read until connection closed or timeout
    while ((client.connected() || client.available()) &&
           millis() - start < timeout) {
      while (client.available()) {
        body += (char)client.read();
      }
      delay(1);
    }
    return (body.length() > 0);
  }
}
